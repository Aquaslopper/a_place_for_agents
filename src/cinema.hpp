// cinema.hpp — the way the game cuts between places
//
// A 16-bit JRPG never simply *drops* you into the field. It fades in steps,
// moves the camera, and names the place in a plate across the screen. This
// module owns that presentation so no other file has to invent it again.
#pragma once

#include "game.hpp"

// Arrive somewhere: a stepped fade, a camera that glides in from the direction
// the party came (panTiles are camera offsets in tiles, so (0,-6) starts the
// shot six tiles north of the party), then a plate naming the place.
void cinemaArrive(const std::string& title, const std::string& sub,
                  float panTilesX = 0.0f, float panTilesY = 0.0f);

// Name a place without moving the camera (crossing into a new country, a
// village you walked into, a shore you beached on).
void cinemaAnnounce(const std::string& title, const std::string& sub);

// The plate for the country underfoot, with the right subtitle for it.
void cinemaAnnounceRegion();

// Name a village the first time the party walks into it (Elmswick is named by
// the opening shot instead). Called every field frame; costs nothing.
void cinemaVillageTick();

void cinemaUpdate(float dt);      // advance the shot; drives the camera while it owns it
bool cinemaOwnsCamera();          // true while the establishing pan is running
bool cinemaBlocksInput();         // true while the player should not steer
bool cinemaPlateVisible();
void cinemaClear();               // for a hard reset (new game, load)

// render.cpp / core ui helpers are used, but the plate lives here.
void drawAreaPlate();
void drawSteppedFade();           // the eight-step palette fade every cut uses
void drawCinemaOverlay();         // plate + fade, in the right order
