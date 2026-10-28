#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <cstdint>
#include <array>

// HalfKAv2 Architecture (King-Relative Piece Squares)
// 64 king squares * 11 piece types * 64 squares = 45,056 features per perspective
// Friendly King is the bucket index (0..63); other 11 piece types (5 friendly + 6 enemy) are features.
constexpr int NNUE_INPUTS = 45056;
constexpr int NNUE_HIDDEN = 512;
constexpr int NNUE_SCALE = 400;

struct alignas(64) Accumulator {
    alignas(64) std::array<int16_t, NNUE_HIDDEN> white;
    alignas(64) std::array<int16_t, NNUE_HIDDEN> black;
    bool computedWhite = false;
    bool computedBlack = false;

    void init(const int16_t* biases) {
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            white[i] = biases[i];
            black[i] = biases[i];
        }
        computedWhite = true;
        computedBlack = true;
    }
};

#endif // ACCUMULATOR_H
