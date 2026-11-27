// One director owns every stream. Menus are overlays, not new music scenes.
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include "commons.hpp"

namespace {
const char* const files[] = {"title", "village", "field", "wood", "fen", "pass", "dunes",
                             "hearth", "chapel", "battle", "boss", "victory", "gameover"};
static_assert(sizeof(files)/sizeof(*files) == MUS_COUNT);
struct Voice { float gain = 0; bool live = false, paused = false; };
Voice voices[MUS_COUNT];
MusicCue current = MUS_NONE, candidate = MUS_NONE;
float dwell = 0;
bool wasAudition = false;
bool location(MusicCue c) { return c >= MUS_VILLAGE && c <= MUS_CHAPEL; }

MusicCue sceneCue(Mode scene) {
    if (scene == MODE_TITLE || scene == MODE_INTRO) return MUS_TITLE;
    if (scene == MODE_GAMEOVER) return MUS_GAMEOVER;
    if (scene == MODE_BATTLE) return battleMusicCue();
    if (scene == MODE_ROOM) return g.room.kind == ROOM_TEMPLE ? MUS_CHAPEL : MUS_HEARTH;
    if (scene == MODE_DUNGEON) return dungeonMusicCue();
    // Out on the water the score has no sea cue, so the Open Road carries it:
    // silence over a lake is worse than a borrowed tune.
    if (scene == MODE_SAIL) return MUS_FIELD;
    if (inCommons(g.px,g.py) || inVillage(g.px,g.py) || (!g.villageLoaded && g.px*g.px+g.py*g.py < 18*18)) return MUS_VILLAGE;
    if (inBossArena(g.px,g.py)) return MUS_PASS;
    switch (sampleGround((int)floorf(g.px),(int)floorf(g.py)).biome) {
        case BI_AMBER: case BI_FOREST: return MUS_WOOD;
        case BI_FEN: return MUS_FEN;
        case BI_HEATH: case BI_FROST: return MUS_PASS;
        case BI_DUNES: return MUS_DUNES;
        default: return MUS_FIELD;
    }
}
}

MusicCue playingCue() { return current; }
const char* cueTitle(MusicCue cue) {
    static const char* const titles[] = {"Overture", "Elmswick", "The Open Road", "Amberwood",
        "The Mirefen", "The Northern Pass", "Sunscorch Flats", "By the Hearth", "Meadow Chapel",
        "Crossed Blades", "The Slime King", "Victory", "Elegy"};
    static_assert(sizeof(titles)/sizeof(*titles) == MUS_COUNT);
    return cue==MUS_NONE ? "Silence" : titles[cue];
}

void loadScore(const std::string& dir) {
    if (!IsAudioDeviceReady()) return;
    for (int i=0; i<MUS_COUNT; ++i) {
        Music& m = g.assets.score[i];
        m = LoadMusicStream((dir + "audio/" + files[i] + ".ogg").c_str());
        m.looping = i != MUS_VICTORY && i != MUS_GAMEOVER;
    }
}

void updateScore(float dt) {
    if (!IsAudioDeviceReady()) return;
    Mode scene = g.mode == MODE_MENU ? g.menuReturn : g.mode == MODE_DIALOG ? g.dialogReturn : g.mode;
    bool audition = g.mode==MODE_MENU && g.menuPage==7;
    MusicCue want = !g.musicOn ? MUS_NONE : audition ? g.audition : sceneCue(scene);
    if (want != candidate) { candidate=want; dwell=0; }
    dwell += dt;
    // A few steps across a biome seam shouldn't restart a phrase. Room,
    // combat, result and mute transitions take effect on this very frame.
    bool crossing = !audition && !wasAudition && scene==MODE_OVERWORLD && location(current) && location(want) && current<=MUS_DUNES;
    wasAudition=audition;
    if (want!=current && (!crossing || dwell>=1.25f)) {
        current=want;
        if (current!=MUS_NONE) {
            auto& v=voices[current]; auto& m=g.assets.score[current];
            if (IsMusicValid(m) && m.stream.buffer) {
                if (!v.live) {
                    SetMusicVolume(m,0);
                    if (v.paused) ResumeMusicStream(m); else PlayMusicStream(m);
                }
                v.live=true; v.paused=false;
            }
        }
    }
    float step = dt / (location(current) ? 0.70f : 0.20f);
    for (int i=0;i<MUS_COUNT;++i) {
        Voice& v=voices[i];
        if (!v.live) continue;
        Music& m=g.assets.score[i];
        v.gain = !g.musicOn ? 0.f : i==current ? std::min(1.f,v.gain+step) : std::max(0.f,v.gain-step);
        SetMusicVolume(m,0.75f*v.gain);
        if (i!=current && v.gain<=0) {
            v.paused=location((MusicCue)i);
            if (v.paused) PauseMusicStream(m); else StopMusicStream(m);
            v.live=false;
        } else if (IsMusicStreamPlaying(m)) UpdateMusicStream(m);
        // A finished fanfare stays finished until the scene changes.
    }
}

void unloadScore() {
    for (Music& m:g.assets.score) if (m.ctxData) { UnloadMusicStream(m); m={}; }
}
