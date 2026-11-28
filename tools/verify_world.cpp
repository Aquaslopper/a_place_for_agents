// Headless acceptance verifier for src/map.cpp. This is a command-line world
// verifier, not a second generator or a test-framework dependency.
#include "map.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace apfa;

namespace {

bool fail(const std::string& check, const std::string& detail) {
    std::cerr << "FAIL " << check << ": " << detail << '\n';
    return false;
}

MapId id(MapKind kind, int x, int y, std::uint32_t seed, std::uint16_t instance = 0) {
    MapId result;
    result.kind = kind;
    result.wcellX = x;
    result.wcellY = y;
    result.seed = seed;
    result.instance = instance;
    return result;
}

int portSide(const MapDef& map, const MapPort& port) {
    if (port.ty == 0) return 0;             // north
    if (port.tx == map.w - 1) return 1;      // east
    if (port.ty == map.h - 1) return 2;      // south
    if (port.tx == 0) return 3;              // west
    return -1;
}

bool checkMap(const MapDef& map, const std::string& context) {
    std::string reason;
    if (!mapValidate(map, &reason)) return fail(context, reason);
    if (!mapValidateReachability(map, &reason)) return fail(context, reason);
    return true;
}

bool checkPortPair(const MapDef& from, std::size_t portIndex, MapStore& store) {
    const MapPort& port = from.ports[portIndex];
    const MapDef& to = store.get(port.to);
    if (port.toIndex >= to.ports.size()) {
        std::ostringstream message;
        message << mapKindName(from.id.kind) << " port " << portIndex
                << " targets missing port " << static_cast<unsigned>(port.toIndex)
                << " on " << mapKindName(to.id.kind);
        return fail("port reciprocity", message.str());
    }
    const MapPort& reciprocal = to.ports[port.toIndex];
    if (reciprocal.to != from.id || reciprocal.toIndex != port.fromIndex ||
        reciprocal.fromIndex != port.toIndex) {
        std::ostringstream message;
        message << "port " << portIndex << " on " << mapKindName(from.id.kind)
                << " (" << from.id.wcellX << ',' << from.id.wcellY << ") targets "
                << mapKindName(to.id.kind) << " (" << to.id.wcellX << ',' << to.id.wcellY
                << "), but its return is " << mapKindName(reciprocal.to.kind) << " ("
                << reciprocal.to.wcellX << ',' << reciprocal.to.wcellY << ") with port-index pair "
                << static_cast<unsigned>(reciprocal.fromIndex) << "->"
                << static_cast<unsigned>(reciprocal.toIndex) << " (expected "
                << static_cast<unsigned>(port.toIndex) << "->"
                << static_cast<unsigned>(port.fromIndex) << ')';
        return fail("port reciprocity", message.str());
    }
    const int fromSide = portSide(from, port);
    const int toSide = portSide(to, reciprocal);
    if (fromSide < 0 || toSide != (fromSide + 2) % 4) {
        std::ostringstream message;
        message << "ports " << portIndex << " and " << static_cast<unsigned>(port.toIndex)
                << " on " << mapKindName(from.id.kind) << " and " << mapKindName(to.id.kind)
                << " do not meet on opposite map boundaries";
        return fail("port geometry", message.str());
    }
    return true;
}

bool checkPortalPair(const MapDef& from, const MapPortal& portal, MapStore& store) {
    const MapDef& to = store.get(portal.to);
    const MapPortal* reciprocal = mapFindPortal(to, portal.targetPortalId);
    if (!reciprocal || reciprocal->to != from.id ||
        reciprocal->targetPortalId != portal.portalId) {
        std::ostringstream message;
        message << mapKindName(from.id.kind) << " portal " << portal.portalId
                << " has no reciprocal " << portal.targetPortalId << " on "
                << mapKindName(to.id.kind) << " at cell (" << to.id.wcellX << ','
                << to.id.wcellY << ')';
        return fail("portal reciprocity", message.str());
    }
    return true;
}

std::vector<MapId> deterministicSamples(std::uint32_t seed) {
    std::vector<MapId> ids;
    int fieldX = -1, fieldY = -1, dungeonX = -1, dungeonY = -1;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const MapSiteKind site = mapSiteForCell(x, y, seed);
            if (site == MapSiteKind::Field) { fieldX = x; fieldY = y; }
            if (site == MapSiteKind::Dungeon) { dungeonX = x; dungeonY = y; }
        }
    }
    if (fieldX < 0 || dungeonX < 0) return {};

    ids.push_back(id(MapKind::AtlasCell, 0, 0, seed));
    ids.push_back(id(MapKind::Town, 0, 0, seed));
    ids.push_back(id(MapKind::Field, fieldX, fieldY, seed));
    ids.push_back(id(MapKind::Dungeon, dungeonX, dungeonY, seed));
    ids.push_back(id(MapKind::Route, 0, 0, seed, 0));
    ids.push_back(id(MapKind::Route, 0, 0, seed, 1));
    for (std::uint16_t floor = 1; floor < 4; ++floor)
        ids.push_back(id(MapKind::Dungeon, dungeonX, dungeonY, seed, floor));
    for (std::uint16_t building = 0; building < 5; ++building)
        ids.push_back(id(MapKind::Interior, 0, 0, seed, building));
    return ids;
}

bool verifyDeterminism(int seeds) {
    std::size_t mapsChecked = 0;
    for (int s = 0; s < seeds; ++s) {
        const std::uint32_t seed = 0x51a70000u + static_cast<std::uint32_t>(s * 7919 + 1);
        for (const MapId& sample : deterministicSamples(seed)) {
            const MapDef a = mapGenerate(sample);
            const MapDef b = mapGenerate(sample);
            if (mapSerialize(a) != mapSerialize(b)) {
                std::ostringstream message;
                message << mapKindName(sample.kind) << " at (" << sample.wcellX << ','
                        << sample.wcellY << "), instance " << sample.instance
                        << " changed bytes when regenerated";
                return fail("determinism", message.str());
            }
            if (!checkMap(a, "generated " + std::string(mapKindName(sample.kind)))) return false;
            ++mapsChecked;
        }
    }
    std::cout << "PASS determinism and local-map invariants (" << mapsChecked
              << " maps, " << seeds << " deterministic seeds)\n";
    return true;
}

bool verifyGrid(int radius, std::uint32_t seed) {
    MapStore store;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const MapDef& atlas = store.get(id(MapKind::AtlasCell, x, y, seed));
            if (!checkMap(atlas, "atlas cell")) return false;
            if (atlas.ports.size() != 4) return fail("atlas ports", "atlas cell must have N/E/S/W ports");
            for (std::size_t i = 0; i < atlas.ports.size(); ++i) {
                if (!checkPortPair(atlas, i, store)) return false;
                const MapDef& route = store.get(atlas.ports[i].to);
                if (!checkMap(route, "route link")) return false;
                for (std::size_t j = 0; j < route.ports.size(); ++j)
                    if (!checkPortPair(route, j, store)) return false;
            }

            for (const MapPortal& portal : atlas.portals) {
                if (!checkPortalPair(atlas, portal, store)) return false;
                const MapDef& local = store.get(portal.to);
                if (!checkMap(local, "site map")) return false;
                for (const MapPortal& localPortal : local.portals) {
                    if (!checkPortalPair(local, localPortal, store)) return false;
                    const MapDef& interiorOrParent = store.get(localPortal.to);
                    if (!checkMap(interiorOrParent, "portal target")) return false;
                    for (const MapPortal& nested : interiorOrParent.portals)
                        if (!checkPortalPair(interiorOrParent, nested, store)) return false;
                }
            }
        }
    }
    std::cout << "PASS reciprocal edge ports and doors (atlas radius " << radius << ")\n";
    return true;
}

bool traversePortal(MapStore& store, const MapId& fromId, std::uint16_t portalId,
                    MapId& arrived) {
    const MapDef& from = store.get(fromId);
    const MapPortal* portal = mapFindPortal(from, portalId);
    if (!portal) return fail("cross-map traversal", "scripted portal is missing");
    const MapDef& target = store.get(portal->to);
    const MapPortal* reciprocal = mapFindPortal(target, portal->targetPortalId);
    if (!reciprocal || reciprocal->to != fromId || reciprocal->targetPortalId != portalId)
        return fail("cross-map traversal", "transition does not land at a matching return portal");
    arrived = portal->to;
    return true;
}

bool crossPort(MapStore& store, const MapId& fromId, std::uint8_t portIndex,
               MapId& arrived, std::uint8_t& arrivedPort) {
    const MapDef& from = store.get(fromId);
    if (portIndex >= from.ports.size()) return fail("cross-map traversal", "scripted edge port is missing");
    const MapPort& port = from.ports[portIndex];
    const MapDef& target = store.get(port.to);
    if (port.toIndex >= target.ports.size()) return fail("cross-map traversal", "target edge port is missing");
    const MapPort& reciprocal = target.ports[port.toIndex];
    if (reciprocal.to != fromId || reciprocal.toIndex != portIndex)
        return fail("cross-map traversal", "edge does not return to the previous map");
    arrived = port.to;
    arrivedPort = port.toIndex;
    return true;
}

bool verifyTraversal(std::uint32_t seed) {
    MapStore store;
    MapId current = id(MapKind::AtlasCell, 0, 0, seed);
    const MapDef& home = store.get(current);
    if (home.site != MapSiteKind::Town) return fail("cross-map traversal", "start cell has no town");

    // Atlas -> generated town -> a real generated interior -> town -> atlas.
    MapId town;
    if (!traversePortal(store, current, 1, town)) return false;
    const MapDef& townMap = store.get(town);
    if (townMap.id.kind != MapKind::Town) return fail("cross-map traversal", "home site did not enter a town map");
    MapId interior;
    if (!traversePortal(store, town, 10, interior)) return false;
    if (interior.kind != MapKind::Interior) return fail("cross-map traversal", "building door did not enter an interior");
    MapId returned;
    if (!traversePortal(store, interior, 1, returned) || returned != town) return false;
    if (!traversePortal(store, town, 1, current) || current != id(MapKind::AtlasCell, 0, 0, seed)) return false;

    // Cross eight bounded east/west routes to the next guaranteed town.
    for (int x = 0; x < 8; ++x) {
        MapId route;
        std::uint8_t routePort = 0;
        if (!crossPort(store, current, 1, route, routePort)) return false;
        if (route.kind != MapKind::Route || route.instance != 0 || routePort != 0)
            return fail("cross-map traversal", "east atlas edge did not enter its east/west route");
        if (!crossPort(store, route, 1, current, routePort)) return false;
        if (current != id(MapKind::AtlasCell, x + 1, 0, seed))
            return fail("cross-map traversal", "route arrived in the wrong atlas cell");
    }

    const MapDef& frontier = store.get(current);
    if (frontier.site != MapSiteKind::Town) return fail("cross-map traversal", "second town lattice cell is missing");
    if (!traversePortal(store, current, 1, town)) return false;
    if (town.kind != MapKind::Town) return fail("cross-map traversal", "second town site did not enter a town map");
    if (!traversePortal(store, town, 13, interior)) return false; // generated temple interior
    if (interior.kind != MapKind::Interior || interior.instance != 3)
        return fail("cross-map traversal", "second town door did not reach its temple interior");
    if (!traversePortal(store, interior, 1, returned) || returned != town) return false;
    if (!traversePortal(store, town, 1, current) || current != id(MapKind::AtlasCell, 8, 0, seed)) return false;

    // Return by the same seam chain in the opposite direction.
    for (int x = 7; x >= 0; --x) {
        MapId route;
        std::uint8_t routePort = 0;
        if (!crossPort(store, current, 3, route, routePort)) return false;
        if (route.kind != MapKind::Route || route.instance != 0 || routePort != 1)
            return fail("cross-map traversal", "west atlas edge did not enter the reciprocal route end");
        if (!crossPort(store, route, 0, current, routePort)) return false;
        if (current != id(MapKind::AtlasCell, x, 0, seed))
            return fail("cross-map traversal", "westbound route arrived in the wrong atlas cell");
    }

    std::cout << "PASS scripted traversal town -> route -> atlas -> second town -> interior -> return (both directions)\n";
    return true;
}

bool verifySaveRoundTrip(std::uint32_t seed) {
    const MapId townId = id(MapKind::Town, 0, 0, seed);
    const MapDef town = mapGenerate(townId);
    const MapProp* chest = nullptr;
    for (const MapProp& prop : town.props) {
        if (prop.kind == MapPropKind::Chest) { chest = &prop; break; }
    }
    if (!chest) return fail("map save round-trip", "generated town has no persistent chest to save");

    MapSessionSave before;
    before.currentMap = townId;
    before.tileX = town.spawnX;
    before.tileY = town.spawnY;
    MapSavedState atlasState;
    atlasState.id = id(MapKind::AtlasCell, 0, 0, seed);
    MapSavedState townState;
    townState.id = townId;
    townState.openedObjectIds = {chest->stableId};
    townState.defeatedObjectIds = {0x51ab1eu};
    townState.unlockedPortalIds = {10, 13};
    before.maps = {townState, atlasState}; // serializer must be order-independent

    std::vector<std::uint8_t> bytes;
    std::string reason;
    if (!mapSessionSaveSerialize(before, bytes, &reason))
        return fail("map save round-trip", "could not serialize: " + reason);
    MapSessionSave after;
    if (!mapSessionSaveDeserialize(bytes, after, &reason))
        return fail("map save round-trip", "could not reload: " + reason);
    std::vector<std::uint8_t> bytesAgain;
    if (!mapSessionSaveSerialize(after, bytesAgain, &reason) || bytesAgain != bytes)
        return fail("map save round-trip", "canonical save bytes changed after reload");
    if (after.currentMap != townId || after.tileX != town.spawnX || after.tileY != town.spawnY)
        return fail("map save round-trip", "reloaded map identity or player tile changed");

    MapStore regenerated;
    const MapDef& resumed = regenerated.get(after.currentMap);
    if (resumed.id != townId || !mapCanStand(resumed, after.tileX, after.tileY))
        return fail("map save round-trip", "saved tile does not resume on the regenerated town map");
    bool restoredChest = false;
    for (const MapSavedState& state : after.maps) {
        if (state.id != townId) continue;
        restoredChest = state.openedObjectIds.size() == 1 &&
                        state.openedObjectIds[0] == chest->stableId &&
                        state.defeatedObjectIds == std::vector<std::uint32_t>{0x51ab1eu} &&
                        state.unlockedPortalIds == std::vector<std::uint16_t>{10, 13};
    }
    if (!restoredChest) return fail("map save round-trip", "per-map chest, guardian or door state changed");

    std::vector<std::uint8_t> corrupt = bytes;
    corrupt[corrupt.size() / 2] ^= 0x80u;
    MapSessionSave rejected;
    if (mapSessionSaveDeserialize(corrupt, rejected, nullptr))
        return fail("map save round-trip", "checksum did not reject a corrupted save");
    std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 1);
    if (mapSessionSaveDeserialize(truncated, rejected, nullptr))
        return fail("map save round-trip", "loader accepted a truncated save");

    std::cout << "PASS map-session save round-trip (generated town, player tile, and per-map state)\n";
    std::cout << "INFO map-session payload is not yet wired into the game's legacy SaveBlob\n";
    return true;
}

bool verifyOutward(int cells, std::uint32_t seed) {
    MapStore store;
    for (int x = 0; x <= cells; ++x) {
        const MapDef& atlas = store.get(id(MapKind::AtlasCell, x, 0, seed));
        if (!mapValidate(atlas)) return fail("outward atlas", "generated atlas cell failed structural validation");
        if (atlas.ports.size() != 4) return fail("outward atlas", "a generated cell lost an edge port");
        const MapPort& east = atlas.ports[1];
        const MapDef& route = store.get(east.to);
        if (!mapValidateReachability(route) || route.ports.size() != 2)
            return fail("outward atlas", "eastbound route failed generation or connectivity");
    }
    std::cout << "PASS outward atlas generation (" << (cells + 1)
              << " cells with valid routes, no fixed outer boundary)\n";
    return true;
}

bool verifyAll(int seeds, int radius, int outward) {
    if (seeds < 1 || radius < 1 || outward < 1) {
        std::cerr << "Usage: verify_world [--seeds N] [--radius N] [--outward N]\n";
        return false;
    }
    if (!verifyDeterminism(seeds)) return false;
    const std::uint32_t seed = 0xa17a5u;
    if (!verifyGrid(radius, seed)) return false;
    if (!verifyTraversal(seed)) return false;
    if (!verifySaveRoundTrip(seed)) return false;
    if (!verifyOutward(outward, seed)) return false;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    int seeds = 10;
    int radius = 3;
    int outward = 200;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto readValue = [&](int& target) -> bool {
            if (i + 1 >= argc) return false;
            char* end = nullptr;
            const long value = std::strtol(argv[++i], &end, 10);
            if (!end || *end || value < 1 || value > 100000) return false;
            target = static_cast<int>(value);
            return true;
        };
        if (arg == "--seeds") {
            if (!readValue(seeds)) { std::cerr << "Invalid --seeds value\n"; return 2; }
        } else if (arg == "--radius") {
            if (!readValue(radius)) { std::cerr << "Invalid --radius value\n"; return 2; }
        } else if (arg == "--outward") {
            if (!readValue(outward)) { std::cerr << "Invalid --outward value\n"; return 2; }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: verify_world [--seeds N] [--radius N] [--outward N]\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << '\n';
            return 2;
        }
    }
    return verifyAll(seeds, radius, outward) ? 0 : 1;
}
