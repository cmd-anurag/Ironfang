#include "tt.h"
#include <cstring>
#include <algorithm>
namespace {
TT_FLAG storedFlag(const TTEntry &entry)
{
    return static_cast<TT_FLAG>(entry.flag);
}
}

TranspositionTable::TranspositionTable()
{
    for (size_t i = 0; i < TT_SIZE; ++i)
    table[i] = TTEntry{};
}

bool TranspositionTable::probe(uint64_t zobristKey, int depth, int alpha, int beta,
                               int ply, int &outEval, uint16_t &outMove, bool allowCutoff)
{
    ++lookupCount;
    outMove = 0;

    size_t index = static_cast<size_t>(zobristKey & TT_MASK);
    TTEntry &entry = table[index];

    if (entry.key == zobristKey) {
        ++keyMatchCount;
        outMove = entry.packedMove;

        // Check if stored depth is enough
        if (allowCutoff && entry.depth >= depth) {
            // Mate scores are normalized when stored so their distance is
            // independent of the path that first reached this position.
            // Restore the current path's score before comparing alpha/beta.
            int eval = entry.eval;
            constexpr int mateThreshold = 99000;
            if (eval > mateThreshold) eval -= ply;
            else if (eval < -mateThreshold) eval += ply;

            switch(storedFlag(entry)) {
                case TT_EXACT:
                    // The reference uses -1 as both a valid evaluation and
                    // LookupFailed. Searcher treats an exact -1 result as a
                    // miss and searches the node again, so preserve that
                    // behavior for decision parity.
                    if (eval == -1) return false;
                    ++hitCount;
                    outEval = eval;
                    return true;
                case TT_LOWER:
                    if (eval >= beta) {
                        if (eval == -1) return false;
                        ++hitCount;
                        outEval = eval;
                        return true;
                    }
                    break;
                case TT_UPPER:
                    if (eval <= alpha) {
                        if (eval == -1) return false;
                        ++hitCount;
                        outEval = eval;
                        return true;
                    }
                    break;
            }
        }
    }

    return false;
}


void TranspositionTable::store(uint64_t zobristKey, int depth, int eval,
                               TT_FLAG flag, uint16_t bestMove)
{
    ++totalStoreAttempts;
    size_t index = static_cast<size_t>(zobristKey & TT_MASK);
    TTEntry &entry = table[index];

    bool isOverwrite = (entry.key != 0 && entry.key != zobristKey);

    bool shouldReplace = false;

    if (entry.key == 0) {
        shouldReplace = true;
    }
    else if (entry.key == zobristKey) {
        shouldReplace = (depth >= entry.depth);
    }
    else {
        int ageDiff = (currentAge - entry.age + 64) & 0x3F;
        int oldScore = entry.depth - ageDiff;
        int newScore = depth;

        shouldReplace = (newScore >= oldScore);
    }

    if (shouldReplace) {
        if (entry.key == 0)       ++entriesOccupied;
        else if (isOverwrite)     ++overwritten;

        entry.key = zobristKey;
        entry.depth = static_cast<uint8_t>(std::clamp(depth, 0, 255));
        entry.eval = eval;
        entry.packedMove = bestMove;
        entry.flag = static_cast<uint8_t>(flag);
        entry.age = currentAge & 0x3F;
        ++actualStores;
    }
    else if (entry.key == zobristKey && bestMove != 0) {
        entry.packedMove = bestMove;
    }
}


void TranspositionTable::clear() {
    for (size_t i = 0; i < TT_SIZE; ++i)
        table[i] = TTEntry{};
    entriesOccupied = 0;
    currentAge = 0;
}

size_t TranspositionTable::countOccupied() const {
    size_t count = 0;
    for (size_t i = 0; i < TT_SIZE; ++i) {
        if (table[i].key != 0) ++count;
    }
    return count;
}

int TranspositionTable::hashfull() const
{
    // Calculate hashfull based on the total number of occupied entries
    if (TT_SIZE == 0) return 0; // Avoid division by zero
    return static_cast<int>((static_cast<uint64_t>(entriesOccupied) * 1000) / TT_SIZE);
}
