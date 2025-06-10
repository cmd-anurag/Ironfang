#include "magic.h"
#include "precomputedMagics.h"
#include <cstring>
#include <cassert>

uint64_t bishopMasks[64];
uint64_t rookMasks[64];

uint64_t bishopAttackTable[64][4096];
uint64_t rookAttackTable  [64][4096];

uint64_t getBishopBlockerMask(int square);
uint64_t getRookBlockerMask(int square);
uint64_t setithBlocker(uint64_t mask, int index);
uint64_t bishop_attacks(int square, uint64_t blockers);
uint64_t rook_attacks(int square, uint64_t blockers);

void initMagicTables() {
    // Zero tables
    memset(bishopAttackTable, 0, sizeof(bishopAttackTable));
    memset(rookAttackTable,   0, sizeof(rookAttackTable));

    for (int sq = 0; sq < 64; ++sq) {
        // 1) compute and store masks
        bishopMasks[sq] = getBishopBlockerMask(sq);
        rookMasks[sq]   = getRookBlockerMask(sq);

        // 2) count relevant bits
        int bishopBits = __builtin_popcountll(bishopMasks[sq]);
        int rookBits   = __builtin_popcountll(rookMasks[sq]);

        int bishopCount = 1 << bishopBits;
        int rookCount   = 1 << rookBits;

        // 3) for each blocker permutation, fill attack table
        for (int idx = 0; idx < bishopCount; ++idx) {
            uint64_t blockers = setithBlocker(bishopMasks[sq], idx);
            uint64_t attacks  = bishop_attacks(sq, blockers);

            // magic hash
            int magicIndex = (int)((blockers * bishopMagics[sq]) >> bishopShifts[sq]);
            bishopAttackTable[sq][magicIndex] = attacks;
        }

        for (int idx = 0; idx < rookCount; ++idx) {
            uint64_t blockers = setithBlocker(rookMasks[sq], idx);
            uint64_t attacks  = rook_attacks(sq, blockers);

            int magicIndex = (int)((blockers * rookMagics[sq]) >> rookShifts[sq]);
            rookAttackTable[sq][magicIndex] = attacks;
        }
    }
}

uint64_t getRookAttacks(int square, uint64_t occupancy) {
    uint64_t blockers = occupancy & rookMasks[square];
    int index = (int)((blockers * rookMagics[square]) >> rookShifts[square]);
    return rookAttackTable[square][index];
}

uint64_t getBishopAttacks(int square, uint64_t occupancy) {
    uint64_t blockers = occupancy & bishopMasks[square];
    int index = (int)((blockers * bishopMagics[square]) >> bishopShifts[square]);
    return bishopAttackTable[square][index];
}

uint64_t getBishopBlockerMask(int square)
{
    int rank = square / 8;
    int file = square % 8;
    uint64_t mask = 0ULL;

    for (int dr = -1; dr <= 1; dr += 2) {
        for (int df = -1; df <= 1; df += 2) {
            int r = rank + dr;
            int f = file + df;
            while (r > 0 && r < 7 && f > 0 && f < 7) { // exclude edge squares
                mask |= (1ULL << (r * 8 + f));
                r += dr;
                f += df;
            }
        }
    }

    return mask;
}

uint64_t getRookBlockerMask(int square)
{
    int rank = square / 8;
    int file = square % 8;
    uint64_t mask = 0ULL;

    // Horizontal (rank)
    for (int f = file + 1; f <= 6; f++) mask |= (1ULL << (rank * 8 + f));
    for (int f = file - 1; f >= 1; f--) mask |= (1ULL << (rank * 8 + f));

    // Vertical (file)
    for (int r = rank + 1; r <= 6; r++) mask |= (1ULL << (r * 8 + file));
    for (int r = rank - 1; r >= 1; r--) mask |= (1ULL << (r * 8 + file));

    return mask;
}

uint64_t setithBlocker(uint64_t mask, int index)
{
    uint64_t result = 0ULL;

    int numBits = popcount(mask);
    for (int i = 0; i < numBits; ++i) {
        int bit = __builtin_ctzll(mask); // get index of least significant 1-bit
        mask &= mask - 1;                // clear that bit

        if (index & (1 << i))
            result |= (1ULL << bit);
    }

    return result;
}

uint64_t bishop_attacks(int square, uint64_t blockers) {
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int dr = -1; dr <= 1; dr += 2) {
        for (int df = -1; df <= 1; df += 2) {
            int r = rank + dr;
            int f = file + df;

            while (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
                int sq = r * 8 + f;
                attacks |= (1ULL << sq);
                if (blockers & (1ULL << sq)) break;
                r += dr;
                f += df;
            }
        }
    }

    return attacks;
}

uint64_t rook_attacks(int square, uint64_t blockers) {
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int d = -1; d <= 1; d += 2) {
        for (int r = rank + d; r >= 0 && r <= 7; r += d) {
            int sq = r * 8 + file;
            attacks |= (1ULL << sq);
            if (blockers & (1ULL << sq)) break;
        }
        for (int f = file + d; f >= 0 && f <= 7; f += d) {
            int sq = rank * 8 + f;
            attacks |= (1ULL << sq);
            if (blockers & (1ULL << sq)) break;
        }
    }

    return attacks;
}
