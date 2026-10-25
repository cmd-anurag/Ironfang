#include "evaluate.h"
#include "bitboard.h"
#include "precomputedEvalTables.h"

#include <algorithm>
#include <array>
#include <cstdlib>

const int *PST[7] = {
    pawnPST,
    knightPST,
    bishopPST,
    rookPST,
    queenPST,
    kingMdPST,
    kingEgPST
};

namespace {
constexpr int passedPawnBonuses[7] = {0, 120, 80, 50, 30, 15, 15};
constexpr int isolatedPawnPenaltyByCount[9] = {0, -10, -25, -50, -75, -75, -75, -75, -75};
constexpr int kingPawnShieldScores[6] = {4, 7, 4, 3, 6, 3};
constexpr int endgameStartWeight = 2 * 20 + 2 * 10 + 2 * 10 + 45;

struct MaterialInfo {
    int materialScore = 0;
    int numPawns = 0;
    int numKnights = 0;
    int numBishops = 0;
    int numRooks = 0;
    int numQueens = 0;
    uint64_t pawns = 0;
    uint64_t enemyPawns = 0;
    float endgameT = 0.0f;
};

uint64_t pawnsFor(const BitBoard& board, Color side) {
    return side == WHITE ? board.getWhitePawns() : board.getBlackPawns();
}

MaterialInfo getMaterialInfo(const BitBoard& board, Color side) {
    const uint64_t pawns = pawnsFor(board, side);
    const uint64_t knights = side == WHITE ? board.getWhiteKnights() : board.getBlackKnights();
    const uint64_t bishops = side == WHITE ? board.getWhiteBishops() : board.getBlackBishops();
    const uint64_t rooks = side == WHITE ? board.getWhiteRooks() : board.getBlackRooks();
    const uint64_t queens = side == WHITE ? board.getWhiteQueens() : board.getBlackQueens();

    MaterialInfo info;
    info.numPawns = __builtin_popcountll(pawns);
    info.numKnights = __builtin_popcountll(knights);
    info.numBishops = __builtin_popcountll(bishops);
    info.numRooks = __builtin_popcountll(rooks);
    info.numQueens = __builtin_popcountll(queens);
    info.pawns = pawns;
    info.enemyPawns = pawnsFor(board, side == WHITE ? BLACK : WHITE);
    info.materialScore =
        info.numPawns * Evaluation::pieceValue[1] +
        info.numKnights * Evaluation::pieceValue[2] +
        info.numBishops * Evaluation::pieceValue[3] +
        info.numRooks * Evaluation::pieceValue[4] +
        info.numQueens * Evaluation::pieceValue[5];

    constexpr int queenEndgameWeight = 45;
    constexpr int rookEndgameWeight = 20;
    constexpr int bishopEndgameWeight = 10;
    constexpr int knightEndgameWeight = 10;
    const int endgameWeightSum =
        info.numQueens * queenEndgameWeight +
        info.numRooks * rookEndgameWeight +
        info.numBishops * bishopEndgameWeight +
        info.numKnights * knightEndgameWeight;
    info.endgameT = 1.0f - std::min(1.0f,
        endgameWeightSum / static_cast<float>(endgameStartWeight));
    return info;
}

int pieceSquareScore(const BitBoard& board, Color side, float endgameT) {
    uint64_t pieces = side == WHITE ? board.getWhitePieces() : board.getBlackPieces();
    int score = 0;
    int pawnEarlyScore = 0;
    int pawnLateScore = 0;

    while (pieces) {
        const int square = board.popLSB(pieces);
        const Piece piece = board.getPiece(square);
        const int pieceType = piece & 7;
        const int tableSquare = side == WHITE ? square : Evaluation::mirror(square);

        if (pieceType == 1) {
            pawnEarlyScore += pawnPST[tableSquare];
            pawnLateScore += pawnEgPST[tableSquare];
        } else if (pieceType == 6) {
            const int early = kingMdPST[tableSquare];
            const int late = kingEgPST[tableSquare];
            score += static_cast<int>(early * (1.0f - endgameT));
            score += static_cast<int>(late * endgameT);
        } else if (pieceType >= 2 && pieceType <= 5) {
            score += PST[pieceType - 1][tableSquare];
        }
    }

    // The reference sums each pawn table first, then applies the phase
    // weights and truncates once per phase. Truncating per pawn changes the
    // evaluator by several centipawns in multi-pawn positions.
    score += static_cast<int>(pawnEarlyScore * (1.0f - endgameT));
    score += static_cast<int>(pawnLateScore * endgameT);

    return score;
}

int mopUpScore(const BitBoard& board, Color side,
               const MaterialInfo& ownMaterial, const MaterialInfo& enemyMaterial) {
    if (ownMaterial.materialScore <= enemyMaterial.materialScore + 2 * Evaluation::pieceValue[1] ||
        enemyMaterial.endgameT <= 0.0f) {
        return 0;
    }

    const int ownKing = side == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    const int enemyKing = side == WHITE ? board.blackKingSquare : board.whiteKingSquare;
    const int fileDistance = std::abs((ownKing & 7) - (enemyKing & 7));
    const int rankDistance = std::abs((ownKing / 8) - (enemyKing / 8));
    const int orthogonalDistance = fileDistance + rankDistance;
    const int enemyFile = enemyKing & 7;
    const int enemyRank = enemyKing / 8;
    const int distanceFromCentre =
        std::max(3 - enemyFile, enemyFile - 4) +
        std::max(3 - enemyRank, enemyRank - 4);

    const int score = (14 - orthogonalDistance) * 4 + distanceFromCentre * 10;
    return static_cast<int>(score * enemyMaterial.endgameT);
}

int pawnScore(const BitBoard& board, Color side) {
    const uint64_t ownPawns = pawnsFor(board, side);
    const uint64_t enemyPawns = pawnsFor(board, side == WHITE ? BLACK : WHITE);
    const uint64_t* passedSpan = side == WHITE ? SPAN_SOUTH : SPAN_NORTH;
    int passedBonus = 0;
    int isolatedCount = 0;
    uint64_t pawns = ownPawns;
    while (pawns) {
        const int square = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        const int file = square & 7;
        const uint64_t forwardSameFile = passedSpan[square];
        const uint64_t forwardAdjacentFiles =
            ((forwardSameFile & ~FILES[0]) >> 1) |
            ((forwardSameFile & ~FILES[7]) << 1);
        const uint64_t passedMask = forwardSameFile | forwardAdjacentFiles;
        if ((enemyPawns & passedMask) == 0) {
            const int boardRank = square / 8;
            const int squaresFromPromotion = side == WHITE ? boardRank : 7 - boardRank;
            const int bonusIndex = std::clamp(squaresFromPromotion, 0, 6);
            passedBonus += passedPawnBonuses[bonusIndex];
        }

        if ((ownPawns & ADJACENT_FILES[file]) == 0) {
            ++isolatedCount;
        }
    }

    return passedBonus + isolatedPawnPenaltyByCount[std::min(isolatedCount, 8)];
}

int kingPawnShield(const BitBoard& board, Color kingSide,
                   const MaterialInfo& attackingMaterial, int enemyPieceSquareScore) {
    if (attackingMaterial.endgameT >= 1.0f) {
        return 0;
    }

    const int kingSquare = kingSide == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    const int kingFile = kingSquare & 7;
    const int kingRank = kingSquare / 8;
    const int ownPawn = kingSide == WHITE ? WP : BP;
    int shieldPenalty = 0;
    int uncastledKingPenalty = 0;
    if (kingFile <= 2 || kingFile >= 5) {
        std::array<int, 6> shieldSquares{};
        int shieldSquareCount = 0;
        const int direction = kingSide == WHITE ? -1 : 1;
        const int clampedFile = std::clamp(kingFile, 1, 6);

        for (int fileOffset = -1; fileOffset <= 1; ++fileOffset) {
            const int file = clampedFile + fileOffset;
            const int rank = kingRank + direction;
            if (file >= 0 && file < 8 && rank >= 0 && rank < 8)
                shieldSquares[shieldSquareCount++] = rank * 8 + file;
        }
        for (int fileOffset = -1; fileOffset <= 1; ++fileOffset) {
            const int file = clampedFile + fileOffset;
            const int rank = kingRank + 2 * direction;
            if (file >= 0 && file < 8 && rank >= 0 && rank < 8)
                shieldSquares[shieldSquareCount++] = rank * 8 + file;
        }

        for (int i = 0; i < shieldSquareCount / 2; ++i) {
            if (board.getPiece(shieldSquares[i]) != ownPawn) {
                if (shieldSquareCount > 3 && i + 3 < shieldSquareCount &&
                    board.getPiece(shieldSquares[i + 3]) == ownPawn) {
                    shieldPenalty += kingPawnShieldScores[i + 3];
                } else {
                    shieldPenalty += kingPawnShieldScores[i];
                }
            }
        }
        shieldPenalty *= shieldPenalty;
    } else {
        const bool canStillCastle = kingSide == WHITE
            ? board.whiteKingsideCastle || board.whiteQueensideCastle
            : board.blackKingsideCastle || board.blackQueensideCastle;
        if (canStillCastle) {
            // This incentive is for keeping castling available. Once both
            // rights are gone, don't reward a king walk toward an unavailable
            // corner; assess its shelter and file exposure below instead.
            const float enemyDevelopmentScore = std::clamp(
                (enemyPieceSquareScore + 10) / 130.0f, 0.0f, 1.0f);
            uncastledKingPenalty = static_cast<int>(50 * enemyDevelopmentScore);
        }
    }

    int openFilePenalty = 0;
    if (attackingMaterial.numRooks > 1 ||
        (attackingMaterial.numRooks > 0 && attackingMaterial.numQueens > 0)) {
        const int clampedKingFile = std::clamp(kingFile, 1, 6);
        for (int attackFile = clampedKingFile; attackFile <= clampedKingFile + 1; ++attackFile) {
            const uint64_t fileMask = FILES[attackFile];
            const bool isKingFile = attackFile == kingFile;
            if ((attackingMaterial.pawns & fileMask) == 0) {
                openFilePenalty += isKingFile ? 25 : 15;
                if ((attackingMaterial.enemyPawns & fileMask) == 0)
                    openFilePenalty += isKingFile ? 15 : 10;
            }
        }
    }

    float weight = 1.0f - attackingMaterial.endgameT;
    if (attackingMaterial.numQueens == 0) {
        weight *= 0.6f;
    }

    return static_cast<int>(
        (-shieldPenalty - uncastledKingPenalty - openFilePenalty) * weight);
}
}

namespace {
template<bool CaptureBreakdown>
int evaluateScore(const BitBoard& board,
                  [[maybe_unused]] Evaluation::Breakdown* breakdown) {
    const MaterialInfo whiteMaterial = getMaterialInfo(board, WHITE);
    const MaterialInfo blackMaterial = getMaterialInfo(board, BLACK);

    // Match the reference evaluator: each side's piece-square phase is based
    // on the opposing side's non-pawn material.
    const int whitePieceSquare = pieceSquareScore(board, WHITE, blackMaterial.endgameT);
    const int blackPieceSquare = pieceSquareScore(board, BLACK, whiteMaterial.endgameT);

    const int whiteMopUp = mopUpScore(board, WHITE, whiteMaterial, blackMaterial);
    const int whitePawn = pawnScore(board, WHITE);
    const int whiteKingSafety = kingPawnShield(board, WHITE, blackMaterial, blackPieceSquare);
    const int blackMopUp = mopUpScore(board, BLACK, blackMaterial, whiteMaterial);
    const int blackPawn = pawnScore(board, BLACK);
    const int blackKingSafety = kingPawnShield(board, BLACK, whiteMaterial, whitePieceSquare);

    const int whiteScore = whiteMaterial.materialScore + whitePieceSquare +
        whiteMopUp + whitePawn + whiteKingSafety;
    const int blackScore = blackMaterial.materialScore + blackPieceSquare +
        blackMopUp + blackPawn + blackKingSafety;
    const int score = board.sideToMove == WHITE ? whiteScore - blackScore : blackScore - whiteScore;

    if constexpr (CaptureBreakdown) {
        breakdown->whiteMaterial = whiteMaterial.materialScore;
        breakdown->whitePieceSquare = whitePieceSquare;
        breakdown->whiteMopUp = whiteMopUp;
        breakdown->whitePawn = whitePawn;
        breakdown->whiteKingSafety = whiteKingSafety;
        breakdown->blackMaterial = blackMaterial.materialScore;
        breakdown->blackPieceSquare = blackPieceSquare;
        breakdown->blackMopUp = blackMopUp;
        breakdown->blackPawn = blackPawn;
        breakdown->blackKingSafety = blackKingSafety;
        breakdown->score = score;
    }
    return score;
}
}

int Evaluation::evaluate(const BitBoard& board) {
    return evaluateScore<false>(board, nullptr);
}

Evaluation::Breakdown Evaluation::evaluateBreakdown(const BitBoard& board) {
    Breakdown breakdown;
    evaluateScore<true>(board, &breakdown);
    return breakdown;
}
