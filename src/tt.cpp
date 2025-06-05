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
    if (entry.key == zobristKey && entry.age == currentAge) {
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

    // If you want to prevent overwriting a real (positive-depth) entry
    // with a quiescence (negative-depth) entry, uncomment:
    // if (depth < 0 && entry.depth > 0 && entry.key != zobristKey) {
    //     return;
    // }

    // Either empty slot or deeper search => store
    if (entry.key != zobristKey || entry.depth <= depth) {
        if (isOverwrite) ++overwritten;
        entry.key = zobristKey;
        entry.depth = depth;
        entry.eval = eval;
        entry.flag = flag;
        entry.bestMove = bestMove;
        entry.age = currentAge; // record current search age
        ++actualStores;
    }
    // Always preserve best move if the position matches
    else if (entry.key == zobristKey && bestMove.piece != NONE) {
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
    // Using actualStores vs overwritten to estimate occupancy
    int estimatedOccupancy = actualStores - overwritten;
    return std::min((unsigned long)1000, std::max((unsigned long)0, (estimatedOccupancy * 1000) / TT_SIZE));
}
