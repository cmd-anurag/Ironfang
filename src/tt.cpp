#include "tt.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace {
constexpr uint16_t INVALID_PACKED_MOVE = 0xFFFF;

uint16_t packMove(const Move &move)
{
    if (move.from < 0 || move.from >= 64 || move.to < 0 || move.to >= 64)
        return INVALID_PACKED_MOVE;

    int flag = 0;
    if (move.isEnPassant) {
        flag = 1;
    } else if (move.isKingSideCastle || move.isQueenSideCastle) {
        flag = 2;
    } else if ((move.promotion & 7) == 5) {
        flag = 4;
    } else if ((move.promotion & 7) == 2) {
        flag = 5;
    } else if ((move.promotion & 7) == 4) {
        flag = 6;
    } else if ((move.promotion & 7) == 3) {
        flag = 7;
    } else if ((move.piece & 7) == 1 && std::abs(move.to - move.from) == 16) {
        flag = 3;
    }

    return static_cast<uint16_t>(move.from | (move.to << 6) | (flag << 12));
}

Move unpackMove(uint16_t packedMove)
{
    if (packedMove == INVALID_PACKED_MOVE)
        return Move(NONE, -1, -1);

    const int from = packedMove & 0x3F;
    const int to = (packedMove >> 6) & 0x3F;
    const int flag = (packedMove >> 12) & 0x0F;
    int promotionType = 0;
    if (flag == 4) promotionType = 5;
    else if (flag == 5) promotionType = 2;
    else if (flag == 6) promotionType = 4;
    else if (flag == 7) promotionType = 3;
    const bool enPassant = flag == 1;
    const bool kingSideCastle = flag == 2 && to > from;
    const bool queenSideCastle = flag == 2 && to < from;
    // Hash moves are hints only, so the piece/capture fields are unnecessary.
    return Move(NONE, from, to, promotionType, 0, kingSideCastle,
                queenSideCastle, enPassant);
}

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
                               int ply, int &outEval, Move &outMove, bool allowCutoff)
{
    ++lookupCount;

    size_t index = static_cast<size_t>(zobristKey % TT_SIZE);
    TTEntry &entry = table[index];

    // Match the reference's hash-move hint behavior: move ordering may use
    // the indexed move even when the stored key collided with this position.
    if (entry.key != 0)
        outMove = unpackMove(entry.packedMove);

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
    entry.packedMove = packMove(bestMove);
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
