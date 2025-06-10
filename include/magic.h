#pragma once
#include<cstdint>

inline int popcount(uint64_t bb) {
    return __builtin_popcountll(bb);
}

uint64_t getBishopBlockerMask(int square);
uint64_t getRookBlockerMask(int square);
uint64_t setithBlocker(uint64_t mask, int index);

uint64_t bishop_attacks(int square, uint64_t blockers);
uint64_t rook_attacks(int square, uint64_t blockers);

extern uint64_t bishopMasks[64];
extern uint64_t rookMasks[64];

extern uint64_t bishopAttackTable[64][4096];
extern uint64_t rookAttackTable[64][4096];

void initMagicTables();

uint64_t getRookAttacks(int square, uint64_t occupancy);
uint64_t getBishopAttacks(int square, uint64_t occupancy);
