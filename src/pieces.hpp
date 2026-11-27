// pieces.hpp — the field is made of map pieces, not one field
//
// Every 16-bit RPG drew its world as *maps*: a bounded rectangle of tiles with
// a composed edge, stitched to the next map by matched openings. This game used
// to be one endless field of noise, which is why it read as a blob no matter how
// good the tiles were. Here the field is cut into pieces the size of one screen
// (PIECE_W x PIECE_H tiles), each generated from its own seed, each framed, and
// each stitched to its neighbours by an edge both sides generate identically.
//
// The world the previous agents built is preserved: rivers, roads, the village,
// the pass ranges, the coast and every story site still come from the existing
// generators. Pieces decide what happens *at their seams* — the treeline, the
// rock wall, the hedge and the gate — the way a hand-authored map's border did.
#pragma once

#include "game.hpp"

// A map piece is a *map*, not a screen: bigger than the field view, so the
// camera can follow the party inside it, stop at its framed edge, and reveal
// the border the way a 16-bit map's edge did. 44x26 tiles at 16px is a map of
// 704x416 pixels against a 512x288 view.
constexpr int PIECE_W = 44;          // tiles across one map piece
constexpr int PIECE_H = 26;          // tiles down one map piece
constexpr int PIECE_BAND = 2;        // tiles of border dressing inside each seam

// The lattice is pinned so Elmswick's square sits in the middle of its piece
// instead of on a seam.
constexpr int PIECE_OX = -PIECE_W / 2;
constexpr int PIECE_OY = -PIECE_H / 2;

// ---- geometry ------------------------------------------------------------
int  pieceFloorDiv(int a, int b);                       // floor division, negatives safe
int  pieceIndexOf(int tileCoord, int size);             // tile -> piece index
void pieceOfTile(int tx, int ty, int& pieceX, int& pieceY);
int  pieceLocalX(int tx);              // 0..PIECE_W-1 inside the piece
int  pieceLocalY(int ty);

// Top-left of the view that frames the piece the party stands in. When the
// window is wider than a piece the piece is centred in it instead.
void pieceOriginPixels(float tileX, float tileY, float viewW, float viewH,
                       float& outX, float& outY);

// ---- seams ---------------------------------------------------------------
// How a seam is dressed. Both pieces that share the seam compute the same
// answer from the same key, which is what "stitched" means.
enum PieceSeam { SEAM_OPEN = 0, SEAM_WOOD, SEAM_RIDGE, SEAM_HEDGE, SEAM_COUNT };

// cross = the piece index along the seam (pieceY for a vertical seam, pieceX for
// a horizontal one), so each piece gets its own gate in the shared edge.
int  pieceSeamStyle(int seamIndex, int axis, int cross);
int  pieceSeamGate(int seamIndex, int axis, int cross);            // first tile of the gate
bool pieceSeamGap(int seamIndex, int axis, int cross, int along);  // true inside the gate

// ---- terrain hooks (called from world.cpp) -------------------------------
// Dress one tile of the border band. Only soft ground is touched; water, rock,
// roads, bridges, plaza, passes, the village and story sites are left alone.
void pieceDressGround(int tx, int ty, GroundTile& t);

// A border's trunk/solid prop, or P_NONE. Gates and their approaches are always
// cleared so a seam can never wall the party in.
PropKind pieceBorderProp(int tx, int ty);

// Every piece also grows one small, composed feature of its own — a copse, a
// boulder ring, a thicket, a glade, a fallen trunk — so two pieces of the same
// country are never the same map. Returns P_NONE off-feature.
PropKind pieceFeatureProp(int tx, int ty);
