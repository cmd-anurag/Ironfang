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

    size_t index = static_cast<size_t>(zobristKey % TT_SIZE);
    TTEntry &entry = table[index];

    // Match the reference's hash-move hint behavior: move ordering may use
    // the indexed move even when the stored key collided with this position.
    // The reference always reads the indexed entry's move, even for an empty
    // slot (whose default move is a1-a1) or a matching zero key.
    outMove = entry.packedMove;

    if (entry.key == zobristKey) {
        ++keyMatchCount;

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
    size_t index = static_cast<size_t>(zobristKey % TT_SIZE);
    TTEntry &entry = table[index];

    // The reference replaces the indexed entry on every store. Its TT is
    // direct-mapped and has no depth/age replacement preference.
    if (entry.key == 0)
        ++entriesOccupied;
    else if (entry.key != zobristKey)
        ++overwritten;

    entry.key = zobristKey;
    entry.depth = static_cast<uint8_t>(depth);
    entry.eval = eval;
    entry.packedMove = bestMove;
    entry.flag = static_cast<uint8_t>(flag);
    ++actualStores;
}


void TranspositionTable::clear() {
    for (size_t i = 0; i < TT_SIZE; ++i)
        table[i] = TTEntry{};
    entriesOccupied = 0;
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
