#pragma once
#include <cstdint>
#include "types.h"
#include "board.h"
#include "bitboard.h"

extern uint64_t zobristPieces[12][64]; // 12 Piece types - 64 possible squares for each
extern uint64_t zobristCastling[16]; // one key for each castling-rights state
extern uint64_t zobristEnPassant[9]; // index 0 is no square, 1-8 are files a-h
extern uint64_t zobristBlackToMove;

void initZobrist();
uint64_t generateZobristHashKey(Board &b);
uint64_t generateZobristHashKey(BitBoard &b);

inline int pieceToZobristIndex(Piece p) {
    if (p == NONE) return -1;
    if (p < BP) return p - 1;    // WP=1 → 0, WK=6 → 5
    return p - 3;                // BP=9 → 6, BK=14 → 11
}

inline int castlingRightsIndex(bool whiteKingside, bool whiteQueenside,
                               bool blackKingside, bool blackQueenside) {
    return (whiteKingside ? 1 : 0) |
           (whiteQueenside ? 2 : 0) |
           (blackKingside ? 4 : 0) |
           (blackQueenside ? 8 : 0);
}

inline void xorPiece(uint64_t &h, Piece p, int sq) {
    int idx = pieceToZobristIndex(p);
    // The reference indexes a1 as square 0; Ironfang's bitboard indexes a8 as 0.
    if (idx >= 0) h ^= zobristPieces[idx][sq ^ 56];
}
