#include "nnue.h"
#include "bitboard.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

bool useNNUE = false;

namespace {

constexpr int QA = 255;
constexpr int QB = 64;

alignas(64) int16_t featureWeights[NNUE_INPUTS][NNUE_HIDDEN];
alignas(64) int16_t featureBiases[NNUE_HIDDEN];
alignas(64) int16_t outputWeights[2 * NNUE_HIDDEN];
int32_t outputBias = 0;

bool networkLoaded = false;

static inline void applyPerspectiveUpdate(
    const int16_t* __restrict prev,
    int16_t* __restrict next,
    const int* removed, int numRemoved,
    const int* added, int numAdded)
{
#if defined(__AVX2__)
    const __m256i* p = reinterpret_cast<const __m256i*>(prev);
    __m256i* n = reinterpret_cast<__m256i*>(next);

    if (numRemoved == 1 && numAdded == 1) {
        const __m256i* w_sub = reinterpret_cast<const __m256i*>(featureWeights[removed[0]]);
        const __m256i* w_add = reinterpret_cast<const __m256i*>(featureWeights[added[0]]);

        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            __m256i acc0 = _mm256_load_si256(p + i + 0);
            __m256i acc1 = _mm256_load_si256(p + i + 1);
            __m256i acc2 = _mm256_load_si256(p + i + 2);
            __m256i acc3 = _mm256_load_si256(p + i + 3);

            __m256i s0 = _mm256_load_si256(w_sub + i + 0);
            __m256i s1 = _mm256_load_si256(w_sub + i + 1);
            __m256i s2 = _mm256_load_si256(w_sub + i + 2);
            __m256i s3 = _mm256_load_si256(w_sub + i + 3);

            __m256i a0 = _mm256_load_si256(w_add + i + 0);
            __m256i a1 = _mm256_load_si256(w_add + i + 1);
            __m256i a2 = _mm256_load_si256(w_add + i + 2);
            __m256i a3 = _mm256_load_si256(w_add + i + 3);

            acc0 = _mm256_add_epi16(_mm256_sub_epi16(acc0, s0), a0);
            acc1 = _mm256_add_epi16(_mm256_sub_epi16(acc1, s1), a1);
            acc2 = _mm256_add_epi16(_mm256_sub_epi16(acc2, s2), a2);
            acc3 = _mm256_add_epi16(_mm256_sub_epi16(acc3, s3), a3);

            _mm256_store_si256(n + i + 0, acc0);
            _mm256_store_si256(n + i + 1, acc1);
            _mm256_store_si256(n + i + 2, acc2);
            _mm256_store_si256(n + i + 3, acc3);
        }
    } else if (numRemoved == 2 && numAdded == 1) {
        const __m256i* w_sub0 = reinterpret_cast<const __m256i*>(featureWeights[removed[0]]);
        const __m256i* w_sub1 = reinterpret_cast<const __m256i*>(featureWeights[removed[1]]);
        const __m256i* w_add = reinterpret_cast<const __m256i*>(featureWeights[added[0]]);

        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            __m256i acc0 = _mm256_load_si256(p + i + 0);
            __m256i acc1 = _mm256_load_si256(p + i + 1);
            __m256i acc2 = _mm256_load_si256(p + i + 2);
            __m256i acc3 = _mm256_load_si256(p + i + 3);

            __m256i s0_0 = _mm256_load_si256(w_sub0 + i + 0);
            __m256i s0_1 = _mm256_load_si256(w_sub0 + i + 1);
            __m256i s0_2 = _mm256_load_si256(w_sub0 + i + 2);
            __m256i s0_3 = _mm256_load_si256(w_sub0 + i + 3);

            __m256i s1_0 = _mm256_load_si256(w_sub1 + i + 0);
            __m256i s1_1 = _mm256_load_si256(w_sub1 + i + 1);
            __m256i s1_2 = _mm256_load_si256(w_sub1 + i + 2);
            __m256i s1_3 = _mm256_load_si256(w_sub1 + i + 3);

            __m256i a0 = _mm256_load_si256(w_add + i + 0);
            __m256i a1 = _mm256_load_si256(w_add + i + 1);
            __m256i a2 = _mm256_load_si256(w_add + i + 2);
            __m256i a3 = _mm256_load_si256(w_add + i + 3);

            acc0 = _mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc0, s0_0), s1_0), a0);
            acc1 = _mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc1, s0_1), s1_1), a1);
            acc2 = _mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc2, s0_2), s1_2), a2);
            acc3 = _mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc3, s0_3), s1_3), a3);

            _mm256_store_si256(n + i + 0, acc0);
            _mm256_store_si256(n + i + 1, acc1);
            _mm256_store_si256(n + i + 2, acc2);
            _mm256_store_si256(n + i + 3, acc3);
        }
    } else {
        const __m256i* w_sub0 = reinterpret_cast<const __m256i*>(featureWeights[removed[0]]);
        const __m256i* w_sub1 = reinterpret_cast<const __m256i*>(featureWeights[removed[1]]);
        const __m256i* w_add0 = reinterpret_cast<const __m256i*>(featureWeights[added[0]]);
        const __m256i* w_add1 = reinterpret_cast<const __m256i*>(featureWeights[added[1]]);

        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            __m256i acc0 = _mm256_load_si256(p + i + 0);
            __m256i acc1 = _mm256_load_si256(p + i + 1);
            __m256i acc2 = _mm256_load_si256(p + i + 2);
            __m256i acc3 = _mm256_load_si256(p + i + 3);

            __m256i s0_0 = _mm256_load_si256(w_sub0 + i + 0);
            __m256i s0_1 = _mm256_load_si256(w_sub0 + i + 1);
            __m256i s0_2 = _mm256_load_si256(w_sub0 + i + 2);
            __m256i s0_3 = _mm256_load_si256(w_sub0 + i + 3);

            __m256i s1_0 = _mm256_load_si256(w_sub1 + i + 0);
            __m256i s1_1 = _mm256_load_si256(w_sub1 + i + 1);
            __m256i s1_2 = _mm256_load_si256(w_sub1 + i + 2);
            __m256i s1_3 = _mm256_load_si256(w_sub1 + i + 3);

            __m256i a0_0 = _mm256_load_si256(w_add0 + i + 0);
            __m256i a0_1 = _mm256_load_si256(w_add0 + i + 1);
            __m256i a0_2 = _mm256_load_si256(w_add0 + i + 2);
            __m256i a0_3 = _mm256_load_si256(w_add0 + i + 3);

            __m256i a1_0 = _mm256_load_si256(w_add1 + i + 0);
            __m256i a1_1 = _mm256_load_si256(w_add1 + i + 1);
            __m256i a1_2 = _mm256_load_si256(w_add1 + i + 2);
            __m256i a1_3 = _mm256_load_si256(w_add1 + i + 3);

            acc0 = _mm256_add_epi16(_mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc0, s0_0), s1_0), a0_0), a1_0);
            acc1 = _mm256_add_epi16(_mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc1, s0_1), s1_1), a0_1), a1_1);
            acc2 = _mm256_add_epi16(_mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc2, s0_2), s1_2), a0_2), a1_2);
            acc3 = _mm256_add_epi16(_mm256_add_epi16(_mm256_sub_epi16(_mm256_sub_epi16(acc3, s0_3), s1_3), a0_3), a1_3);

            _mm256_store_si256(n + i + 0, acc0);
            _mm256_store_si256(n + i + 1, acc1);
            _mm256_store_si256(n + i + 2, acc2);
            _mm256_store_si256(n + i + 3, acc3);
        }
    }
#else
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        int16_t val = prev[i];
        for (int r = 0; r < numRemoved; ++r) val -= featureWeights[removed[r]][i];
        for (int a = 0; a < numAdded; ++a) val += featureWeights[added[a]][i];
        next[i] = val;
    }
#endif
}

} // namespace

bool NNUE::isLoaded() {
    return networkLoaded;
}

bool NNUE::initialize() {
    if (loadFile("resources/ironfang.nnue") || loadFile("ironfang.nnue") ||
        loadFile("../resources/ironfang.nnue") || loadFile("../ironfang.nnue") ||
        loadFile("resources/nnue.bin") || loadFile("nnue.bin")) {
        useNNUE = true;
        return true;
    }
    return false;
}

bool NNUE::loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read feature transformer biases and weights
    file.read(reinterpret_cast<char*>(featureBiases), sizeof(featureBiases));
    file.read(reinterpret_cast<char*>(featureWeights), sizeof(featureWeights));

    // Read output layer weights and bias
    file.read(reinterpret_cast<char*>(outputWeights), sizeof(outputWeights));
    file.read(reinterpret_cast<char*>(&outputBias), sizeof(outputBias));

    if (!file) {
        std::cerr << "info string NNUE failed to read full network file: " << path << std::endl;
        networkLoaded = false;
        useNNUE = false;
        return false;
    }

    networkLoaded = true;
    useNNUE = true;
    std::cout << "info string NNUE loaded successfully from " << path << std::endl;
    return true;
}

void NNUE::refreshPerspective(const BitBoard& board, Accumulator& acc, Color side) {
    auto scan = [&](uint64_t bb, Piece piece, int kingSq) {
        while (bb) {
            const int sq = __builtin_ctzll(bb);
            bb &= bb - 1;
            addPiece(acc, kingSq, piece, sq, side);
        }
    };

    if (side == WHITE) {
        std::memcpy(acc.white.data(), featureBiases, sizeof(acc.white));
        const int wKing = board.whiteKingSquare;
        scan(board.getWhitePawns(), WP, wKing);
        scan(board.getWhiteKnights(), WN, wKing);
        scan(board.getWhiteBishops(), WB, wKing);
        scan(board.getWhiteRooks(), WR, wKing);
        scan(board.getWhiteQueens(), WQ, wKing);

        scan(board.getBlackPawns(), BP, wKing);
        scan(board.getBlackKnights(), BN, wKing);
        scan(board.getBlackBishops(), BB, wKing);
        scan(board.getBlackRooks(), BR, wKing);
        scan(board.getBlackQueens(), BQ, wKing);
        addPiece(acc, wKing, BK, board.blackKingSquare, WHITE);
        acc.computedWhite = true;
    } else {
        std::memcpy(acc.black.data(), featureBiases, sizeof(acc.black));
        const int bKing = board.blackKingSquare;
        scan(board.getBlackPawns(), BP, bKing);
        scan(board.getBlackKnights(), BN, bKing);
        scan(board.getBlackBishops(), BB, bKing);
        scan(board.getBlackRooks(), BR, bKing);
        scan(board.getBlackQueens(), BQ, bKing);

        scan(board.getWhitePawns(), WP, bKing);
        scan(board.getWhiteKnights(), WN, bKing);
        scan(board.getWhiteBishops(), WB, bKing);
        scan(board.getWhiteRooks(), WR, bKing);
        scan(board.getWhiteQueens(), WQ, bKing);
        addPiece(acc, bKing, WK, board.whiteKingSquare, BLACK);
        acc.computedBlack = true;
    }
}

void NNUE::refreshAccumulator(const BitBoard& board, Accumulator& acc) {
    refreshPerspective(board, acc, WHITE);
    refreshPerspective(board, acc, BLACK);
}

void NNUE::addPiece(Accumulator& acc, int kingSq, Piece piece, int square, Color side) {
    if (side == WHITE) {
        const int idx = whiteFeatureIndex(kingSq, piece, square);
        if (idx < 0) return;
#if defined(__AVX2__)
        __m256i* wAcc = reinterpret_cast<__m256i*>(acc.white.data());
        const __m256i* wW = reinterpret_cast<const __m256i*>(featureWeights[idx]);
        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            _mm256_store_si256(wAcc + i + 0, _mm256_add_epi16(_mm256_load_si256(wAcc + i + 0), _mm256_load_si256(wW + i + 0)));
            _mm256_store_si256(wAcc + i + 1, _mm256_add_epi16(_mm256_load_si256(wAcc + i + 1), _mm256_load_si256(wW + i + 1)));
            _mm256_store_si256(wAcc + i + 2, _mm256_add_epi16(_mm256_load_si256(wAcc + i + 2), _mm256_load_si256(wW + i + 2)));
            _mm256_store_si256(wAcc + i + 3, _mm256_add_epi16(_mm256_load_si256(wAcc + i + 3), _mm256_load_si256(wW + i + 3)));
        }
#else
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            acc.white[i] += featureWeights[idx][i];
        }
#endif
    } else {
        const int idx = blackFeatureIndex(kingSq, piece, square);
        if (idx < 0) return;
#if defined(__AVX2__)
        __m256i* bAcc = reinterpret_cast<__m256i*>(acc.black.data());
        const __m256i* bW = reinterpret_cast<const __m256i*>(featureWeights[idx]);
        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            _mm256_store_si256(bAcc + i + 0, _mm256_add_epi16(_mm256_load_si256(bAcc + i + 0), _mm256_load_si256(bW + i + 0)));
            _mm256_store_si256(bAcc + i + 1, _mm256_add_epi16(_mm256_load_si256(bAcc + i + 1), _mm256_load_si256(bW + i + 1)));
            _mm256_store_si256(bAcc + i + 2, _mm256_add_epi16(_mm256_load_si256(bAcc + i + 2), _mm256_load_si256(bW + i + 2)));
            _mm256_store_si256(bAcc + i + 3, _mm256_add_epi16(_mm256_load_si256(bAcc + i + 3), _mm256_load_si256(bW + i + 3)));
        }
#else
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            acc.black[i] += featureWeights[idx][i];
        }
#endif
    }
}

void NNUE::removePiece(Accumulator& acc, int kingSq, Piece piece, int square, Color side) {
    if (side == WHITE) {
        const int idx = whiteFeatureIndex(kingSq, piece, square);
        if (idx < 0) return;
#if defined(__AVX2__)
        __m256i* wAcc = reinterpret_cast<__m256i*>(acc.white.data());
        const __m256i* wW = reinterpret_cast<const __m256i*>(featureWeights[idx]);
        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            _mm256_store_si256(wAcc + i + 0, _mm256_sub_epi16(_mm256_load_si256(wAcc + i + 0), _mm256_load_si256(wW + i + 0)));
            _mm256_store_si256(wAcc + i + 1, _mm256_sub_epi16(_mm256_load_si256(wAcc + i + 1), _mm256_load_si256(wW + i + 1)));
            _mm256_store_si256(wAcc + i + 2, _mm256_sub_epi16(_mm256_load_si256(wAcc + i + 2), _mm256_load_si256(wW + i + 2)));
            _mm256_store_si256(wAcc + i + 3, _mm256_sub_epi16(_mm256_load_si256(wAcc + i + 3), _mm256_load_si256(wW + i + 3)));
        }
#else
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            acc.white[i] -= featureWeights[idx][i];
        }
#endif
    } else {
        const int idx = blackFeatureIndex(kingSq, piece, square);
        if (idx < 0) return;
#if defined(__AVX2__)
        __m256i* bAcc = reinterpret_cast<__m256i*>(acc.black.data());
        const __m256i* bW = reinterpret_cast<const __m256i*>(featureWeights[idx]);
        for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
            _mm256_store_si256(bAcc + i + 0, _mm256_sub_epi16(_mm256_load_si256(bAcc + i + 0), _mm256_load_si256(bW + i + 0)));
            _mm256_store_si256(bAcc + i + 1, _mm256_sub_epi16(_mm256_load_si256(bAcc + i + 1), _mm256_load_si256(bW + i + 1)));
            _mm256_store_si256(bAcc + i + 2, _mm256_sub_epi16(_mm256_load_si256(bAcc + i + 2), _mm256_load_si256(bW + i + 2)));
            _mm256_store_si256(bAcc + i + 3, _mm256_sub_epi16(_mm256_load_si256(bAcc + i + 3), _mm256_load_si256(bW + i + 3)));
        }
#else
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            acc.black[i] -= featureWeights[idx][i];
        }
#endif
    }
}

void NNUE::updateMove(const Accumulator& prev, Accumulator& next, const BitBoard& board, uint16_t encodedMove) {
    const int from = encodedMove & 0x3f;
    const int to = (encodedMove >> 6) & 0x3f;
    const int flag = encodedMove >> 12;
    const Piece piece = board.getPiece(from);
    const bool isWhiteMove = board.sideToMove == WHITE;
    const bool isCastle = flag == COMPACT_MOVE_CASTLE;
    const bool isEnPassant = flag == COMPACT_MOVE_EN_PASSANT;
    const bool isPromotion = flag >= COMPACT_MOVE_PROMOTE_QUEEN;
    const int captureSquare = isEnPassant ? to + (isWhiteMove ? 8 : -8) : to;
    const Piece capturedPiece = isEnPassant ? (isWhiteMove ? BP : WP) : board.getPiece(to);
    const int wKing = board.whiteKingSquare;
    const int bKing = board.blackKingSquare;

    Piece placedPiece = piece;
    if (isPromotion) {
        placedPiece = flag == COMPACT_MOVE_PROMOTE_QUEEN ? (isWhiteMove ? WQ : BQ)
            : flag == COMPACT_MOVE_PROMOTE_KNIGHT ? (isWhiteMove ? WN : BN)
            : flag == COMPACT_MOVE_PROMOTE_ROOK ? (isWhiteMove ? WR : BR)
            : (isWhiteMove ? WB : BB);
    }

    // 1. White Perspective
    if (piece == WK) {
        next.computedWhite = false;
    } else if (!prev.computedWhite) {
        next.computedWhite = false;
    } else {
        next.computedWhite = true;
        int removed[2];
        int numRemoved = 0;
        int added[2];
        int numAdded = 0;

        if (isCastle && !isWhiteMove) {
            const bool kingSide = to > from;
            const int rookFrom = kingSide ? 7 : 0;
            const int rookTo = kingSide ? 5 : 3;
            removed[0] = whiteFeatureIndex(wKing, BK, from);
            removed[1] = whiteFeatureIndex(wKing, BR, rookFrom);
            numRemoved = 2;
            added[0] = whiteFeatureIndex(wKing, BK, to);
            added[1] = whiteFeatureIndex(wKing, BR, rookTo);
            numAdded = 2;
        } else {
            removed[numRemoved++] = whiteFeatureIndex(wKing, piece, from);
            if (capturedPiece != NONE) {
                removed[numRemoved++] = whiteFeatureIndex(wKing, capturedPiece, captureSquare);
            }
            added[numAdded++] = whiteFeatureIndex(wKing, placedPiece, to);
        }

        applyPerspectiveUpdate(prev.white.data(), next.white.data(), removed, numRemoved, added, numAdded);
    }

    // 2. Black Perspective
    if (piece == BK) {
        next.computedBlack = false;
    } else if (!prev.computedBlack) {
        next.computedBlack = false;
    } else {
        next.computedBlack = true;
        int removed[2];
        int numRemoved = 0;
        int added[2];
        int numAdded = 0;

        if (isCastle && isWhiteMove) {
            const bool kingSide = to > from;
            const int rookFrom = kingSide ? 63 : 56;
            const int rookTo = kingSide ? 61 : 59;
            removed[0] = blackFeatureIndex(bKing, WK, from);
            removed[1] = blackFeatureIndex(bKing, WR, rookFrom);
            numRemoved = 2;
            added[0] = blackFeatureIndex(bKing, WK, to);
            added[1] = blackFeatureIndex(bKing, WR, rookTo);
            numAdded = 2;
        } else {
            removed[numRemoved++] = blackFeatureIndex(bKing, piece, from);
            if (capturedPiece != NONE) {
                removed[numRemoved++] = blackFeatureIndex(bKing, capturedPiece, captureSquare);
            }
            added[numAdded++] = blackFeatureIndex(bKing, placedPiece, to);
        }

        applyPerspectiveUpdate(prev.black.data(), next.black.data(), removed, numRemoved, added, numAdded);
    }
}

void NNUE::updateMove(Accumulator& acc, const BitBoard& board, uint16_t encodedMove) {
    Accumulator temp = acc;
    updateMove(temp, acc, board, encodedMove);
}

int NNUE::evaluate(const BitBoard& board, Accumulator& acc) {
    if (!acc.computedWhite) {
        refreshPerspective(board, acc, WHITE);
    }
    if (!acc.computedBlack) {
        refreshPerspective(board, acc, BLACK);
    }

    const int16_t* us = board.sideToMove == WHITE ? acc.white.data() : acc.black.data();
    const int16_t* them = board.sideToMove == WHITE ? acc.black.data() : acc.white.data();

#if defined(__AVX2__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i maxVal = _mm256_set1_epi16(QA);

    __m256i accSum0 = _mm256_setzero_si256();
    __m256i accSum1 = _mm256_setzero_si256();
    __m256i accSum2 = _mm256_setzero_si256();
    __m256i accSum3 = _mm256_setzero_si256();

    const __m256i* usVec = reinterpret_cast<const __m256i*>(us);
    const __m256i* outUs = reinterpret_cast<const __m256i*>(outputWeights);

    // Perspective 1: us (friendly pieces) - 16 vectors unrolled 4x
    for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
        __m256i in0 = _mm256_load_si256(usVec + i + 0);
        __m256i in1 = _mm256_load_si256(usVec + i + 1);
        __m256i in2 = _mm256_load_si256(usVec + i + 2);
        __m256i in3 = _mm256_load_si256(usVec + i + 3);

        __m256i c0 = _mm256_min_epi16(_mm256_max_epi16(in0, zero), maxVal);
        __m256i c1 = _mm256_min_epi16(_mm256_max_epi16(in1, zero), maxVal);
        __m256i c2 = _mm256_min_epi16(_mm256_max_epi16(in2, zero), maxVal);
        __m256i c3 = _mm256_min_epi16(_mm256_max_epi16(in3, zero), maxVal);

        __m256i w0 = _mm256_load_si256(outUs + i + 0);
        __m256i w1 = _mm256_load_si256(outUs + i + 1);
        __m256i w2 = _mm256_load_si256(outUs + i + 2);
        __m256i w3 = _mm256_load_si256(outUs + i + 3);

        accSum0 = _mm256_add_epi32(accSum0, _mm256_madd_epi16(c0, w0));
        accSum1 = _mm256_add_epi32(accSum1, _mm256_madd_epi16(c1, w1));
        accSum2 = _mm256_add_epi32(accSum2, _mm256_madd_epi16(c2, w2));
        accSum3 = _mm256_add_epi32(accSum3, _mm256_madd_epi16(c3, w3));
    }

    const __m256i* themVec = reinterpret_cast<const __m256i*>(them);
    const __m256i* outThem = reinterpret_cast<const __m256i*>(outputWeights + NNUE_HIDDEN);

    // Perspective 2: them (opponent pieces) - 16 vectors unrolled 4x
    for (int i = 0; i < NNUE_HIDDEN / 16; i += 4) {
        __m256i in0 = _mm256_load_si256(themVec + i + 0);
        __m256i in1 = _mm256_load_si256(themVec + i + 1);
        __m256i in2 = _mm256_load_si256(themVec + i + 2);
        __m256i in3 = _mm256_load_si256(themVec + i + 3);

        __m256i c0 = _mm256_min_epi16(_mm256_max_epi16(in0, zero), maxVal);
        __m256i c1 = _mm256_min_epi16(_mm256_max_epi16(in1, zero), maxVal);
        __m256i c2 = _mm256_min_epi16(_mm256_max_epi16(in2, zero), maxVal);
        __m256i c3 = _mm256_min_epi16(_mm256_max_epi16(in3, zero), maxVal);

        __m256i w0 = _mm256_load_si256(outThem + i + 0);
        __m256i w1 = _mm256_load_si256(outThem + i + 1);
        __m256i w2 = _mm256_load_si256(outThem + i + 2);
        __m256i w3 = _mm256_load_si256(outThem + i + 3);

        accSum0 = _mm256_add_epi32(accSum0, _mm256_madd_epi16(c0, w0));
        accSum1 = _mm256_add_epi32(accSum1, _mm256_madd_epi16(c1, w1));
        accSum2 = _mm256_add_epi32(accSum2, _mm256_madd_epi16(c2, w2));
        accSum3 = _mm256_add_epi32(accSum3, _mm256_madd_epi16(c3, w3));
    }

    accSum0 = _mm256_add_epi32(accSum0, accSum1);
    accSum2 = _mm256_add_epi32(accSum2, accSum3);
    accSum0 = _mm256_add_epi32(accSum0, accSum2);

    __m128i lo = _mm256_castsi256_si128(accSum0);
    __m128i hi = _mm256_extracti128_si256(accSum0, 1);
    __m128i sum128 = _mm_add_epi32(lo, hi);
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2)));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1)));
    const int32_t sum = outputBias + _mm_cvtsi128_si32(sum128);
#else
    int32_t sum = outputBias;
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        const int16_t clampedUs = std::min<int16_t>(std::max<int16_t>(us[i], 0), QA);
        sum += clampedUs * outputWeights[i];
    }
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        const int16_t clampedThem = std::min<int16_t>(std::max<int16_t>(them[i], 0), QA);
        sum += clampedThem * outputWeights[NNUE_HIDDEN + i];
    }
#endif

    const int eval = sum / (QA * QB);
    return eval;
}
