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
        for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
            wAcc[i] = _mm256_add_epi16(wAcc[i], wW[i]);
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
        for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
            bAcc[i] = _mm256_add_epi16(bAcc[i], bW[i]);
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
        for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
            wAcc[i] = _mm256_sub_epi16(wAcc[i], wW[i]);
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
        for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
            bAcc[i] = _mm256_sub_epi16(bAcc[i], bW[i]);
        }
#else
        for (int i = 0; i < NNUE_HIDDEN; ++i) {
            acc.black[i] -= featureWeights[idx][i];
        }
#endif
    }
}

void NNUE::updateMove(Accumulator& acc, const BitBoard& board, uint16_t encodedMove) {
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

    // 1. King Moves: lazy refresh for the moving king's perspective;
    // for the opponent perspective, the king is simply an enemy piece moving.
    if (piece == WK) {
        acc.computedWhite = false;
        removePiece(acc, bKing, WK, from, BLACK);
        addPiece(acc, bKing, WK, to, BLACK);
        if (capturedPiece != NONE) {
            removePiece(acc, bKing, capturedPiece, captureSquare, BLACK);
        }
        return;
    }
    if (piece == BK) {
        acc.computedBlack = false;
        removePiece(acc, wKing, BK, from, WHITE);
        addPiece(acc, wKing, BK, to, WHITE);
        if (capturedPiece != NONE) {
            removePiece(acc, wKing, capturedPiece, captureSquare, WHITE);
        }
        return;
    }

    // 2. Normal Piece Moves (Pawn, Knight, Bishop, Rook, Queen):
    removePiece(acc, wKing, piece, from, WHITE);
    removePiece(acc, bKing, piece, from, BLACK);

    if (capturedPiece != NONE) {
        removePiece(acc, wKing, capturedPiece, captureSquare, WHITE);
        removePiece(acc, bKing, capturedPiece, captureSquare, BLACK);
    }

    Piece placedPiece = piece;
    if (isPromotion) {
        placedPiece = flag == COMPACT_MOVE_PROMOTE_QUEEN ? (isWhiteMove ? WQ : BQ)
            : flag == COMPACT_MOVE_PROMOTE_KNIGHT ? (isWhiteMove ? WN : BN)
            : flag == COMPACT_MOVE_PROMOTE_ROOK ? (isWhiteMove ? WR : BR)
            : (isWhiteMove ? WB : BB);
    }

    if (isCastle) {
        const bool kingSide = to > from;
        const int rookFrom = isWhiteMove ? (kingSide ? 63 : 56) : (kingSide ? 7 : 0);
        const int rookTo = isWhiteMove ? (kingSide ? 61 : 59) : (kingSide ? 5 : 3);
        const Piece rook = isWhiteMove ? WR : BR;

        if (isWhiteMove) {
            acc.computedWhite = false;
            removePiece(acc, bKing, WK, from, BLACK);
            addPiece(acc, bKing, WK, to, BLACK);
            removePiece(acc, bKing, rook, rookFrom, BLACK);
            addPiece(acc, bKing, rook, rookTo, BLACK);
        } else {
            acc.computedBlack = false;
            removePiece(acc, wKing, BK, from, WHITE);
            addPiece(acc, wKing, BK, to, WHITE);
            removePiece(acc, wKing, rook, rookFrom, WHITE);
            addPiece(acc, wKing, rook, rookTo, WHITE);
        }
    } else {
        addPiece(acc, wKing, placedPiece, to, WHITE);
        addPiece(acc, bKing, placedPiece, to, BLACK);
    }
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

    int32_t sum = outputBias;

#if defined(__AVX2__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i maxVal = _mm256_set1_epi16(QA);
    __m256i accSum = _mm256_setzero_si256();

    // Perspective 1: us (friendly pieces)
    for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
        const __m256i in = _mm256_load_si256(reinterpret_cast<const __m256i*>(us) + i);
        const __m256i clamped = _mm256_min_epi16(_mm256_max_epi16(in, zero), maxVal);
        const __m256i weight = _mm256_load_si256(reinterpret_cast<const __m256i*>(outputWeights) + i);
        accSum = _mm256_add_epi32(accSum, _mm256_madd_epi16(clamped, weight));
    }

    // Perspective 2: them (opponent pieces)
    for (int i = 0; i < NNUE_HIDDEN / 16; ++i) {
        const __m256i in = _mm256_load_si256(reinterpret_cast<const __m256i*>(them) + i);
        const __m256i clamped = _mm256_min_epi16(_mm256_max_epi16(in, zero), maxVal);
        const __m256i weight = _mm256_load_si256(reinterpret_cast<const __m256i*>(outputWeights + NNUE_HIDDEN) + i);
        accSum = _mm256_add_epi32(accSum, _mm256_madd_epi16(clamped, weight));
    }

    alignas(32) int32_t lanes[8];
    _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), accSum);
    for (int i = 0; i < 8; ++i) {
        sum += lanes[i];
    }
#else
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
