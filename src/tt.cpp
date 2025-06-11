#include "tt.h"
#include <cstring>
#include "algorithm"

TranspositionTable::TranspositionTable()
{
    for (size_t i = 0; i < TT_SIZE; ++i)
    table[i] = TTEntry{};
    // setting all keys to zero indicating empty
    currentAge = 0;
}

bool TranspositionTable::probe(uint64_t zobristKey, int depth, int alpha, int beta, int &outEval, Move &outMove)
{
    ++lookupCount;

    size_t index = zobristKey & TT_MASK;
    TTEntry &entry = table[index];

    // Optional: check if entry is from current search age
    if (entry.key == zobristKey) {
        ++keyMatchCount;
        // Always extract the move if it's the correct position
        outMove = entry.bestMove;

        // Check if stored depth is enough
        if (entry.depth >= depth) {
            switch(entry.flag) {
                case TT_EXACT:
                    ++hitCount;
                    outEval = entry.eval;
                    return true;
                case TT_LOWER:
                    if (entry.eval >= beta) {
                        ++hitCount;
                        outEval = entry.eval;
                        return true;
                    }
                    break;
                case TT_UPPER:
                    if (entry.eval <= alpha) {
                        ++hitCount;
                        outEval = entry.eval;
                        return true;
                    }
                    break;
            }
        }
    }

    return false;
}


void TranspositionTable::store(uint64_t zobristKey, int depth, int eval,
                               TT_FLAG flag, const Move &bestMove)
{
    ++totalStoreAttempts;
    size_t index = zobristKey & TT_MASK;
    TTEntry &entry = table[index];

    bool isOverwrite = (entry.key != 0 && entry.key != zobristKey);

    // Prevent quiescence (depth<0) from wiping out real searches
    if (depth < 0 && entry.depth > 0 && entry.key != zobristKey)
        return;

    // --- start replacement-policy patch ---
    bool shouldReplace = false;

    if (entry.key == 0) {
        // Empty slot
        shouldReplace = true;
    }
    else if (entry.key == zobristKey) {
        // Same position: always allow updates for deeper or better moves
        shouldReplace = (depth >= entry.depth);
    }
    else {
        // Different key: compare old vs new “priority score”
        int ageDiff = currentAge - entry.age;
        int oldScore = entry.depth - ageDiff;
        int newScore = depth;  // fresh entry has no age penalty

        shouldReplace = (newScore >= oldScore);
    }
    // --- end replacement-policy patch ---

    if (shouldReplace) {
        if (entry.key == 0)       ++entriesOccupied;
        else if (isOverwrite)     ++overwritten;

        entry.key      = zobristKey;
        entry.depth    = depth;
        entry.eval     = eval;
        entry.flag     = flag;
        entry.bestMove = bestMove;
        entry.age      = currentAge;
        ++actualStores;
    }
    else if (entry.key == zobristKey && bestMove.piece != NONE) {
        // still update the move if it’s a better move for same position
        entry.bestMove = bestMove;
    }
}


void TranspositionTable::clear() {
    for (size_t i = 0; i < TT_SIZE; ++i)
    table[i] = TTEntry{};
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
