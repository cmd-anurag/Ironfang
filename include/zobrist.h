#pragma once
#include <cstdint>
#include "types.h"
#include "board.h"

extern uint64_t zobristPieces[12][64]; // 12 Piece types - 64 possible squares for each
extern uint64_t zobristCastling[4]; // KQkq
extern uint64_t zobristEnPassant[8]; // a-h
extern uint64_t zobristBlackToMove;

void initZobrist();
uint64_t generateZobristHashKey(Board b);

inline int pieceToZobristIndex(Piece p) {
    if (p == NONE) return -1;
    if (p < BP) return p - 1;    // WP=1 → 0, WK=6 → 5
    return p - 3;                // BP=9 → 6, BK=14 → 11
}

inline void xorPiece(uint64_t &h, Piece p, int sq) {
    int idx = pieceToZobristIndex(p);
    if (idx >= 0) h ^= zobristPieces[idx][sq];
}