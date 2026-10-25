#ifndef MOVE_H
#define MOVE_H
#include <cstdint>
#include <string>
#include <iostream>
#include "types.h"

class Board;
class BitBoard;

struct Move
{
    Piece piece;

    int from;
    int to;
    int promotion = 0;
    int capture = 0;
    bool isKingSideCastle = false;
    bool isQueenSideCastle = false;
    bool isEnPassant = false;

    int heuristicScore = 0;

    Move() : piece(NONE), from(-1), to(-1) {};

    Move(Piece p, int from, int to, int promotion = 0, int capture = 0, bool isKingSideCastle = false, bool isQueenSideCastle = false, bool isEnPassant = false) : piece(p), from(from), to(to), promotion(promotion), capture(capture), isKingSideCastle(isKingSideCastle), isQueenSideCastle(isQueenSideCastle), isEnPassant(isEnPassant) {};
};

// TT-only marker used to preserve the reference's packed pawn-double-push
// flag after decoding a move hint. Search never treats this as a board piece.
inline constexpr Piece TT_PAWN_DOUBLE_PUSH_HINT = static_cast<Piece>(15);

// Four-bit move flags in the reference engine's compact 16-bit move format.
// Keep the flag-to-promotion mapping shared by move generation, search, and
// make/unmake so they cannot silently disagree about the same packed move.
enum CompactMoveFlag : uint8_t {
    COMPACT_MOVE_NORMAL = 0,
    COMPACT_MOVE_EN_PASSANT = 1,
    COMPACT_MOVE_CASTLE = 2,
    COMPACT_MOVE_PAWN_DOUBLE_PUSH = 3,
    COMPACT_MOVE_PROMOTE_QUEEN = 4,
    COMPACT_MOVE_PROMOTE_KNIGHT = 5,
    COMPACT_MOVE_PROMOTE_ROOK = 6,
    COMPACT_MOVE_PROMOTE_BISHOP = 7
};

std::string moveToAlgrebraic(Move move);
std::string moveToUCI(Move move);
Move uciToMove(const std::string& uci, BitBoard &board);
std::ostream& operator<<(std::ostream& os, const Move &move);
bool operator==(const Move &lhs, const Move &rhs);
bool operator!=(const Move &lhs, const Move &rhs);
#endif
