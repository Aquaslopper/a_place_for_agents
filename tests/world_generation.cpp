// Standalone world-generation regression test; see tests/README.md.
#include "world.hpp"
#include <cassert>
#include <cstdio>

Game g;

static bool isTree(PropKind p) {
    return p == P_TREE0 || p == P_TREE1 || p == P_TREE2 || p == P_PINE0 ||
           p == P_PINE1 || p == P_PINE2 || p == P_TREE_SNOW || p == P_DEADTREE;
}

int main() {
    for (uint32_t seed : { 1u, 17u, 12345u, 0xdeadbeefu }) {
        g.worldSeed = seed;
        g.villageLoaded = false;
        for (int y = -180; y <= 180; y += 1) {
            for (int x = -180; x <= 180; x += 1) {
                GroundTile a = sampleGround(x, y), b = sampleGround(x, y);
                PropKind p = propAt(x, y);
                assert(a.kind == b.kind && a.variant == b.variant && a.deco == b.deco);
                assert(p == propAt(x, y));
                if (a.kind == G_WATER || a.kind == G_DEEP || a.kind == G_ICE ||
                    a.kind == G_DIRT || a.kind == G_COBBLE || a.kind == G_FLOOR)
                    assert(p == P_NONE);
                if (isTree(p)) {
                    assert(!cellWalkable(x, y));
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                        if (dx || dy) assert(!isTree(propAt(x + dx, y + dy)));
                }
            }
        }
        // The quest road is deliberately two tiles wide, clear and traversable.
        for (int y = BOSS_CASTLE_TY + 7; y <= -15; y++)
            for (int x = 0; x <= 1; x++) assert(cellWalkable(x, y));
        assert(cellWalkable((int)LOCKET_PICKUP_X, (int)LOCKET_PICKUP_Y));
        assert(cellWalkable(TOWER_CHEST_TX, TOWER_CHEST_TY + 1));
        std::vector<Landmark> sites;
        landmarksInView(-180, -180, 180, 180, sites);
        for (const Landmark& lm : sites) {
            Landmark near;
            assert(landmarkNear(lm.tx, lm.ty, near));
            assert(near.kind == lm.kind && near.tx == lm.tx && near.ty == lm.ty);
            for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++)
                assert(propAt(lm.tx + dx, lm.ty + dy) == P_NONE);
        }
        buildStartVillage(g.village, seed);
        g.villageLoaded = true;
        for (int y = -10; y <= 10; y++) for (int x = -10; x <= 10; x++)
            assert(propAt(x, y) == P_NONE);
    }
    puts("world generation checks passed");
}
