#ifndef NNUE_H
#define NNUE_H

#include <string>
#include <cstdint>
#include "types.h"
#include "accumulator.h"

class BitBoard;

class NNUE {
public:
    static bool initialize();
    static bool loadFile(const std::string& path);
    static bool isLoaded();

    // Recomputes the entire accumulator from scratch
    static void refreshAccumulator(const BitBoard& board, Accumulator& acc);
    static void refreshPerspective(const BitBoard& board, Accumulator& acc, Color side);

    // Incremental feature updates for makeMove
    static void addPiece(Accumulator& acc, int kingSq, Piece piece, int square, Color side);
    static void removePiece(Accumulator& acc, int kingSq, Piece piece, int square, Color side);
    static void updateMove(Accumulator& acc, const BitBoard& board, uint16_t encodedMove);
    static void updateMove(const Accumulator& prev, Accumulator& next, const BitBoard& board, uint16_t encodedMove);

    // Forward inference pass: returns evaluation in centipawns from the perspective of sideToMove.
    static int evaluate(const BitBoard& board, Accumulator& acc);

    // HalfKAv2 piece type mapping (0..10):
    // 0..4: Friendly P, N, B, R, Q
    // 5..10: Enemy P, N, B, R, Q, K
    static inline int whitePieceType(Piece p) {
        if (p >= WP && p <= WQ) return p - WP;       // 0..4
        if (p >= BP && p <= BK) return p - BP + 5;   // 5..10
        return -1; // WK
    }

    static inline int blackPieceType(Piece p) {
        if (p >= BP && p <= BQ) return p - BP;       // 0..4
        if (p >= WP && p <= WK) return p - WP + 5;   // 5..10
        return -1; // BK
    }

    // HalfKAv2 feature index: kingSq * 704 + pieceType * 64 + square
    static inline int whiteFeatureIndex(int whiteKingSq, Piece p, int sq) {
        const int pIdx = whitePieceType(p);
        if (pIdx < 0) return -1;
        return whiteKingSq * 704 + pIdx * 64 + sq;
    }

    static inline int blackFeatureIndex(int blackKingSq, Piece p, int sq) {
        const int pIdx = blackPieceType(p);
        if (pIdx < 0) return -1;
        const int kFlipped = blackKingSq ^ 56;
        const int sqFlipped = sq ^ 56;
        return kFlipped * 704 + pIdx * 64 + sqFlipped;
    }
};

extern bool useNNUE;

#endif // NNUE_H
