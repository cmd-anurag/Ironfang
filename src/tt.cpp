#include "tt.h"
#include <cstring>
#include <algorithm>

namespace {
constexpr uint16_t INVALID_PACKED_MOVE = 0xFFFF;
constexpr uint8_t TT_FLAG_MASK = 0x03;
constexpr int TT_AGE_MASK = 0x3F;

uint16_t packMove(const Move &move)
{
    if (move.from < 0 || move.from >= 64 || move.to < 0 || move.to >= 64)
        return INVALID_PACKED_MOVE;

    const uint16_t promotionType = static_cast<uint16_t>(move.promotion & 7);
    return static_cast<uint16_t>(move.from | (move.to << 6) | (promotionType << 12));
}

Move unpackMove(uint16_t packedMove)
{
    if (packedMove == INVALID_PACKED_MOVE)
        return Move(NONE, -1, -1);

    const int from = packedMove & 0x3F;
    const int to = (packedMove >> 6) & 0x3F;
    const int promotionType = (packedMove >> 12) & 0x07;
    // The caller uses this only as a hash-move hint. Piece and capture data are
    // already present in the generated move and are not needed for matching.
    return Move(NONE, from, to, promotionType);
}

uint8_t packMetadata(TT_FLAG flag, int age)
{
    return static_cast<uint8_t>(((age & TT_AGE_MASK) << 2) |
                                (static_cast<uint8_t>(flag) & TT_FLAG_MASK));
}

int storedAge(const TTEntry &entry)
{
    return entry.metadata >> 2;
}

TT_FLAG storedFlag(const TTEntry &entry)
{
    return static_cast<TT_FLAG>(entry.metadata & TT_FLAG_MASK);
}
}

TranspositionTable::TranspositionTable()
{
    for (size_t i = 0; i < TT_SIZE; ++i)
    table[i] = TTEntry{};
    // setting all keys to zero indicating empty
    currentAge = 0;
}

bool TranspositionTable::probe(uint64_t zobristKey, int depth, int alpha, int beta,
                               int ply, int &outEval, Move &outMove, bool allowCutoff)
{
    ++lookupCount;

    size_t index = zobristKey & TT_MASK;
    TTEntry &entry = table[index];

    // ABSOLUTELY DONT MESS AROUND HERE
    if (entry.key == zobristKey) {
        ++keyMatchCount;
        // Always extract the move if it's the correct position
        outMove = unpackMove(entry.packedMove);

        // Check if stored depth is enough
        if (allowCutoff && entry.depth >= depth) {
            // Mate scores are normalized when stored so their distance is
            // independent of the path that first reached this position.
            // Restore the current path's score before comparing alpha/beta.
            int eval = entry.eval;
            constexpr int mateThreshold = 800000;
            if (eval >= mateThreshold) eval -= ply;
            else if (eval <= -mateThreshold) eval += ply;

            switch(storedFlag(entry)) {
                case TT_EXACT:
                    ++hitCount;
                    outEval = eval;
                    return true;
                case TT_LOWER:
                    if (eval >= beta) {
                        ++hitCount;
                        outEval = eval;
                        return true;
                    }
                    break;
                case TT_UPPER:
                    if (eval <= alpha) {
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
        int ageDiff = (currentAge - storedAge(entry) + 64) & TT_AGE_MASK;
        int oldScore = entry.depth - ageDiff;
        int newScore = depth;  // fresh entry has no age penalty

        shouldReplace = (newScore >= oldScore);
    }
    // --- end replacement-policy patch ---

    if (shouldReplace) {
        if (entry.key == 0)       ++entriesOccupied;
        else if (isOverwrite)     ++overwritten;

        entry.key      = zobristKey;
        entry.depth    = static_cast<uint8_t>(std::clamp(depth, 0, 255));
        entry.eval     = eval;
        entry.packedMove = packMove(bestMove);
        entry.metadata = packMetadata(flag, currentAge);
        ++actualStores;
    }
    else if (entry.key == zobristKey && bestMove.piece != NONE) {
        // still update the move if it’s a better move for same position
        entry.packedMove = packMove(bestMove);
    }
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
