#pragma once
#include <cstddef>
#include <cstdint>

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
    uint8_t flag : 2;
    uint8_t age  : 6;
};

static_assert(sizeof(TTEntry) == 16, "TT entries should stay compact");

// Match the reference's 64 MiB direct-mapped table. With 16-byte entries this
// yields exactly 4,194,304 slots; indexing uses mask TT_MASK = TT_SIZE - 1.
constexpr size_t TT_SIZE = (64ULL * 1024 * 1024) / sizeof(TTEntry);
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
            uint16_t &outMove,
            bool allowCutoff = true
        );

        void store(
            uint64_t zobristKey,
            int depth,
            int eval,
            TT_FLAG flag,
            uint16_t bestMove
        );

        void clear();
        void incrementAge() { ++currentAge; }
        uint8_t currentAge = 0;
        uint64_t lookupCount = 0;      // all TT probes in the main search
        uint64_t hitCount = 0;         // probes that returned a usable alpha-beta bound
        uint64_t keyMatchCount = 0;    // probes whose indexed entry matched the full key
        size_t totalStoreAttempts = 0;
        size_t actualStores = 0;
        size_t overwritten = 0;
        size_t entriesOccupied = 0;
        size_t countOccupied() const;
        int hashfull() const;

    private:
        TTEntry table[TT_SIZE];
};
