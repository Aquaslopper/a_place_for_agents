// map.cpp — pure seeded generation for bounded maps and reciprocal links.
#include "map.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace apfa {
namespace {

constexpr std::uint16_t kAtlasSitePortal = 1;
constexpr std::uint16_t kTownGatePortal = 1;
constexpr std::uint16_t kDungeonEntrancePortal = 1;
constexpr std::uint16_t kTownBuildingPortalBase = 10;
constexpr std::uint16_t kDungeonDownPortal = 2;
constexpr std::uint16_t kDungeonUpPortal = 3;
constexpr std::uint16_t kDungeonFloorCount = 4;
constexpr int kAtlasW = 24;
constexpr int kAtlasH = 18;
constexpr int kTownW = 40;
constexpr int kTownH = 32;
constexpr int kFieldW = 44;
constexpr int kFieldH = 28;
constexpr int kDungeonW = 56;
constexpr int kDungeonH = 40;
constexpr int kInteriorW = 20;
constexpr int kInteriorH = 14;

std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

std::uint32_t coordHash(std::uint32_t seed, std::int32_t x, std::int32_t y,
                        std::uint32_t salt) {
    std::uint32_t h = seed ^ salt;
    h ^= static_cast<std::uint32_t>(x) * 0x9e3779b1u;
    h = mix32(h + 0x85ebca6bu);
    h ^= static_cast<std::uint32_t>(y) * 0xc2b2ae35u;
    return mix32(h);
}

std::uint32_t mapHash(const MapId& id, std::uint32_t salt) {
    std::uint32_t h = coordHash(id.seed, id.wcellX, id.wcellY,
                                salt ^ (static_cast<std::uint32_t>(id.kind) << 24));
    return mix32(h ^ (static_cast<std::uint32_t>(id.instance) * 0x27d4eb2du));
}

struct Rng {
    std::uint32_t state;
    explicit Rng(std::uint32_t seed) : state(seed ? seed : 0x6d2b79f5u) {}

    std::uint32_t next() {
        state += 0x9e3779b9u;
        return mix32(state);
    }
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<std::uint32_t>(hi - lo + 1));
    }
};

MapId makeId(MapKind kind, std::int32_t x, std::int32_t y,
             std::uint32_t seed, std::uint16_t instance = 0) {
    MapId id;
    id.kind = kind;
    id.wcellX = x;
    id.wcellY = y;
    id.seed = seed;
    id.instance = instance;
    return id;
}

MapSiteKind siteForCell(const MapId& id) {
    // Every 8x8 atlas block guarantees one town, one field and one dungeon.
    // Their positions vary with the block seed, so density is stable without
    // putting the same three icons in a rigid diagonal on every world.
    const std::int32_t blockX = id.wcellX >= 0 ? id.wcellX / 8 : (id.wcellX - 7) / 8;
    const std::int32_t blockY = id.wcellY >= 0 ? id.wcellY / 8 : (id.wcellY - 7) / 8;
    const std::int32_t localX = id.wcellX - blockX * 8;
    const std::int32_t localY = id.wcellY - blockY * 8;
    const std::uint32_t blockHash = coordHash(id.seed, blockX, blockY, 0x51a7e11u);
    const std::int32_t fieldX = 1 + static_cast<std::int32_t>(blockHash % 3u);
    const std::int32_t fieldY = 1 + static_cast<std::int32_t>((blockHash >> 5) % 3u);
    const std::int32_t dungeonX = 4 + static_cast<std::int32_t>((blockHash >> 10) % 3u);
    const std::int32_t dungeonY = 4 + static_cast<std::int32_t>((blockHash >> 15) % 3u);
    if (localX == 0 && localY == 0) return MapSiteKind::Town;
    if (localX == fieldX && localY == fieldY) return MapSiteKind::Field;
    if (localX == dungeonX && localY == dungeonY) return MapSiteKind::Dungeon;

    const std::uint32_t h = coordHash(id.seed, id.wcellX, id.wcellY, 0xa5b91u);
    if (h % 19u != 0u) return MapSiteKind::None;
    switch ((h >> 8) % 3u) {
        case 0: return MapSiteKind::Town;
        case 1: return MapSiteKind::Field;
        default: return MapSiteKind::Dungeon;
    }
}

MapKind localKindFor(MapSiteKind site) {
    switch (site) {
        case MapSiteKind::Town: return MapKind::Town;
        case MapSiteKind::Field: return MapKind::Field;
        case MapSiteKind::Dungeon: return MapKind::Dungeon;
        default: return MapKind::Field;
    }
}

bool isPassableTerrain(MapTerrain terrain) {
    switch (terrain) {
        case MapTerrain::Water:
        case MapTerrain::Mountain:
        case MapTerrain::Wall:
        case MapTerrain::Void:
            return false;
        default:
            return true;
    }
}

bool hasAnyPropOverlap(const MapDef& map, int x, int y) {
    for (const MapProp& p : map.props)
        if (x >= p.x && y >= p.y && x < p.x + p.w && y < p.y + p.h) return true;
    return false;
}

bool hasSolidOverlap(const MapDef& map, int x, int y) {
    for (const MapProp& p : map.props) {
        if (!p.blocks) continue;
        if (x >= p.x && y >= p.y && x < p.x + p.w && y < p.y + p.h) return true;
    }
    return false;
}

MapTile& tileAt(MapDef& map, int x, int y) {
    return map.tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(map.w) +
                     static_cast<std::size_t>(x)];
}

const MapTile& tileAt(const MapDef& map, int x, int y) {
    return map.tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(map.w) +
                     static_cast<std::size_t>(x)];
}

void resetMap(MapDef& map, MapId id, int w, int h, MapTerrain terrain) {
    map = MapDef{};
    map.id = id;
    map.w = w;
    map.h = h;
    map.spawnX = static_cast<std::int16_t>(w / 2);
    map.spawnY = static_cast<std::int16_t>(h / 2);
    map.tiles.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                     MapTile{terrain, 0});
}

void setTerrain(MapDef& map, int x, int y, MapTerrain terrain, std::uint8_t variant = 0) {
    if (!mapContains(map, x, y)) return;
    tileAt(map, x, y) = {terrain, variant};
}

void fillRect(MapDef& map, int x0, int y0, int x1, int y1,
              MapTerrain terrain, std::uint8_t variant = 0) {
    x0 = std::max(0, x0); y0 = std::max(0, y0);
    x1 = std::min(map.w - 1, x1); y1 = std::min(map.h - 1, y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            setTerrain(map, x, y, terrain, variant);
}

void addProp(MapDef& map, MapPropKind kind, int x, int y, int w = 1, int h = 1,
             std::uint32_t stableId = 0, std::uint8_t variant = 0,
             bool blocks = false, bool required = false) {
    if (x < 0 || y < 0 || x + w > map.w || y + h > map.h) return;
    MapProp p;
    p.kind = kind;
    p.stableId = stableId;
    p.x = static_cast<std::int16_t>(x);
    p.y = static_cast<std::int16_t>(y);
    p.w = static_cast<std::uint8_t>(w);
    p.h = static_cast<std::uint8_t>(h);
    p.variant = variant;
    p.blocks = blocks;
    p.requiredReachable = required;
    map.props.push_back(p);
}

void addPortal(MapDef& map, std::uint16_t id, PortalKind kind, int x, int y,
               const MapId& to, std::uint16_t targetId, bool locked = false) {
    MapPortal p;
    p.portalId = id;
    p.kind = kind;
    p.tx = static_cast<std::int16_t>(x);
    p.ty = static_cast<std::int16_t>(y);
    p.to = to;
    p.targetPortalId = targetId;
    p.locked = locked;
    map.portals.push_back(p);
}

void addPort(MapDef& map, const MapId& to, int fromIndex, int toIndex, int x, int y) {
    MapPort p;
    p.to = to;
    p.fromIndex = static_cast<std::uint8_t>(fromIndex);
    p.toIndex = static_cast<std::uint8_t>(toIndex);
    p.tx = static_cast<std::int16_t>(x);
    p.ty = static_cast<std::int16_t>(y);
    map.ports.push_back(p);
}

void carveCorridor(MapDef& map, int x0, int y0, int x1, int y1,
                   bool horizontalFirst, MapTerrain terrain, int halfWidth = 1) {
    auto carvePoint = [&](int x, int y) {
        for (int dy = -halfWidth; dy <= halfWidth; ++dy)
            for (int dx = -halfWidth; dx <= halfWidth; ++dx) {
                if (!mapContains(map, x + dx, y + dy)) continue;
                setTerrain(map, x + dx, y + dy, terrain);
            }
    };
    auto carveX = [&](int from, int to, int y) {
        int step = (to >= from) ? 1 : -1;
        for (int x = from; x != to + step; x += step) carvePoint(x, y);
    };
    auto carveY = [&](int x, int from, int to) {
        int step = (to >= from) ? 1 : -1;
        for (int y = from; y != to + step; y += step) carvePoint(x, y);
    };
    if (horizontalFirst) {
        carveX(x0, x1, y0);
        carveY(x1, y0, y1);
    } else {
        carveY(x0, y0, y1);
        carveX(x0, x1, y1);
    }
}

void carveRoom(MapDef& map, int cx, int cy, int rx, int ry, MapTerrain terrain,
               std::uint8_t variant) {
    fillRect(map, cx - rx, cy - ry, cx + rx, cy + ry, terrain, variant);
}

bool nearPath(const std::vector<int>& path, int along, int cross, int radius) {
    const int lo = std::max(0, along - radius);
    const int hi = std::min(static_cast<int>(path.size()) - 1, along + radius);
    for (int a = lo; a <= hi; ++a) {
        if (std::abs(path[static_cast<std::size_t>(a)] - cross) <= radius + 1) return true;
    }
    return false;
}

std::vector<int> makeWindingPath(int length, int crossSize, std::uint32_t seed) {
    std::vector<int> path(static_cast<std::size_t>(length), crossSize / 2);
    Rng rng(seed);
    int previous = crossSize / 2;
    for (int x = 1; x < length - 1; ++x) {
        if (x % 4 == 0) previous = std::max(3, std::min(crossSize - 4,
                                                       previous + rng.range(-2, 2)));
        path[static_cast<std::size_t>(x)] = previous;
    }
    path.front() = crossSize / 2;
    path.back() = crossSize / 2;
    return path;
}

void generateAtlasCell(MapDef& map) {
    resetMap(map, map.id, kAtlasW, kAtlasH, MapTerrain::Grass);
    map.site = siteForCell(map.id);
    map.spawnX = kAtlasW / 2;
    map.spawnY = kAtlasH / 2;

    for (int y = 0; y < map.h; ++y) {
        for (int x = 0; x < map.w; ++x) {
            const std::uint32_t h = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 31 + x / 4),
                static_cast<std::int32_t>(map.id.wcellY * 29 + y / 4), 0x71aa5u);
            const std::uint32_t roll = h % 100u;
            MapTerrain terrain = MapTerrain::Grass;
            if (roll < 7u) terrain = MapTerrain::Water;
            else if (roll < 13u) terrain = MapTerrain::Mountain;
            else if (roll < 40u) terrain = MapTerrain::Forest;
            else if (roll < 49u && map.id.wcellY > 0) terrain = MapTerrain::Sand;
            else if (roll < 56u && map.id.wcellY < 0) terrain = MapTerrain::Snow;
            setTerrain(map, x, y, terrain, static_cast<std::uint8_t>((h >> 12) & 3u));
        }
    }

    // The atlas road is the cell's backbone: every one of its four ports and
    // its deterministic site can be reached from the same central junction.
    const int cx = map.w / 2;
    const int cy = map.h / 2;
    for (int x = 0; x < map.w; ++x) setTerrain(map, x, cy, MapTerrain::Road);
    for (int y = 0; y < map.h; ++y) setTerrain(map, cx, y, MapTerrain::Road);
    fillRect(map, cx - 1, cy - 1, cx + 1, cy + 1, MapTerrain::Road);

    map.ports.reserve(4);
    const MapId northRoute = makeId(MapKind::Route, map.id.wcellX,
        static_cast<std::int32_t>(map.id.wcellY - 1), map.id.seed, 1);
    const MapId eastRoute = makeId(MapKind::Route, map.id.wcellX,
        map.id.wcellY, map.id.seed, 0);
    const MapId southRoute = makeId(MapKind::Route, map.id.wcellX,
        map.id.wcellY, map.id.seed, 1);
    const MapId westRoute = makeId(MapKind::Route,
        static_cast<std::int32_t>(map.id.wcellX - 1), map.id.wcellY, map.id.seed, 0);
    addPort(map, northRoute, 0, 1, cx, 0);
    addPort(map, eastRoute, 1, 0, map.w - 1, cy);
    addPort(map, southRoute, 2, 0, cx, map.h - 1);
    addPort(map, westRoute, 3, 1, 0, cy);

    if (map.site != MapSiteKind::None) {
        map.siteX = static_cast<std::int16_t>(cx);
        map.siteY = static_cast<std::int16_t>(cy);
        const MapId destination = makeId(localKindFor(map.site), map.id.wcellX,
                                          map.id.wcellY, map.id.seed);
        addProp(map, MapPropKind::Site, cx, cy, 1, 1,
                coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0x51a7e11u),
                static_cast<std::uint8_t>(map.site), false, true);
        addPortal(map, kAtlasSitePortal, PortalKind::SiteEntry, cx, cy,
                  destination, kTownGatePortal);
    }

    // Lightly dress the cell without ever filling the cross-road or a site.
    for (int y = 1; y < map.h - 1; ++y) {
        for (int x = 1; x < map.w - 1; ++x) {
            if (x == cx || y == cy) continue;
            const MapTile& t = tileAt(map, x, y);
            const std::uint32_t h = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 37 + x),
                static_cast<std::int32_t>(map.id.wcellY * 41 + y), 0x0b1ec7u);
            if (t.terrain == MapTerrain::Forest && h % 17u == 0u)
                addProp(map, MapPropKind::Tree, x, y, 1, 1, h, 0, true, false);
            else if (t.terrain == MapTerrain::Mountain && h % 9u == 0u)
                addProp(map, MapPropKind::Rock, x, y, 1, 1, h, 0, true, false);
        }
    }
}

void generateRoute(MapDef& map) {
    const bool horizontal = map.id.instance == 0;
    const int w = horizontal ? 32 : 18;
    const int h = horizontal ? 18 : 32;
    resetMap(map, map.id, w, h, MapTerrain::Grass);
    Rng rng(mapHash(map.id, 0x7017e5u));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t cell = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 31 + x / 5),
                static_cast<std::int32_t>(map.id.wcellY * 37 + y / 5),
                0x51f1e1du ^ map.id.instance);
            MapTerrain terrain = MapTerrain::Grass;
            if ((cell % 100u) < 26u) terrain = MapTerrain::Forest;
            else if ((cell % 100u) < 34u) terrain = MapTerrain::Sand;
            setTerrain(map, x, y, terrain, static_cast<std::uint8_t>((cell >> 10) & 3u));
        }
    }

    const int length = horizontal ? w : h;
    const int crossSize = horizontal ? h : w;
    const std::vector<int> path = makeWindingPath(length, crossSize, mapHash(map.id, 0x1a6e5u));
    const bool hasRiver = (mapHash(map.id, 0x71e2u) % 3u) == 0u;
    const int riverAt = length / 3 + static_cast<int>(mapHash(map.id, 0x22a1u) %
                                                       static_cast<std::uint32_t>(std::max(1, length / 3)));

    if (hasRiver) {
        if (horizontal) {
            for (int y = 0; y < h; ++y)
                for (int dx = 0; dx < 2; ++dx)
                    setTerrain(map, std::min(w - 2, riverAt + dx), y, MapTerrain::Water);
        } else {
            for (int dy = 0; dy < 2; ++dy)
                for (int x = 0; x < w; ++x)
                    setTerrain(map, x, std::min(h - 2, riverAt + dy), MapTerrain::Water);
        }
    }

    for (int along = 0; along < length; ++along) {
        const int cross = path[static_cast<std::size_t>(along)];
        for (int offset = -1; offset <= 1; ++offset) {
            const int across = cross + offset;
            const int x = horizontal ? along : across;
            const int y = horizontal ? across : along;
            const MapTile& old = tileAt(map, x, y);
            const MapTerrain road = old.terrain == MapTerrain::Water ? MapTerrain::Bridge : MapTerrain::Road;
            setTerrain(map, x, y, road);
        }
    }

    const int center = crossSize / 2;
    map.spawnX = static_cast<std::int16_t>(horizontal ? 0 : center);
    map.spawnY = static_cast<std::int16_t>(horizontal ? center : 0);
    const MapId firstCell = makeId(MapKind::AtlasCell, map.id.wcellX, map.id.wcellY, map.id.seed);
    const MapId secondCell = horizontal
        ? makeId(MapKind::AtlasCell, static_cast<std::int32_t>(map.id.wcellX + 1), map.id.wcellY, map.id.seed)
        : makeId(MapKind::AtlasCell, map.id.wcellX, static_cast<std::int32_t>(map.id.wcellY + 1), map.id.seed);
    if (horizontal) {
        addPort(map, firstCell, 0, 1, 0, center);
        addPort(map, secondCell, 1, 3, w - 1, center);
    } else {
        addPort(map, firstCell, 0, 2, center, 0);
        addPort(map, secondCell, 1, 0, center, h - 1);
    }

    // Seeded verge dressing stays outside a two-tile safety envelope around the
    // road, bridge and both boundary sockets.
    for (int along = 2; along < length - 2; ++along) {
        for (int across = 1; across < crossSize - 1; ++across) {
            if (nearPath(path, along, across, 2)) continue;
            const int x = horizontal ? along : across;
            const int y = horizontal ? across : along;
            const MapTile& t = tileAt(map, x, y);
            const std::uint32_t roll = rng.next() % 100u;
            if (t.terrain == MapTerrain::Forest && roll < 7u)
                addProp(map, MapPropKind::Tree, x, y, 1, 1, roll ^ (x * 31u + y), 0, true, false);
            else if (t.terrain == MapTerrain::Grass && roll < 2u)
                addProp(map, MapPropKind::Rock, x, y, 1, 1, roll ^ (x * 71u + y), 0, true, false);
        }
    }
}

struct BuildingSpec {
    const char* name;
    int x, y, w, h;
    int doorX, doorY;
    std::uint8_t kind;
};

constexpr std::array<BuildingSpec, 5> kBuildings = {{
    {"shop",   3,  3, 7, 5,  6,  8, 0},
    {"inn",   30,  3, 7, 5, 33,  8, 1},
    {"manor", 16,  2, 9, 5, 20,  7, 2},
    {"temple", 3, 21, 9, 5,  7, 26, 3},
    {"smithy",28, 21, 9, 5, 32, 26, 4}
}};

void generateTown(MapDef& map) {
    resetMap(map, map.id, kTownW, kTownH, MapTerrain::Grass);
    map.spawnX = kTownW / 2;
    map.spawnY = kTownH - 2;
    std::array<BuildingSpec, 5> buildings = kBuildings;
    const std::uint32_t layout = mapHash(map.id, 0x70a11u);
    const int streetY = 14 + static_cast<int>(layout % 3u);
    const int shopDoorX = 5 + static_cast<int>((layout >> 4) % 4u);
    const int innDoorX = 32 + static_cast<int>((layout >> 7) % 4u);
    const int templeDoorX = 6 + static_cast<int>((layout >> 10) % 4u);
    const int smithyDoorX = 31 + static_cast<int>((layout >> 13) % 4u);
    const int shopTop = 2 + static_cast<int>((layout >> 16) & 1u);
    const int innTop = 2 + static_cast<int>((layout >> 17) & 1u);
    const int templeTop = 19 + static_cast<int>((layout >> 18) % 3u);
    const int smithyTop = 19 + static_cast<int>((layout >> 20) % 3u);
    buildings[0] = {"shop", shopDoorX - 3, shopTop, 7, 5, shopDoorX, shopTop + 5, 0};
    buildings[1] = {"inn", innDoorX - 3, innTop, 7, 5, innDoorX, innTop + 5, 1};
    buildings[2] = {"manor", 16, 2, 9, 5, 20, 7, 2};
    buildings[3] = {"temple", templeDoorX - 4, templeTop, 9, 5,
                    templeDoorX, templeTop + 5, 3};
    buildings[4] = {"smithy", smithyDoorX - 4, smithyTop, 9, 5,
                    smithyDoorX, smithyTop + 5, 4};

    for (int x = 0; x < map.w; ++x) {
        setTerrain(map, x, 0, MapTerrain::Wall);
        setTerrain(map, x, map.h - 1, MapTerrain::Wall);
    }
    for (int y = 0; y < map.h; ++y) {
        setTerrain(map, 0, y, MapTerrain::Wall);
        setTerrain(map, map.w - 1, y, MapTerrain::Wall);
    }
    // South gate: the town itself is bounded, and this reciprocal doorway is
    // its only field exit.
    setTerrain(map, map.w / 2, map.h - 1, MapTerrain::Road);
    addPortal(map, kTownGatePortal, PortalKind::TownGate, map.w / 2, map.h - 1,
              makeId(MapKind::AtlasCell, map.id.wcellX, map.id.wcellY, map.id.seed),
              kAtlasSitePortal);

    // Crossroads and short civic lanes. The plazas and doors stay guaranteed,
    // while the road's position and each lot's placement change with the seed.
    for (int x = 1; x < map.w - 1; ++x) setTerrain(map, x, streetY, MapTerrain::Road);
    for (int y = 7; y < map.h - 1; ++y) setTerrain(map, 20, y, MapTerrain::Road);
    for (int i : {0, 1}) {
        const BuildingSpec& b = buildings[static_cast<std::size_t>(i)];
        for (int y = b.doorY; y <= streetY; ++y) setTerrain(map, b.doorX, y, MapTerrain::Road);
    }
    for (int i : {3, 4}) {
        const BuildingSpec& b = buildings[static_cast<std::size_t>(i)];
        for (int y = streetY; y <= b.doorY; ++y) setTerrain(map, b.doorX, y, MapTerrain::Road);
    }

    // The five civic lots are seeded, non-overlapping placements with clear
    // aprons. Their generated doors lead to distinct generated interiors.
    for (std::size_t i = 0; i < buildings.size(); ++i) {
        const BuildingSpec& b = buildings[i];
        fillRect(map, b.x, b.y, b.x + b.w - 1, b.y + b.h - 1, MapTerrain::Wall, b.kind);
        addProp(map, MapPropKind::Building, b.x, b.y, b.w, b.h,
                static_cast<std::uint32_t>(i + 1), b.kind, true, false);
        setTerrain(map, b.doorX, b.doorY, MapTerrain::Road);
        const MapId interior = makeId(MapKind::Interior, map.id.wcellX, map.id.wcellY,
                                      map.id.seed, static_cast<std::uint16_t>(i));
        const std::uint16_t portalId = static_cast<std::uint16_t>(kTownBuildingPortalBase + i);
        addPortal(map, portalId, PortalKind::BuildingDoor, b.doorX, b.doorY,
                  interior, 1);
    }

    // A small paved square around the central well keeps its approach open.
    fillRect(map, 18, streetY - 2, 22, streetY + 2, MapTerrain::Cobble);
    for (int y = streetY - 2; y <= streetY + 2; ++y) setTerrain(map, 20, y, MapTerrain::Cobble);
    for (int x = 18; x <= 22; ++x) setTerrain(map, x, streetY, MapTerrain::Cobble);
    addProp(map, MapPropKind::Well, 20, streetY - 1, 1, 1, 0x57454c4cu, 0, false, true);
    addProp(map, MapPropKind::Npc, 22, streetY - 1, 1, 1, 0x4d41594fu, 0, false, true);
    addProp(map, MapPropKind::Noticeboard, 21, streetY + 1, 1, 1, 0x424f4152u, 0, false, true);
    addProp(map, MapPropKind::Chest, 25, streetY, 1, 1,
            coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0x7ea5u), 0, false, true);

    // Seeded gardens and flowers make each town differ without introducing
    // collisions in its guaranteed road-and-door network.
    for (int y = 2; y < map.h - 2; ++y) {
        for (int x = 2; x < map.w - 2; ++x) {
            if (tileAt(map, x, y).terrain == MapTerrain::Road ||
                tileAt(map, x, y).terrain == MapTerrain::Cobble) continue;
            if (hasSolidOverlap(map, x, y)) continue;
            if (std::abs(x - map.spawnX) <= 1 && y >= map.spawnY - 2) continue;
            const std::uint32_t h = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 43 + x),
                static_cast<std::int32_t>(map.id.wcellY * 47 + y), 0x6a4deu);
            if (h % 83u == 0u)
                addProp(map, MapPropKind::Flower, x, y, 1, 1, h, 0, false, false);
        }
    }
}

void generateField(MapDef& map) {
    resetMap(map, map.id, kFieldW, kFieldH, MapTerrain::Forest);
    for (int y = 0; y < map.h; ++y) {
        for (int x = 0; x < map.w; ++x) {
            const std::uint32_t h = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 53 + x / 4),
                static_cast<std::int32_t>(map.id.wcellY * 59 + y / 4), 0xf1e1d5u);
            const MapTerrain t = (h % 100u < 24u) ? MapTerrain::Grass : MapTerrain::Forest;
            setTerrain(map, x, y, t, static_cast<std::uint8_t>((h >> 9) & 3u));
        }
    }

    for (int x = 0; x < map.w; ++x) {
        setTerrain(map, x, 0, MapTerrain::Mountain);
        setTerrain(map, x, map.h - 1, MapTerrain::Mountain);
    }
    for (int y = 0; y < map.h; ++y) {
        setTerrain(map, 0, y, MapTerrain::Mountain);
        setTerrain(map, map.w - 1, y, MapTerrain::Mountain);
    }

    const int entryX = map.w / 2;
    map.spawnX = static_cast<std::int16_t>(entryX);
    map.spawnY = static_cast<std::int16_t>(map.h - 2);
    setTerrain(map, entryX, map.h - 1, MapTerrain::Road);
    addPortal(map, kAtlasSitePortal, PortalKind::SiteEntry, entryX, map.h - 1,
              makeId(MapKind::AtlasCell, map.id.wcellX, map.id.wcellY, map.id.seed),
              kAtlasSitePortal);

    std::vector<int> path;
    path.reserve(map.h);
    const int targetX = map.w / 2 + 5;
    for (int y = 0; y < map.h; ++y) {
        const int drift = (y / 5) % 2 == 0 ? 0 : 1;
        path.push_back(std::min(map.w - 3, entryX + drift));
    }
    for (int y = map.h - 1; y >= 10; --y) {
        const int x = path[static_cast<std::size_t>(y)];
        for (int dx = -1; dx <= 1; ++dx)
            setTerrain(map, x + dx, y, MapTerrain::Road);
    }
    for (int x = entryX; x <= targetX; ++x)
        for (int dy = -1; dy <= 1; ++dy)
            setTerrain(map, x, 12 + dy, MapTerrain::Road);
    fillRect(map, targetX - 3, 9, targetX + 3, 15, MapTerrain::Grass);

    addProp(map, MapPropKind::Chest, targetX, 11, 1, 1,
            coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0xc4e57u), 0, false, true);
    addProp(map, MapPropKind::Npc, targetX - 2, 12, 1, 1,
            coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0x0b1ec7u), 0, false, true);

    for (int y = 2; y < map.h - 2; ++y) {
        for (int x = 2; x < map.w - 2; ++x) {
            if (y >= 9 && y <= 15 && x >= entryX - 2 && x <= targetX + 2) continue;
            if (y >= 10 && std::abs(x - entryX) <= 2) continue;
            const std::uint32_t h = coordHash(map.id.seed,
                static_cast<std::int32_t>(map.id.wcellX * 61 + x),
                static_cast<std::int32_t>(map.id.wcellY * 67 + y), 0x7eeu);
            if (tileAt(map, x, y).terrain == MapTerrain::Forest && h % 11u == 0u)
                addProp(map, MapPropKind::Tree, x, y, 1, 1, h, 0, true, false);
        }
    }
}

void generateTownInterior(MapDef& map) {
    if (map.id.instance >= kBuildings.size()) return;
    resetMap(map, map.id, kInteriorW, kInteriorH, MapTerrain::WoodFloor);
    for (int x = 0; x < map.w; ++x) {
        setTerrain(map, x, 0, MapTerrain::Wall);
        setTerrain(map, x, map.h - 1, MapTerrain::Wall);
    }
    for (int y = 0; y < map.h; ++y) {
        setTerrain(map, 0, y, MapTerrain::Wall);
        setTerrain(map, map.w - 1, y, MapTerrain::Wall);
    }
    const int exitX = map.w / 2;
    setTerrain(map, exitX, map.h - 1, MapTerrain::WoodFloor);
    map.spawnX = static_cast<std::int16_t>(exitX);
    map.spawnY = static_cast<std::int16_t>(map.h - 2);
    const std::uint16_t townPortal = static_cast<std::uint16_t>(kTownBuildingPortalBase + map.id.instance);
    addPortal(map, 1, PortalKind::BuildingDoor, exitX, map.h - 1,
              makeId(MapKind::Town, map.id.wcellX, map.id.wcellY, map.id.seed), townPortal);

    const std::uint8_t kind = static_cast<std::uint8_t>(map.id.instance);
    const MapTerrain floor = (kind == 3) ? MapTerrain::StoneFloor : MapTerrain::WoodFloor;
    fillRect(map, 1, 1, map.w - 2, map.h - 2, floor,
             static_cast<std::uint8_t>(mapHash(map.id, 0x1a7e2u) & 3u));
    // Restore the border after filling the room interior.
    for (int x = 0; x < map.w; ++x) {
        setTerrain(map, x, 0, MapTerrain::Wall);
        if (x != exitX) setTerrain(map, x, map.h - 1, MapTerrain::Wall);
    }
    for (int y = 0; y < map.h; ++y) {
        setTerrain(map, 0, y, MapTerrain::Wall);
        setTerrain(map, map.w - 1, y, MapTerrain::Wall);
    }

    if (kind == 0) { // shop
        addProp(map, MapPropKind::Furniture, 3, 3, 4, 1, 0x5348454cu, 0, true, false);
        addProp(map, MapPropKind::Furniture, 6, 6, 8, 1, 0x434f554eu, 0, true, false);
        addProp(map, MapPropKind::Npc, 10, 8, 1, 1, 0x53484f50u, 0, false, true);
        addProp(map, MapPropKind::Chest, 15, 10, 1, 1, 0x53484f43u, 0, false, true);
    } else if (kind == 1) { // inn
        addProp(map, MapPropKind::Furniture, 3, 3, 3, 2, 0x42454431u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 14, 3, 3, 2, 0x42454432u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 4, 8, 3, 1, 0x5441424cu, 0, true, false);
        addProp(map, MapPropKind::Npc, 10, 6, 1, 1, 0x494e4e4bu, 0, false, true);
    } else if (kind == 2) { // manor
        addProp(map, MapPropKind::Furniture, 4, 3, 3, 2, 0x42454431u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 13, 3, 3, 2, 0x42454432u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 6, 8, 8, 1, 0x4c4f4e47u, 0, true, false);
        addProp(map, MapPropKind::Npc, 10, 6, 1, 1, 0x4d414e4fu, 0, false, true);
    } else if (kind == 3) { // temple
        addProp(map, MapPropKind::Furniture, 8, 2, 4, 2, 0x414c5441u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 3, 5, 3, 1, 0x50455731u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 14, 5, 3, 1, 0x50455732u, 0, true, false);
        addProp(map, MapPropKind::Npc, 10, 7, 1, 1, 0x50524945u, 0, false, true);
    } else { // smithy
        addProp(map, MapPropKind::Furniture, 3, 3, 3, 2, 0x464f5247u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 14, 3, 2, 2, 0x414e5649u, 0, true, false);
        addProp(map, MapPropKind::Furniture, 6, 8, 8, 1, 0x434f554eu, 0, true, false);
        addProp(map, MapPropKind::Npc, 10, 6, 1, 1, 0x534d4954u, 0, false, true);
    }
}

void generateDungeon(MapDef& map) {
    if (map.id.instance >= kDungeonFloorCount) return;
    resetMap(map, map.id, kDungeonW, kDungeonH, MapTerrain::Wall);
    const std::uint32_t seed = mapHash(map.id, 0xd06e0u);
    Rng rng(seed);
    const MapTerrain floorTerrain = (map.id.instance % 2u == 0u)
        ? MapTerrain::StoneFloor : MapTerrain::Cobble;
    const std::uint8_t floorVariant = static_cast<std::uint8_t>(rng.next() & 3u);

    struct Room { int x, y, rx, ry; };
    const std::array<Room, 6> rooms = {{
        {28 + rng.range(-1, 1), 34 + rng.range(-1, 1), 5, 3},
        {12 + rng.range(-1, 1), 27 + rng.range(-1, 1), 5, 3},
        {44 + rng.range(-1, 1), 24 + rng.range(-1, 1), 5, 3},
        {12 + rng.range(-1, 1), 14 + rng.range(-1, 1), 5, 4},
        {44 + rng.range(-1, 1), 11 + rng.range(-1, 1), 5, 4},
        {28 + rng.range(-1, 1), 4 + rng.range(0, 1), 7, 3}
    }};

    for (const Room& room : rooms)
        carveRoom(map, room.x, room.y, room.rx, room.ry, floorTerrain, floorVariant);

    // A seeded, connected room backbone with two optional alcoves. The room
    // graph is fixed; only corridor bends and chamber offsets vary by MapId.
    for (std::size_t i = 1; i < rooms.size(); ++i) {
        const Room& a = rooms[i - 1];
        const Room& b = rooms[i];
        carveCorridor(map, a.x, a.y, b.x, b.y, (rng.next() & 1u) != 0u,
                      floorTerrain, 1);
    }
    const int branchX = 5 + rng.range(0, 2);
    const int branchY = 18 + rng.range(-1, 1);
    carveRoom(map, branchX, branchY, 3, 3, floorTerrain, floorVariant);
    carveCorridor(map, rooms[3].x, rooms[3].y, branchX, branchY,
                  (rng.next() & 1u) != 0u, floorTerrain, 1);

    const int entryX = rooms[0].x;
    const int entryY = rooms[0].y;
    const int stairX = rooms[5].x;
    const int stairY = rooms[5].y;
    map.spawnX = static_cast<std::int16_t>(entryX);
    map.spawnY = static_cast<std::int16_t>(entryY);

    if (map.id.instance == 0) {
        addPortal(map, kDungeonEntrancePortal, PortalKind::DungeonEntrance,
                  entryX, entryY,
                  makeId(MapKind::AtlasCell, map.id.wcellX, map.id.wcellY, map.id.seed),
                  kAtlasSitePortal);
    } else {
        addPortal(map, kDungeonUpPortal, PortalKind::StairUp, entryX, entryY,
                  makeId(MapKind::Dungeon, map.id.wcellX, map.id.wcellY,
                         map.id.seed, static_cast<std::uint16_t>(map.id.instance - 1)),
                  kDungeonDownPortal);
    }
    if (map.id.instance + 1u < kDungeonFloorCount) {
        addPortal(map, kDungeonDownPortal, PortalKind::StairDown, stairX, stairY,
                  makeId(MapKind::Dungeon, map.id.wcellX, map.id.wcellY,
                         map.id.seed, static_cast<std::uint16_t>(map.id.instance + 1)),
                  kDungeonUpPortal);
    } else {
        addProp(map, MapPropKind::Guardian, stairX, stairY, 1, 1,
                coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0xb055u),
                static_cast<std::uint8_t>(map.id.instance), false, true);
    }

    const int chestRoom = (rng.next() & 1u) ? 1 : 4;
    const Room& chest = rooms[static_cast<std::size_t>(chestRoom)];
    const int chestX = chest.x + (rng.next() & 1u ? 2 : -2);
    const int chestY = chest.y + 1;
    addProp(map, MapPropKind::Chest, chestX, chestY, 1, 1,
            coordHash(map.id.seed, map.id.wcellX, map.id.wcellY,
                      0xc4e57u + map.id.instance),
            static_cast<std::uint8_t>(map.id.instance), false, true);

    // The optional side alcove always has a reachable extra treasure on one
    // floor in four; its existence is a pure per-floor roll.
    if (mapHash(map.id, 0x0f710u) % 4u == 0u)
        addProp(map, MapPropKind::Chest, branchX, branchY, 1, 1,
                coordHash(map.id.seed, map.id.wcellX, map.id.wcellY, 0x51deu),
                1, false, true);

    // Decorative crystals and rubble are placed only within carved rooms and
    // never on any stair, chest, guardian, or spawn coordinate.
    for (std::size_t i = 1; i < rooms.size() - 1; ++i) {
        const Room& room = rooms[i];
        for (int y = room.y - room.ry + 1; y < room.y + room.ry; ++y) {
            for (int x = room.x - room.rx + 1; x < room.x + room.rx; ++x) {
                if (std::abs(x - entryX) + std::abs(y - entryY) < 3) continue;
                if (std::abs(x - stairX) + std::abs(y - stairY) < 3) continue;
                const std::uint32_t h = coordHash(map.id.seed,
                    static_cast<std::int32_t>(map.id.wcellX * 71 + x),
                    static_cast<std::int32_t>(map.id.wcellY * 73 + y),
                    0xdeca000u + map.id.instance);
                if (h % 29u == 0u && !hasAnyPropOverlap(map, x, y))
                    addProp(map, MapPropKind::Rock, x, y, 1, 1, h, 0, true, false);
            }
        }
    }
}

void appendU8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}
void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}
void appendI16(std::vector<std::uint8_t>& out, std::int16_t value) {
    appendU16(out, static_cast<std::uint16_t>(value));
}
void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu));
}
void appendI32(std::vector<std::uint8_t>& out, std::int32_t value) {
    appendU32(out, static_cast<std::uint32_t>(value));
}
void appendId(std::vector<std::uint8_t>& out, const MapId& id) {
    appendU8(out, static_cast<std::uint8_t>(id.kind));
    appendI32(out, id.wcellX);
    appendI32(out, id.wcellY);
    appendU32(out, id.seed);
    appendU16(out, id.instance);
}

bool validMapKind(std::uint8_t kind) {
    return kind <= static_cast<std::uint8_t>(MapKind::Dungeon);
}

std::uint32_t checksum32(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

class ByteReader {
public:
    ByteReader(const std::vector<std::uint8_t>& bytes, std::size_t limit)
        : bytes_(bytes), limit_(std::min(limit, bytes.size())) {}

    bool readU8(std::uint8_t& value) {
        if (offset_ >= limit_) return false;
        value = bytes_[offset_++];
        return true;
    }
    bool readU16(std::uint16_t& value) {
        std::uint8_t lo = 0, hi = 0;
        if (!readU8(lo) || !readU8(hi)) return false;
        value = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
        return true;
    }
    bool readI16(std::int16_t& value) {
        std::uint16_t raw = 0;
        if (!readU16(raw)) return false;
        value = static_cast<std::int16_t>(raw);
        return true;
    }
    bool readU32(std::uint32_t& value) {
        value = 0;
        for (int i = 0; i < 4; ++i) {
            std::uint8_t byte = 0;
            if (!readU8(byte)) return false;
            value |= static_cast<std::uint32_t>(byte) << (i * 8);
        }
        return true;
    }
    bool readI32(std::int32_t& value) {
        std::uint32_t raw = 0;
        if (!readU32(raw)) return false;
        value = static_cast<std::int32_t>(raw);
        return true;
    }
    bool readId(MapId& id) {
        std::uint8_t kind = 0;
        if (!readU8(kind) || !validMapKind(kind)) return false;
        id.kind = static_cast<MapKind>(kind);
        return readI32(id.wcellX) && readI32(id.wcellY) &&
               readU32(id.seed) && readU16(id.instance);
    }
    bool readU32Vector(std::vector<std::uint32_t>& values, std::size_t& total) {
        std::uint32_t count = 0;
        if (!readU32(count) || count > kMaxObjectIds ||
            total > kMaxObjectIds - static_cast<std::size_t>(count) ||
            static_cast<std::size_t>(count) > remaining() / 4u) return false;
        total += count;
        values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t value = 0;
            if (!readU32(value)) return false;
            values.push_back(value);
        }
        return true;
    }
    bool readU16Vector(std::vector<std::uint16_t>& values, std::size_t& total) {
        std::uint32_t count = 0;
        if (!readU32(count) || count > kMaxObjectIds ||
            total > kMaxObjectIds - static_cast<std::size_t>(count) ||
            static_cast<std::size_t>(count) > remaining() / 2u) return false;
        total += count;
        values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint16_t value = 0;
            if (!readU16(value)) return false;
            values.push_back(value);
        }
        return true;
    }
    std::size_t remaining() const { return limit_ - offset_; }
    bool done() const { return offset_ == limit_; }

private:
    static constexpr std::size_t kMaxObjectIds = 1u << 20;
    const std::vector<std::uint8_t>& bytes_;
    std::size_t limit_ = 0;
    std::size_t offset_ = 0;
};

} // namespace

bool operator==(const MapId& a, const MapId& b) {
    return a.kind == b.kind && a.wcellX == b.wcellX && a.wcellY == b.wcellY &&
           a.seed == b.seed && a.instance == b.instance;
}
bool operator!=(const MapId& a, const MapId& b) { return !(a == b); }
bool operator<(const MapId& a, const MapId& b) {
    return std::tie(a.kind, a.seed, a.wcellX, a.wcellY, a.instance) <
           std::tie(b.kind, b.seed, b.wcellX, b.wcellY, b.instance);
}

MapSiteKind mapSiteForCell(std::int32_t cellX, std::int32_t cellY, std::uint32_t seed) {
    return siteForCell(makeId(MapKind::AtlasCell, cellX, cellY, seed));
}

MapDef mapGenerate(const MapId& id) {
    MapDef map;
    map.id = id;
    switch (id.kind) {
        case MapKind::AtlasCell:
            if (id.instance == 0) generateAtlasCell(map);
            break;
        case MapKind::Route:
            if (id.instance <= 1) generateRoute(map);
            break;
        case MapKind::Field:
            if (id.instance == 0 && siteForCell(id) == MapSiteKind::Field) generateField(map);
            break;
        case MapKind::Town:
            if (id.instance == 0 && siteForCell(id) == MapSiteKind::Town) generateTown(map);
            break;
        case MapKind::Interior:
            if (id.instance < kBuildings.size() && siteForCell(id) == MapSiteKind::Town)
                generateTownInterior(map);
            break;
        case MapKind::Dungeon:
            if (id.instance < kDungeonFloorCount && siteForCell(id) == MapSiteKind::Dungeon)
                generateDungeon(map);
            break;
        default: break;
    }
    return map;
}

std::vector<std::uint8_t> mapSerialize(const MapDef& map) {
    std::vector<std::uint8_t> out;
    out.reserve(32 + map.tiles.size() * 2 + map.props.size() * 20 +
                map.ports.size() * 24 + map.portals.size() * 30);
    appendU8(out, 'A'); appendU8(out, 'P'); appendU8(out, 'M'); appendU8(out, 1);
    appendId(out, map.id);
    appendI32(out, map.w); appendI32(out, map.h);
    appendU8(out, static_cast<std::uint8_t>(map.site));
    appendI16(out, map.siteX); appendI16(out, map.siteY);
    appendI16(out, map.spawnX); appendI16(out, map.spawnY);
    appendU32(out, static_cast<std::uint32_t>(map.tiles.size()));
    for (const MapTile& t : map.tiles) {
        appendU8(out, static_cast<std::uint8_t>(t.terrain));
        appendU8(out, t.variant);
    }
    appendU32(out, static_cast<std::uint32_t>(map.props.size()));
    for (const MapProp& p : map.props) {
        appendU8(out, static_cast<std::uint8_t>(p.kind));
        appendU32(out, p.stableId);
        appendI16(out, p.x); appendI16(out, p.y);
        appendU8(out, p.w); appendU8(out, p.h); appendU8(out, p.variant);
        appendU8(out, p.blocks ? 1 : 0);
        appendU8(out, p.requiredReachable ? 1 : 0);
    }
    appendU32(out, static_cast<std::uint32_t>(map.ports.size()));
    for (const MapPort& p : map.ports) {
        appendId(out, p.to);
        appendU8(out, p.toIndex); appendU8(out, p.fromIndex);
        appendI16(out, p.tx); appendI16(out, p.ty);
    }
    appendU32(out, static_cast<std::uint32_t>(map.portals.size()));
    for (const MapPortal& p : map.portals) {
        appendU16(out, p.portalId);
        appendU8(out, static_cast<std::uint8_t>(p.kind));
        appendI16(out, p.tx); appendI16(out, p.ty);
        appendId(out, p.to);
        appendU16(out, p.targetPortalId);
        appendU8(out, p.locked ? 1 : 0);
    }
    return out;
}

std::uint64_t mapFingerprint(const MapDef& map) {
    const std::vector<std::uint8_t> bytes = mapSerialize(map);
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool mapSessionSaveValidate(const MapSessionSave& save, std::string* reason) {
    auto fail = [&](const std::string& message) {
        if (reason) *reason = message;
        return false;
    };
    constexpr std::size_t maxMaps = 4096;
    constexpr std::size_t maxObjectIds = 1u << 20;
    if (!validMapKind(static_cast<std::uint8_t>(save.currentMap.kind)))
        return fail("save has an invalid current map kind");
    if (save.maps.empty() || save.maps.size() > maxMaps)
        return fail("save map-state count is outside the supported range");

    std::set<MapId> mapIds;
    std::size_t totalObjectIds = 0;
    bool foundCurrent = false;
    auto uniqueValues = [](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;
        std::set<Value> unique(values.begin(), values.end());
        return unique.size() == values.size();
    };
    for (const MapSavedState& state : save.maps) {
        if (!validMapKind(static_cast<std::uint8_t>(state.id.kind)))
            return fail("save contains an invalid map-state kind");
        if (!mapIds.insert(state.id).second) return fail("save contains duplicate map-state identities");
        foundCurrent = foundCurrent || state.id == save.currentMap;
        const std::size_t opened = state.openedObjectIds.size();
        const std::size_t defeated = state.defeatedObjectIds.size();
        const std::size_t unlocked = state.unlockedPortalIds.size();
        if (opened > maxObjectIds || defeated > maxObjectIds || unlocked > maxObjectIds ||
            totalObjectIds > maxObjectIds - std::min(maxObjectIds, opened) ||
            totalObjectIds + opened > maxObjectIds - std::min(maxObjectIds, defeated) ||
            totalObjectIds + opened + defeated > maxObjectIds - std::min(maxObjectIds, unlocked))
            return fail("save contains too many per-map object states");
        totalObjectIds += opened + defeated + unlocked;
        if (!uniqueValues(state.openedObjectIds) || !uniqueValues(state.defeatedObjectIds) ||
            !uniqueValues(state.unlockedPortalIds))
            return fail("save contains duplicate object-state IDs");
    }
    if (!foundCurrent) return fail("current map has no corresponding persistent-state entry");

    const MapDef current = mapGenerate(save.currentMap);
    std::string mapReason;
    if (!mapValidate(current, &mapReason))
        return fail("current map cannot be regenerated: " + mapReason);
    if (!mapCanStand(current, save.tileX, save.tileY))
        return fail("saved player tile is outside the current map or blocked");
    if (reason) reason->clear();
    return true;
}

bool mapSessionSaveSerialize(const MapSessionSave& save, std::vector<std::uint8_t>& bytes,
                             std::string* reason) {
    bytes.clear();
    std::string validation;
    if (!mapSessionSaveValidate(save, &validation)) {
        if (reason) *reason = validation;
        return false;
    }

    constexpr std::size_t maxBytes = 16u * 1024u * 1024u;
    std::vector<const MapSavedState*> ordered;
    ordered.reserve(save.maps.size());
    for (const MapSavedState& state : save.maps) ordered.push_back(&state);
    std::sort(ordered.begin(), ordered.end(), [](const MapSavedState* a, const MapSavedState* b) {
        return a->id < b->id;
    });

    std::vector<std::uint8_t> encoded;
    encoded.reserve(40 + ordered.size() * 48);
    appendU8(encoded, 'A'); appendU8(encoded, 'P'); appendU8(encoded, 'S'); appendU8(encoded, 'V');
    appendU8(encoded, 1); // format version
    appendId(encoded, save.currentMap);
    appendI16(encoded, save.tileX);
    appendI16(encoded, save.tileY);
    appendU32(encoded, static_cast<std::uint32_t>(ordered.size()));
    auto appendCanonicalIds = [&](const auto& ids) {
        using Value = typename std::decay_t<decltype(ids)>::value_type;
        std::vector<Value> values(ids.begin(), ids.end());
        std::sort(values.begin(), values.end());
        appendU32(encoded, static_cast<std::uint32_t>(values.size()));
        for (Value value : values) {
            if constexpr (sizeof(Value) == sizeof(std::uint16_t))
                appendU16(encoded, static_cast<std::uint16_t>(value));
            else
                appendU32(encoded, static_cast<std::uint32_t>(value));
        }
    };
    for (const MapSavedState* state : ordered) {
        appendId(encoded, state->id);
        appendCanonicalIds(state->openedObjectIds);
        appendCanonicalIds(state->defeatedObjectIds);
        appendCanonicalIds(state->unlockedPortalIds);
        if (encoded.size() > maxBytes - 4u) {
            if (reason) *reason = "serialized map-session save exceeds the size limit";
            return false;
        }
    }
    appendU32(encoded, checksum32(encoded.data(), encoded.size()));
    if (encoded.size() > maxBytes) {
        if (reason) *reason = "serialized map-session save exceeds the size limit";
        return false;
    }
    bytes.swap(encoded);
    if (reason) reason->clear();
    return true;
}

bool mapSessionSaveDeserialize(const std::vector<std::uint8_t>& bytes, MapSessionSave& save,
                               std::string* reason) {
    auto fail = [&](const std::string& message) {
        if (reason) *reason = message;
        return false;
    };
    constexpr std::size_t maxBytes = 16u * 1024u * 1024u;
    constexpr std::uint32_t maxMaps = 4096;
    if (bytes.size() < 36u || bytes.size() > maxBytes)
        return fail("map-session save size is outside the supported range");
    const std::size_t payloadSize = bytes.size() - 4u;
    const std::uint32_t storedChecksum =
        static_cast<std::uint32_t>(bytes[payloadSize]) |
        (static_cast<std::uint32_t>(bytes[payloadSize + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[payloadSize + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[payloadSize + 3]) << 24);
    if (storedChecksum != checksum32(bytes.data(), payloadSize))
        return fail("map-session save checksum does not match");

    ByteReader reader(bytes, payloadSize);
    std::uint8_t a = 0, p = 0, s = 0, v = 0, version = 0;
    if (!reader.readU8(a) || !reader.readU8(p) || !reader.readU8(s) || !reader.readU8(v) ||
        !reader.readU8(version) || a != 'A' || p != 'P' || s != 'S' || v != 'V' || version != 1)
        return fail("map-session save has an unknown signature or version");

    MapSessionSave candidate;
    if (!reader.readId(candidate.currentMap) || !reader.readI16(candidate.tileX) ||
        !reader.readI16(candidate.tileY))
        return fail("map-session save has a truncated current-map record");
    std::uint32_t stateCount = 0;
    if (!reader.readU32(stateCount) || stateCount == 0 || stateCount > maxMaps ||
        static_cast<std::size_t>(stateCount) > reader.remaining() / 27u)
        return fail("map-session save has an invalid map-state count");
    candidate.maps.reserve(stateCount);
    std::size_t totalObjectIds = 0;
    for (std::uint32_t i = 0; i < stateCount; ++i) {
        MapSavedState state;
        if (!reader.readId(state.id) ||
            !reader.readU32Vector(state.openedObjectIds, totalObjectIds) ||
            !reader.readU32Vector(state.defeatedObjectIds, totalObjectIds) ||
            !reader.readU16Vector(state.unlockedPortalIds, totalObjectIds))
            return fail("map-session save contains a truncated or oversized map-state record");
        candidate.maps.push_back(std::move(state));
    }
    if (!reader.done()) return fail("map-session save has trailing payload bytes");
    std::string validation;
    if (!mapSessionSaveValidate(candidate, &validation)) return fail(validation);
    save = std::move(candidate);
    if (reason) reason->clear();
    return true;
}

bool mapContains(const MapDef& map, int x, int y) {
    return x >= 0 && y >= 0 && x < map.w && y < map.h;
}

bool mapCanStand(const MapDef& map, int x, int y) {
    if (!mapContains(map, x, y) || map.tiles.size() != static_cast<std::size_t>(map.w) * map.h)
        return false;
    return isPassableTerrain(tileAt(map, x, y).terrain) && !hasSolidOverlap(map, x, y);
}

bool mapValidate(const MapDef& map, std::string* reason) {
    auto fail = [&](const std::string& message) {
        if (reason) *reason = message;
        return false;
    };
    if (map.w <= 0 || map.h <= 0) return fail("map has non-positive dimensions");
    if (map.w > std::numeric_limits<std::int16_t>::max() ||
        map.h > std::numeric_limits<std::int16_t>::max()) return fail("map exceeds coordinate range");
    if (map.tiles.size() != static_cast<std::size_t>(map.w) * static_cast<std::size_t>(map.h))
        return fail("tile buffer does not match map dimensions");
    if (!mapCanStand(map, map.spawnX, map.spawnY)) return fail("spawn tile is blocked");

    for (std::size_t i = 0; i < map.ports.size(); ++i) {
        const MapPort& port = map.ports[i];
        if (port.fromIndex != i) return fail("port fromIndex does not match vector index");
        const bool onBoundary = port.tx == 0 || port.ty == 0 ||
                                port.tx == map.w - 1 || port.ty == map.h - 1;
        if (!onBoundary) return fail("edge port is not on the map boundary");
        if (!mapCanStand(map, port.tx, port.ty)) return fail("port tile is blocked or outside the map");
    }
    std::set<std::uint16_t> portalIds;
    for (const MapPortal& portal : map.portals) {
        if (portal.portalId == 0 || !portalIds.insert(portal.portalId).second)
            return fail("portal IDs must be unique and non-zero");
        if (!mapCanStand(map, portal.tx, portal.ty)) return fail("portal tile is blocked or outside the map");
        if (portal.targetPortalId == 0) return fail("portal has no reciprocal target ID");
    }
    for (const MapProp& prop : map.props) {
        if (prop.w == 0 || prop.h == 0 || prop.x < 0 || prop.y < 0 ||
            prop.x + prop.w > map.w || prop.y + prop.h > map.h)
            return fail("prop footprint lies outside the map");
    }
    if (reason) reason->clear();
    return true;
}

bool mapValidateReachability(const MapDef& map, std::string* reason) {
    std::string validation;
    if (!mapValidate(map, &validation)) {
        if (reason) *reason = validation;
        return false;
    }

    std::vector<std::uint8_t> seen(map.tiles.size(), 0);
    std::queue<std::pair<int, int>> open;
    open.push({map.spawnX, map.spawnY});
    seen[static_cast<std::size_t>(map.spawnY) * map.w + map.spawnX] = 1;
    const int dirs[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
    while (!open.empty()) {
        const auto p = open.front(); open.pop();
        for (const auto& d : dirs) {
            const int x = p.first + d[0], y = p.second + d[1];
            if (!mapCanStand(map, x, y)) continue;
            const std::size_t index = static_cast<std::size_t>(y) * map.w + x;
            if (seen[index]) continue;
            seen[index] = 1;
            open.push({x, y});
        }
    }

    auto reachable = [&](int x, int y) {
        return mapContains(map, x, y) && seen[static_cast<std::size_t>(y) * map.w + x] != 0;
    };
    for (const MapPort& port : map.ports) {
        if (!reachable(port.tx, port.ty)) {
            if (reason) *reason = "an edge port is unreachable from the spawn";
            return false;
        }
    }
    for (const MapPortal& portal : map.portals) {
        if (!reachable(portal.tx, portal.ty)) {
            if (reason) *reason = "a portal is unreachable from the spawn";
            return false;
        }
    }
    for (const MapProp& prop : map.props) {
        if (!prop.requiredReachable) continue;
        const int tx = prop.x + prop.w / 2;
        const int ty = prop.y + prop.h / 2;
        if (!reachable(tx, ty)) {
            if (reason) {
                std::ostringstream message;
                message << "required prop kind " << static_cast<unsigned>(prop.kind)
                        << " at (" << tx << ',' << ty << ") is unreachable";
                *reason = message.str();
            }
            return false;
        }
    }
    if (reason) reason->clear();
    return true;
}

const MapPortal* mapFindPortal(const MapDef& map, std::uint16_t portalId) {
    for (const MapPortal& portal : map.portals)
        if (portal.portalId == portalId) return &portal;
    return nullptr;
}

const MapDef& MapStore::get(const MapId& id) {
    auto found = maps_.find(id);
    if (found != maps_.end()) return found->second;
    return maps_.emplace(id, mapGenerate(id)).first->second;
}

void MapStore::clear() { maps_.clear(); }

const char* mapKindName(MapKind kind) {
    switch (kind) {
        case MapKind::AtlasCell: return "atlas-cell";
        case MapKind::Route: return "route";
        case MapKind::Field: return "field";
        case MapKind::Town: return "town";
        case MapKind::Interior: return "interior";
        case MapKind::Dungeon: return "dungeon";
        default: return "unknown";
    }
}

const char* mapTerrainName(MapTerrain terrain) {
    switch (terrain) {
        case MapTerrain::Grass: return "grass";
        case MapTerrain::Forest: return "forest";
        case MapTerrain::Sand: return "sand";
        case MapTerrain::Snow: return "snow";
        case MapTerrain::Water: return "water";
        case MapTerrain::Mountain: return "mountain";
        case MapTerrain::Road: return "road";
        case MapTerrain::Bridge: return "bridge";
        case MapTerrain::Cobble: return "cobble";
        case MapTerrain::WoodFloor: return "wood-floor";
        case MapTerrain::StoneFloor: return "stone-floor";
        case MapTerrain::Wall: return "wall";
        case MapTerrain::Void: return "void";
        default: return "unknown";
    }
}

} // namespace apfa
