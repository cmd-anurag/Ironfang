#pragma once
#include <cstddef>
#include <cstdint>
#include "move.h"

enum TT_FLAG  { 
    TT_EXACT,
    TT_UPPER,
    TT_LOWER,
};

struct TTEntry {
    uint64_t key;
    int eval;
    uint16_t packedMove;
    uint8_t depth;
    // Two low bits hold TT_FLAG; the upper six bits hold the replacement age.
    uint8_t metadata;
};

static_assert(sizeof(TTEntry) == 16, "TT entries should stay compact");

// Use a 128 MiB budget with compact 16-byte entries.
constexpr size_t TT_SIZE = (128ULL * 1024 * 1024) / sizeof(TTEntry);
constexpr size_t TT_MASK = TT_SIZE - 1;
static_assert((TT_SIZE & TT_MASK) == 0, "TT_SIZE must be a power of two");

class TranspositionTable {
    public:
        TranspositionTable();
        ~TranspositionTable() = default;

        // False keeps a matched hash move for ordering but suppresses score reuse.
        bool probe(
            uint64_t zobristKey,
            int depth,
            int alpha,
            int beta,
            int ply,
            int &outEval,
            Move &outMove,
            bool allowCutoff = true
        );

        void store(
            uint64_t zobristKey,
            int depth,
            int eval,
            TT_FLAG flag,
            const Move &bestMove
        );

        void clear();
        uint64_t lookupCount = 0;      // all TT probes, including quiescence probes
        uint64_t hitCount = 0;         // probes that returned a usable alpha-beta bound
        uint64_t keyMatchCount = 0;    // probes whose indexed entry matched the full key
        size_t totalStoreAttempts = 0;
        size_t actualStores = 0;
        size_t overwritten = 0;
        size_t entriesOccupied = 0;
        int currentAge;

        size_t countOccupied() const;
        int hashfull() const;

    private:
        TTEntry table[TT_SIZE];
};
