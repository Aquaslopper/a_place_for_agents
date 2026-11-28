// map.hpp — deterministic, bounded maps and explicit world connections.
//
// This module is deliberately independent of raylib and game.hpp: generation
// must be usable by rendering, collision, save/load and a headless verifier
// without any global state.  The game-facing adapter can translate MapTile and
// MapProp into the existing art/runtime types without creating a second
// generator.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace apfa {

enum class MapKind : std::uint8_t {
    AtlasCell = 0,  // one bounded cell of the expandable world atlas
    Route,          // a two-ended road linking neighbouring atlas cells
    Field,          // a natural clearing/site map
    Town,           // a bounded settlement with building portals
    Interior,       // a generated building interior
    Dungeon         // one generated floor; instance is the floor number
};

enum class MapTerrain : std::uint8_t {
    Grass = 0,
    Forest,
    Sand,
    Snow,
    Water,
    Mountain,
    Road,
    Bridge,
    Cobble,
    WoodFloor,
    StoneFloor,
    Wall,
    Void
};

enum class MapPropKind : std::uint8_t {
    Site = 0,
    Tree,
    Rock,
    Well,
    Noticeboard,
    Building,
    Npc,
    Chest,
    Stair,
    Guardian,
    Furniture,
    Flower
};

enum class MapSiteKind : std::uint8_t {
    None = 0,
    Town,
    Field,
    Dungeon
};

enum class PortalKind : std::uint8_t {
    SiteEntry = 0,
    TownGate,
    BuildingDoor,
    DungeonEntrance,
    StairUp,
    StairDown
};

// `seed` is the world seed, not a visitation-order seed. `instance` separates
// floors and interiors without making them dependent on the order in which
// they are generated (Route uses 0 = east/west, 1 = north/south).
struct MapId {
    MapKind kind = MapKind::AtlasCell;
    std::int32_t wcellX = 0;
    std::int32_t wcellY = 0;
    std::uint32_t seed = 0;
    std::uint16_t instance = 0;
};

bool operator==(const MapId& a, const MapId& b);
bool operator!=(const MapId& a, const MapId& b);
bool operator<(const MapId& a, const MapId& b);

// Each MapPort occupies a cell on the map boundary. `fromIndex` is its index
// in this map's port vector; `toIndex` is the reciprocal port in `to`.
struct MapPort {
    MapId to;
    std::uint8_t toIndex = 0;
    std::uint8_t fromIndex = 0;
    std::int16_t tx = 0;
    std::int16_t ty = 0;
};

// Portals are in-map door/site/stair transitions. IDs are stable within a map
// and point to a reciprocal portal ID on the target map.
struct MapPortal {
    std::uint16_t portalId = 0;
    PortalKind kind = PortalKind::SiteEntry;
    std::int16_t tx = 0;
    std::int16_t ty = 0;
    MapId to;
    std::uint16_t targetPortalId = 0;
    bool locked = false;
};

struct MapTile {
    MapTerrain terrain = MapTerrain::Grass;
    std::uint8_t variant = 0;
};

// Coordinates are the top-left of the footprint. A required prop is an
// objective/NPC/treasure that must be reachable from the map's spawn point.
struct MapProp {
    MapPropKind kind = MapPropKind::Flower;
    std::uint32_t stableId = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint8_t w = 1;
    std::uint8_t h = 1;
    std::uint8_t variant = 0;
    bool blocks = false;
    bool requiredReachable = false;
};

struct MapDef {
    MapId id;
    std::int32_t w = 0;
    std::int32_t h = 0;
    MapSiteKind site = MapSiteKind::None;
    std::int16_t siteX = -1;
    std::int16_t siteY = -1;
    std::int16_t spawnX = 0;
    std::int16_t spawnY = 0;
    std::vector<MapTile> tiles;
    std::vector<MapProp> props;
    std::vector<MapPort> ports;
    std::vector<MapPortal> portals;
};

// Mutable progress is kept separate from pure generated geometry and keyed by
// MapId plus stable object IDs. This is the portable map-session payload that
// the game SaveBlob can embed during runtime integration.
struct MapSavedState {
    MapId id;
    std::vector<std::uint32_t> openedObjectIds;
    std::vector<std::uint32_t> defeatedObjectIds;
    std::vector<std::uint16_t> unlockedPortalIds;
};

struct MapSessionSave {
    MapId currentMap;
    std::int16_t tileX = 0;
    std::int16_t tileY = 0;
    std::vector<MapSavedState> maps;
};

// Stable, seed-only site roll for a world-atlas cell.
MapSiteKind mapSiteForCell(std::int32_t cellX, std::int32_t cellY, std::uint32_t seed);

// Pure generator: no globals, caches, filesystem, or visit-order inputs.
// Site maps (Town/Field/Dungeon) and interiors only exist at matching atlas
// sites; unsupported MapIds return an empty MapDef rather than a fake map.
MapDef mapGenerate(const MapId& id);

// Canonical little-endian representation used for determinism checks and later
// save/diagnostic tooling. It is independent of compiler struct padding.
std::vector<std::uint8_t> mapSerialize(const MapDef& map);
std::uint64_t mapFingerprint(const MapDef& map);

// Versioned, checksummed serialization for the active map, player tile and
// per-map persistent flags. It stores identities and progress, not generated
// geometry; load regenerates each MapDef from its MapId.
bool mapSessionSaveValidate(const MapSessionSave& save, std::string* reason = nullptr);
bool mapSessionSaveSerialize(const MapSessionSave& save, std::vector<std::uint8_t>& bytes,
                             std::string* reason = nullptr);
bool mapSessionSaveDeserialize(const std::vector<std::uint8_t>& bytes, MapSessionSave& save,
                               std::string* reason = nullptr);

bool mapContains(const MapDef& map, int x, int y);
bool mapCanStand(const MapDef& map, int x, int y);
bool mapValidate(const MapDef& map, std::string* reason = nullptr);
bool mapValidateReachability(const MapDef& map, std::string* reason = nullptr);

const MapPortal* mapFindPortal(const MapDef& map, std::uint16_t portalId);

// Immutable generated definitions may be cached; gameplay state (opened chests,
// defeated guardians, unlocked doors) belongs in a separate MapId-keyed store.
class MapStore {
public:
    const MapDef& get(const MapId& id);
    void clear();
    std::size_t size() const { return maps_.size(); }

private:
    std::map<MapId, MapDef> maps_;
};

const char* mapKindName(MapKind kind);
const char* mapTerrainName(MapTerrain terrain);

} // namespace apfa
