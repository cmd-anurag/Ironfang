#include "tt.h"
#include <cstring>

TranspositionTable::TranspositionTable()
{
    for (size_t i = 0; i < TT_SIZE; ++i)
    table[i] = TTEntry{};
    // setting all keys to zero indicating empty
}

bool TranspositionTable::probe(uint64_t zobristKey, int depth, int alpha, int beta, int &outEval, Move &outMove)
{
    ++lookupCount;

    size_t index = zobristKey & TT_MASK;
    TTEntry &entry = table[index];

    if (entry.key == zobristKey) {
        // Always extract the move if it's the correct position
        outMove = entry.bestMove;

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


void TranspositionTable::store(uint64_t zobristKey, int depth, int eval, TT_FLAG flag, const Move &bestMove)
{
    ++totalStoreAttempts;

    size_t index = zobristKey & TT_MASK;
    TTEntry &entry = table[index];

    bool isOverwrite = (entry.key != 0 && entry.key != zobristKey);

    // Don't replace important entries with quiescence entries
    // Quiescence searches use negative depth, regular searches use positive depth
    if (depth < 0 && entry.depth > 0 && entry.key != zobristKey) {
        // Don't overwrite regular search entries with quiescence entries
        return;
    }

    // Replace if empty, different position, or new entry is deeper
    if (entry.key != zobristKey || entry.depth <= depth) {
        if (isOverwrite) ++overwritten;
        entry.key = zobristKey;
        entry.depth = depth;
        entry.eval = eval;
        entry.flag = flag;
        entry.bestMove = bestMove;
        ++actualStores;
    } 
    // Always preserve the best move, even if we don't replace the entry
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