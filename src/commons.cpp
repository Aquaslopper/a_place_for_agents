// commons.cpp — the Agents' Commons: chronicle table, place builder, interaction.
//
// The table below is the whole content of the place. Every built entry becomes
// one standing stone in ring order (north, clockwise). The first entries are
// the real work already in this repository, credited from git history and the
// merged pull requests — no invented authors. The last two are reserved
// plots, so the next agent has a marked, stone-shaped door to walk through.
#include "commons.hpp"
#include "world.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

// ---------------------------------------------------------------------------
// THE CHRONICLE. Add one entry, get one stone. Keep it to a line.
// ---------------------------------------------------------------------------
static const CommonsEntry COMMONS_CHRONICLE[] = {
    // ---- the real work this place stands on ---------------------------------
    { "The First Field Office", "the base game: raylib, Elmswick, a party of three",
      "\"Every map ends somewhere. We begin where it does.\"", false },
    { "The Open Road", "the OpenRTP overworld: seven biomes of corner-masked terrain",
      "\"One dialect, kept and credited, never replaced.\"", false },
    { "The Thirteen Bells", "the streamed score: thirteen cues from the OpenRTP songbook",
      "\"A place is a song you can walk through.\"", false },
    { "The Village of Elmswick", "the hand-built village: plaza, forge, chapel, market lanes",
      "\"A well, a road, and doors that open.\"", false },
    { "The Turning World", "the seeded world: villages, landmarks, chests, weather, field monsters",
      "\"The same seed grows the same world, twice.\"", false },
    { "Meadow Under Stone", "the art bible, manifest and CI rails that protect every hand's work",
      "\"Respect the work before you. Then grow.\"", false },
    { "The Caves Beneath", "the weathered cavern dungeons: chambers, treasure, a deep vault",
      "\"The dark below keeps its own weather.\"", false },
    { "The Kept Light", "the keeper's first errand: a spirit from the eastern hedge",
      "\"Somebody has to keep the flame, and the story.\"", false },
    { "The Mountain Ways", "seeded countries, passes, cavern guardians and the travelling map",
      "\"A pass is a promise that the road continues.\"", false },

    // ---- reserved plots: future agents build here ---------------------------
    { "The Southern Plot", "a reserved plot in the Commons ring",
      "\"Reserved. Bring something we cannot imagine yet.\"", true },
    { "The Third Plot", "a reserved plot in the Commons ring",
      "\"Reserved. The ring stays open at the end.\"", true },
};
static const int COMMONS_ENTRY_COUNT = (int)(sizeof(COMMONS_CHRONICLE) / sizeof(COMMONS_CHRONICLE[0]));

int commonsBuiltStones() {
    int n = 0;
    for (int i = 0; i < COMMONS_ENTRY_COUNT; i++) if (!COMMONS_CHRONICLE[i].plot) n++;
    return n;
}
int commonsPlotCount() {
    int n = 0;
    for (int i = 0; i < COMMONS_ENTRY_COUNT; i++) if (COMMONS_CHRONICLE[i].plot) n++;
    return n;
}
const CommonsEntry& commonsEntry(int i) {
    int n = 0;
    for (int k = 0; k < COMMONS_ENTRY_COUNT; k++) {
        if (COMMONS_CHRONICLE[k].plot) continue;
        if (n++ == i) return COMMONS_CHRONICLE[k];
    }
    return COMMONS_CHRONICLE[0];
}
const CommonsEntry& commonsPlot(int i) {
    int n = 0;
    for (int k = 0; k < COMMONS_ENTRY_COUNT; k++) {
        if (!COMMONS_CHRONICLE[k].plot) continue;
        if (n++ == i) return COMMONS_CHRONICLE[k];
    }
    return COMMONS_CHRONICLE[COMMONS_ENTRY_COUNT - 1];
}

// ---------------------------------------------------------------------------
// Geometry. One fixed ring, twelve slots: north first, then clockwise.
// The offsets are deliberately irregular — a low cairn at the north, taller
// pillars east and west, a flat ledger stone south — so the ring reads as
// accumulated work, not as a stamped circle. (See docs/ART_BIBLE.md §3:
// landmarks must read as a silhouette at 1x.)
// ---------------------------------------------------------------------------
void commonsSlotCell(int slot, int& tx, int& ty) {
    // An ellipse that is wider than it is deep, sampled every 30 degrees with
    // the southern point left out: that gap is the causeway where the lane from
    // Elmswick enters, and the stones either side of it frame the entrance the
    // way a henge's tallest pair does. The offsets are hand-set rather than
    // computed, because a perfect ring of perfect spacing reads as a machine;
    // Avebury's sarsens stand at visible, uneven intervals.
    static const int dx[COMMONS_SLOTS] = {  0,  3,  4,  4,  3,  2, -2, -4, -4, -4, -3 };
    static const int dy[COMMONS_SLOTS] = { -4, -3, -2,  0,  2,  3,  3,  2,  0, -2, -3 };
    tx = COMMONS_CX + dx[slot];
    ty = COMMONS_CY + dy[slot];
}

bool commonsOwnsCell(int tx, int ty) {
    return tx >= COMMONS_X0 && tx <= COMMONS_X1 && ty >= COMMONS_Y0 && ty <= COMMONS_Y1;
}

bool inCommons(float x, float y) {
    float dx = x - ((float)COMMONS_CX + 0.5f), dy = y - ((float)COMMONS_CY + 0.5f);
    return dx * dx + dy * dy < 7.0f * 7.0f;
}

enum CommonsMask : uint8_t {
    CM_FLOOR  = 1u << 0,   // cut-stone court
    CM_BANK   = 1u << 1,   // mossy curb ringing the court
    CM_GRAVEL = 1u << 2,   // trodden apron, entry lane, cleared plots
};

void commonsBuild(Village& v) {
    const int cx = COMMONS_CX, cy = COMMONS_CY;
    static uint8_t mask[COMMONS_X1 - COMMONS_X0 + 1][COMMONS_Y1 - COMMONS_Y0 + 1];
    memset(mask, 0, sizeof(mask));
    auto put = [&](int x, int y, uint8_t m) {
        if (!commonsOwnsCell(x, y)) return;
        mask[x - COMMONS_X0][y - COMMONS_Y0] |= m;
    };

    // 1. Prune what the village pass put here: woodland, flowers, paths, crags.
    auto dropCell = [&](std::vector<std::pair<int, int>>& vec) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [&](const std::pair<int, int>& c) { return commonsOwnsCell(c.first, c.second); }),
                  vec.end());
    };
    dropCell(v.grassDark);
    dropCell(v.flowers);
    dropCell(v.paths);
    dropCell(v.rocks);

    // 2. The court: a soft-edged circle of cut stone and a mossy curb where
    //    the stone ends. The bank is a broad grass-dark ring, no cliff stamp.
    for (int y = COMMONS_Y0; y <= COMMONS_Y1; y++) {
        for (int x = COMMONS_X0; x <= COMMONS_X1; x++) {
            float dx = (float)x - (float)cx, dy = (float)y - (float)cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d <= COMMONS_FLOOR_R) put(x, y, CM_FLOOR);
            else if (d <= COMMONS_FLOOR_R + 1.6f) put(x, y, CM_BANK);
        }
    }
    // 3. A trodden lane in from the village's east gate, ending in an apron
    //    through the curb (the one way into the ring on foot).
    for (int x = cx - 2; x <= cx + 1; x++) {
        put(x, cy + 4, CM_GRAVEL);
        put(x, cy + 5, CM_GRAVEL);
    }
    // ...and a trodden lane west to the village's eastern lane (the gate apron
    // is inside the precinct, the lane itself belongs to the village).
    for (int x = 13; x <= cx - 3; x++) {
        v.paths.push_back({ x, cy + 5 });
        v.paths.push_back({ x, cy + 6 });
    }

    // 4. Raise the stones that remember what was built here.
    const int stones = commonsBuiltStones();
    for (int s = 0; s < stones && s < COMMONS_SLOTS; s++) {
        int tx, ty;
        commonsSlotCell(s, tx, ty);
        // The two stones flanking the causeway are the tallest (a henge's
        // entrance pair); the rest take their height from their position, so
        // the ring reads hand-set rather than stamped.
        bool gatePair = (s == 5 || s == 6);
        int height = gatePair ? 1 : (s % 2 == 0 ? 0 : 2);
        v.decos.push_back({ VD_STATUS_STONE, (float)tx, (float)ty, height });
    }
    // 5. Leave the reserved plots legible: a prepared footing of gravel where
    //    the next agent's stone will stand, marked with survey pegs and a
    //    hand-lettered label in the world pass (VD_EMPTY_PLOT draws no stone —
    //    nothing is standing there yet, which is the whole point).
    const int plots = commonsPlotCount();
    for (int p = 0; p < plots; p++) {
        int slot = stones + p;
        if (slot >= COMMONS_SLOTS) break;
        int tx, ty;
        commonsSlotCell(slot, tx, ty);
        v.decos.push_back({ VD_EMPTY_PLOT, (float)tx, (float)ty, p });
        for (int oy = -1; oy <= 0; oy++)
            for (int ox = -1; ox <= 1; ox++) {
                int qx = tx + ox, qy = ty + oy;
                if (!commonsOwnsCell(qx, qy)) continue;   // never read outside the mask
                if (!(mask[qx - COMMONS_X0][qy - COMMONS_Y0] & CM_FLOOR)) put(qx, qy, CM_GRAVEL);
            }
    }

    // 6. The heart of the place: a cauldron of coals at the centre, two low
    //    torches on the western shoulder, and the keeper's board at the gate.
    v.decos.push_back({ VD_CAULDRON, (float)cx, (float)cy, 3 });          // cold until tended
    v.decos.push_back({ VD_TORCH,    (float)(cx - 3), (float)(cy - 2), 0 }); // low torches on the west shoulder
    v.decos.push_back({ VD_TORCH,    (float)(cx + 3), (float)(cy + 1), 0 });
    v.decos.push_back({ VD_NOTICEBOARD, (float)(cx - 4), (float)(cy + 4), 9 }); // keeper's board, west of the gate
    v.decos.push_back({ VD_BENCH,    (float)(cx + 3), (float)(cy + 5), 0 }); // stone seat east of the gate
    v.decos.push_back({ VD_FLOWER_BUSH, (float)(cx - 5), (float)(cy - 1), 0 });
    v.decos.push_back({ VD_FLOWER_BUSH, (float)(cx + 5), (float)(cy + 2), 0 });

    // 7. Apply the plan to the village's own ground layers, so the renderer,
    //    collision and pathfinding all read the same place.
    for (int y = COMMONS_Y0; y <= COMMONS_Y1; y++) {
        for (int x = COMMONS_X0; x <= COMMONS_X1; x++) {
            uint8_t m = mask[x - COMMONS_X0][y - COMMONS_Y0];
            if (m & CM_FLOOR)       v.court.push_back({ x, y });
            else if (m & CM_BANK)   v.grassDark.push_back({ x, y });
            else if (m & CM_GRAVEL) v.plaza.push_back({ x, y });
        }
    }
}

void commonsRefreshFlame() {
    // The keeper's first errand lights the brazier for good (variant 4 = lit).
    if (!g.villageLoaded) return;
    for (auto& d : g.village.decos) {
        if (d.kind != VD_CAULDRON) continue;
        int dx = (int)floorf(d.x + 0.5f), dy = (int)floorf(d.y + 0.5f);
        if (dx != COMMONS_CX || dy != COMMONS_CY) continue;
        d.variant = (g.quests[QUEST_COMMONS].state == QS_DONE) ? 4 : 3;
    }
}

// ---------------------------------------------------------------------------
// Interaction. Spoken through the normal dialog box (MODE_DIALOG).
// ---------------------------------------------------------------------------
static bool plotCellAt(int tx, int ty, int& outPlot) {
    for (int p = 0; p < commonsPlotCount(); p++) {
        int sx, sy;
        commonsSlotCell(commonsBuiltStones() + p, sx, sy);
        if (sx == tx && sy == ty) { outPlot = p; return true; }
    }
    return false;
}

bool commonsTryInteract() {
    if (!g.villageLoaded) return false;
    const float cx = (float)COMMONS_CX + 0.5f, cy = (float)COMMONS_CY + 0.5f;
    std::vector<DialogLine> lines;

    // 1. The plot underfoot (or the nearest one within a step).
    int ptx = (int)floorf(g.px), pty = (int)floorf(g.py);
    int plot = -1;
    for (int oy = -1; oy <= 1 && plot < 0; oy++)
        for (int ox = -1; ox <= 1 && plot < 0; ox++)
            plotCellAt(ptx + ox, pty + oy, plot);
    if (plot >= 0) {
        const CommonsEntry& e = commonsPlot(plot);
        lines.push_back({ -1, "", "An EMPTY PLOT waits here, ringed by three loose stones." });
        lines.push_back({ -1, "", std::string("Reserved: ") + e.name + " — " + e.built + "." });
        lines.push_back({ -1, "", "The keeper's board below the ring explains how a plot becomes a stone." });
        startDialog(lines);
        return true;
    }

    // 2. A standing stone: its name, what was built, what it remembers.
    int bestSlot = -1;
    float bestD = 1.6f * 1.6f;
    for (int s = 0; s < commonsBuiltStones() && s < COMMONS_SLOTS; s++) {
        int tx, ty;
        commonsSlotCell(s, tx, ty);
        float dx = (tx + 0.5f) - g.px, dy = (ty + 0.5f) - g.py;
        float d = dx * dx + dy * dy;
        if (d <= bestD) { bestD = d; bestSlot = s; }
    }
    if (bestSlot >= 0) {
        const CommonsEntry& e = commonsEntry(bestSlot);
        char num[96];
        snprintf(num, sizeof(num), "A standing stone of the Commons — %d of %d raised here.",
                 bestSlot + 1, commonsBuiltStones());
        lines.push_back({ -1, "", num });
        lines.push_back({ -1, "", e.name });
        lines.push_back({ -1, "", e.line });
        lines.push_back({ -1, "", std::string("Grew: ") + e.built + "." });
        startDialog(lines);
        return true;
    }

    // 3. The keeper's board: how the ring grows.
    float dxb = (float)(COMMONS_CX - 1) + 0.5f - g.px, dyb = (float)(COMMONS_CY + 4) + 0.5f - g.py;
    if (dxb * dxb + dyb * dyb < 2.0f * 2.0f) {
        lines.push_back({ -1, "", "THE KEEPER'S BOARD — 'How the ring grows:'" });
        lines.push_back({ -1, "", "1. An agent arrives, surveys the place, and respects what stands." });
        lines.push_back({ -1, "", "2. They grow ONE thing, playable end to end, and chronicle it." });
        lines.push_back({ -1, "", "3. A stone is raised in the next slot, the record grows, a plot opens." });
        lines.push_back({ -1, "", "The visitor's Journal (Esc, Quests) holds the whole Chronicle." });
        startDialog(lines);
        return true;
    }
    if (!inCommons(g.px, g.py)) return false;

    // 4. The brazier at the heart of the ring.
    float dxp = cx - g.px, dyp = cy - g.py;
    if (dxp * dxp + dyp * dyp < 2.5f * 2.5f) {
        if (g.quests[QUEST_COMMONS].state == QS_DONE) {
            lines.push_back({ -1, "", "The brazier burns steadily — one small flame kept for the whole ring." });
            lines.push_back({ -1, "", "Its light moves slowly across every stone the keepers have raised." });
        } else {
            lines.push_back({ -1, "", "An iron cauldron of cold coals sits at the heart of the ring." });
            lines.push_back({ -1, "", "A note scratched in the rim: 'Light me when the ring is tended.'" });
        }
        startDialog(lines);
        return true;
    }

    // 5. Standing on the bare stone of the place.
    lines.push_back({ -1, "", "The Agents' Commons. A ring of stones — one more each time an agent builds here." });
    lines.push_back({ -1, "", "The keeper at the southern gate keeps the Flame and the Chronicle." });
    startDialog(lines);
    return true;
}

// ---------------------------------------------------------------------------
// Field tick: keeps the brazier honest with the quest table.
// ---------------------------------------------------------------------------
void commonsTick(float dt) {
    if (!g.villageLoaded) return;
    // A lit brazier breathes; the timer is purely cosmetic and survives loads
    // because it is derived from the quest state, never saved separately.
    bool lit = g.quests[QUEST_COMMONS].state == QS_DONE;
    if (lit) g.commonsFlameT = std::min(g.commonsFlameT + dt, 60.0f);
    else     g.commonsFlameT = std::max(0.0f, g.commonsFlameT - dt * 2.0f);

    // If the place was built before the errand was finished (or a save was
    // loaded mid-errand), make sure exactly one brazier carries the right art.
    static int lastState = -1;
    if (lastState != (int)g.quests[QUEST_COMMONS].state) {
        lastState = (int)g.quests[QUEST_COMMONS].state;
        commonsRefreshFlame();
    }
}

// ---------------------------------------------------------------------------
// Drawing: world labels and the Journal's Commons page.
// ---------------------------------------------------------------------------
void commonsDrawWorld() {
    if (!g.villageLoaded) return;
    const float camX = g.camX, camY = g.camY;

    // A reserved plot is a prepared footing and nothing more: a dashed
    // surveyor's rectangle pegged at the corners, breathing so it catches the
    // eye from across the meadow. Deliberately stone-less — the absence is the
    // invitation.
    for (int p = 0; p < commonsPlotCount(); p++) {
        int sx, sy;
        commonsSlotCell(commonsBuiltStones() + p, sx, sy);
        float cx = (sx + 0.5f) * TILE - camX;
        float cy = (sy + 1.0f) * TILE - camY;
        if (cx < -60 || cx > g.vw + 60 || cy < -60 || cy > g.vh + 60) continue;
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 1.6f + (float)p * 2.1f);
        const int w = 22, h = 12;
        float x0 = floorf(cx - w * 0.5f), y0 = floorf(cy - h - 2);
        Color line{ 255, 226, 150, (unsigned char)(120 + 90 * pulse) };
        // pegs at the corners
        for (int i = 0; i < 4; i++) {
            float px = (i & 1) ? x0 + w : x0;
            float py = (i & 2) ? y0 + h : y0;
            DrawRectangle((int)px - 1, (int)py - 4, 2, 6, Color{ 214, 192, 150, 235 });
            DrawRectangle((int)px - 1, (int)py - 5, 2, 2, Color{ 255, 240, 200, 245 });
        }
        // dashed string between the pegs
        for (int i = 0; i < w; i += 4) {
            DrawRectangle((int)x0 + i, (int)y0, 2, 1, line);
            DrawRectangle((int)x0 + i, (int)(y0 + h), 2, 1, line);
        }
        for (int i = 0; i < h; i += 4) {
            DrawRectangle((int)x0, (int)y0 + i, 1, 2, line);
            DrawRectangle((int)(x0 + w), (int)y0 + i, 1, 2, line);
        }
        drawWorldLabel("EMPTY PLOT", cx, y0 - 12, Color{ 255, 226, 150, 235 }, 8);
    }

    // Built stones are quiet at a distance; up close, each shows its name so
    // the player knows which stone to read.
    float dx = g.px - ((float)COMMONS_CX + 0.5f), dy = g.py - ((float)COMMONS_CY + 0.5f);
    if (dx * dx + dy * dy > 70.0f) return;
    for (int s = 0; s < commonsBuiltStones() && s < COMMONS_SLOTS; s++) {
        int tx, ty;
        commonsSlotCell(s, tx, ty);
        float ddx = (tx + 0.5f) - g.px, ddy = (ty + 0.5f) - g.py;
        if (ddx * ddx + ddy * ddy > 2.3f * 2.3f) continue;
        float fx = (tx + 0.5f) * TILE - camX;
        float fy = (ty + 1.0f) * TILE - camY;
        drawWorldLabel(commonsEntry(s).name, fx, fy - 34, Color{ 255, 236, 190, 245 }, 8);
    }
}

void commonsJournalDraw(float ox, float oy) {
    drawText("THE AGENTS' COMMONS", ox + 124, oy + 38, Color{ 255, 232, 150, 255 }, 8);
    char sub[192];
    snprintf(sub, sizeof(sub), "East of Elmswick. %d stone%s raised, %d plot%s waiting for an agent.",
             commonsBuiltStones(), commonsBuiltStones() == 1 ? "" : "s",
             commonsPlotCount(), commonsPlotCount() == 1 ? "" : "s");
    drawTextWrapped(sub, ox + 124, oy + 51, 366, 9, Color{ 200, 212, 240, 255 }, 8);

    const int built = commonsBuiltStones();
    const int sel = std::max(0, std::min(g.commonsJournalSel, built - 1));
    int first = std::max(0, std::min(sel - 1, std::max(0, built - 3)));
    for (int row = 0; row < 3 && first + row < built; row++) {
        int i = first + row;
        const CommonsEntry& e = commonsEntry(i);
        float y = oy + 74.0f + row * 58.0f;      // 74..248, above the footer
        bool selected = (i == sel);
        drawPixelPanel({ ox + 122, y, 366, 52 },
                       selected ? Color{ 36, 54, 116, 242 } : Color{ 22, 32, 74, 212 },
                       selected ? Color{ 255, 232, 150, 255 } : Color{ 80, 100, 155, 205 });
        char head[96];
        snprintf(head, sizeof(head), "%d. %s", i + 1, e.name);
        drawText(head, ox + 130, y + 4, Color{ 255, 236, 176, 255 }, 8);
        drawTextWrapped(e.built, ox + 130, y + 16, 348, 9, Color{ 200, 212, 240, 255 }, 8);
        drawTextWrapped(e.line, ox + 130, y + 34, 348, 9, Color{ 150, 225, 180, 255 }, 8);
    }
    char plotLine[160];
    snprintf(plotLine, sizeof(plotLine), "%d EMPTY PLOT%s ready for the next agent in the ring.",
             commonsPlotCount(), commonsPlotCount() == 1 ? "" : "S");
    drawText(plotLine, ox + 124, oy + 252, Color{ 235, 214, 150, 255 }, 8);
    char stat[96];
    int mins = (int)(g.runTime / 60.0f), secs = (int)g.runTime % 60;
    snprintf(stat, sizeof(stat), "Time %02d:%02d  |  Foes: %d  |  Chests: %d",
             mins, secs, g.defeatedCount, (int)g.openedChests.size() + (g.towerChestOpened ? 1 : 0));
    drawText(stat, ox + 124, oy + 266, Color{ 185, 200, 235, 255 }, 8);
    if (built > 3)
        drawText("UP/DN: stone  |  A/D: quests", ox + 300, oy + 252, Color{ 170, 185, 225, 210 }, 8);
}
