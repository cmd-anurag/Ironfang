#pragma once
#include <cstdint>
#include "move.h"

constexpr size_t TT_SIZE = 1 << 23; // 2^23
constexpr size_t TT_MASK = TT_SIZE  - 1;

enum TT_FLAG  { 
    TT_EXACT,
    TT_UPPER,
    TT_LOWER,
};

struct TTEntry {
    uint64_t key;
    int depth;
    int eval;
    TT_FLAG flag;
    Move bestMove;
    int age;
};

class TranspositionTable {
    public:
        TranspositionTable();
        ~TranspositionTable() = default;

        bool probe(
            uint64_t zobristKey,
            int depth,
            int alpha,
            int beta,
            int &outEval,
            Move &outMove
        );

        void store(
            uint64_t zobristKey,
            int depth,
            int eval,
            TT_FLAG flag,
            const Move &bestMove
        );

        void clear();
        uint64_t lookupCount = 0; 
        uint64_t hitCount    = 0;
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
    