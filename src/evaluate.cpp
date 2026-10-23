#include "evaluate.h"
#include "bitboard.h"
#include "precomputedEvalTables.h"
#include <algorithm>
#include <cstdlib>


const int *PST[7] = {pawnPST, knightPST, bishopPST, rookPST, queenPST, kingMdPST, kingEgPST};



class Board;
class BitBoard;
int evaluatePawnStructure(const BitBoard &board);
int evaluateKingSafety(const BitBoard &board);

namespace {
int kingDistance(int from, int to) {
    const int fileDistance = std::abs((from & 7) - (to & 7));
    const int rankDistance = std::abs((from / 8) - (to / 8));
    return std::max(fileDistance, rankDistance);
}

int passedPawnBonus(const BitBoard &board, int square, Color pawnColor, int advancement) {
    int bonus = 30 + advancement * advancement * 5;
    const int file = square & 7;
    const int rank = square / 8;
    const int promotionSquare = pawnColor == WHITE ? file : 56 + file;
    const int movesToPromotion = pawnColor == WHITE ? rank : 7 - rank;
    const int enemyKing = pawnColor == WHITE ? board.blackKingSquare : board.whiteKingSquare;
    const int friendlyKing = pawnColor == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    const int defenderMoves = movesToPromotion - (board.sideToMove == pawnColor ? 1 : 0);

    // A pawn-only passer bonus is misleading when the defending king can
    // occupy the promotion square before the pawn arrives. Apply this only
    // when the pawn's route is clear and its own king cannot support promotion.
    bool clearRoute = true;
    const int step = pawnColor == WHITE ? -8 : 8;
    for (int target = square + step;
         pawnColor == WHITE ? target >= promotionSquare : target <= promotionSquare;
         target += step) {
        const Piece occupant = board.getPiece(target);
        if (occupant == NONE) continue;
        if (target == promotionSquare && occupant == (pawnColor == WHITE ? BK : WK)) {
            clearRoute = true;
            break;
        }
        clearRoute = false;
        break;
    }

    const int enemyDistance = kingDistance(enemyKing, promotionSquare);
    const int friendlyDistance = kingDistance(friendlyKing, promotionSquare);
    const bool enemyKingCanStop = clearRoute && defenderMoves >= 0 &&
        enemyDistance <= defenderMoves && enemyDistance < friendlyDistance &&
        !board.isSquareAttacked(promotionSquare, pawnColor);
    if (enemyKingCanStop) {
        bonus = std::min(bonus, 30);
    }

    return bonus;
}
}

int Evaluation::evaluate(const BitBoard &board) {
    int phaseMaterial = 0;
    int score = 0;
    
    // First pass: calculate phase material
    uint64_t allPieces = board.getAllPieces();
    uint64_t tempPieces = allPieces;
    while(tempPieces) {
        int square = board.popLSB(tempPieces);
        Piece p = board.getPiece(square);
        int basePiece = p & 7;
        
        if(basePiece != 1 && basePiece != 6) {
            phaseMaterial += pieceValue[basePiece];
        }
    }
    bool endgame = phaseMaterial <= 1300;
    uint64_t whitePieces = board.getWhitePieces();
    uint64_t blackPieces = board.getBlackPieces();
    uint64_t occupied = allPieces;
    
    // Evaluate white pieces
    while(whitePieces) {
        int square = board.popLSB(whitePieces);
        Piece p = board.getPiece(square);
        int basePiece = p & 7;
        
        if(endgame && basePiece == 6) {
            score += pieceValue[basePiece] + PST[basePiece][square];    
        }
        else {
            score += pieceValue[basePiece] + PST[basePiece-1][square];
        }
    }
    
    // Evaluate black pieces
    while(blackPieces) {
        int square = board.popLSB(blackPieces);
        Piece p = board.getPiece(square);
        int basePiece = p & 7;
        
        if(endgame && basePiece == 6) {
            score -= pieceValue[basePiece] + PST[basePiece][mirror(square)];    
        }
        else {
            score -= pieceValue[basePiece] + PST[basePiece - 1][mirror(square)];
        }
    }

    // Reward useful piece mobility while excluding friendly blockers.
    uint64_t movers = board.getWhiteKnights();
    while (movers) {
        int square = board.popLSB(movers);
        score += std::min(8, __builtin_popcountll(board.getKnightAttacks(square) & ~board.getWhitePieces())) * 2;
    }
    movers = board.getBlackKnights();
    while (movers) {
        int square = board.popLSB(movers);
        score -= std::min(8, __builtin_popcountll(board.getKnightAttacks(square) & ~board.getBlackPieces())) * 2;
    }
    movers = board.getWhiteBishops();
    while (movers) {
        int square = board.popLSB(movers);
        score += std::min(13, __builtin_popcountll(board.getBishopAttacks(square, occupied) & ~board.getWhitePieces())) * 2;
    }
    movers = board.getBlackBishops();
    while (movers) {
        int square = board.popLSB(movers);
        score -= std::min(13, __builtin_popcountll(board.getBishopAttacks(square, occupied) & ~board.getBlackPieces())) * 2;
    }
    movers = board.getWhiteRooks();
    while (movers) {
        int square = board.popLSB(movers);
        score += std::min(14, __builtin_popcountll(board.getRookAttacks(square, occupied) & ~board.getWhitePieces()));
    }
    movers = board.getBlackRooks();
    while (movers) {
        int square = board.popLSB(movers);
        score -= std::min(14, __builtin_popcountll(board.getRookAttacks(square, occupied) & ~board.getBlackPieces()));
    }

    // A bishop pair is a long-term asset, and rooks belong on open files.
    if (__builtin_popcountll(board.getWhiteBishops()) >= 2) score += 30;
    if (__builtin_popcountll(board.getBlackBishops()) >= 2) score -= 30;
    auto rookFileBonus = [&](uint64_t rooks, uint64_t ownPawns, uint64_t enemyPawns, int sign) {
        uint64_t temp = rooks;
        while (temp) {
            int square = board.popLSB(temp);
            uint64_t file = FILES[square & 7];
            if (!(ownPawns & file)) score += sign * ((enemyPawns & file) ? 10 : 18);
            if ((sign > 0 && square / 8 == 1) || (sign < 0 && square / 8 == 6))
                score += sign * 16;
        }
    };
    rookFileBonus(board.getWhiteRooks(), board.getWhitePawns(), board.getBlackPawns(), 1);
    rookFileBonus(board.getBlackRooks(), board.getBlackPawns(), board.getWhitePawns(), -1);

    // Pawn Structure
    score += evaluatePawnStructure(board);

    // King Safety (only in middlegame)
    if (!endgame) {
        score += evaluateKingSafety(board);
    }

    return (board.sideToMove == WHITE) ? score : -score;
}

int evaluateKingSafety(const BitBoard &board) {
    int score = 0;

    // Reward a king that has reached a protected castled position in the middlegame.
    int whiteKingFile = board.whiteKingSquare & 7;
    if (board.whiteKingSquare / 8 == 7 && (whiteKingFile == 2 || whiteKingFile == 6))
        score += 25;
    int blackKingFile = board.blackKingSquare & 7;
    if (board.blackKingSquare / 8 == 0 && (blackKingFile == 2 || blackKingFile == 6))
        score -= 25;
    
    // Evaluate white king safety
    
    // Pawn shield: check for pawns in front of king
    int shieldBonus = 0;
    for (int f = whiteKingFile - 1; f <= whiteKingFile + 1; ++f) {
        if (f >= 0 && f < 8) {
            uint64_t fileSquares = FILES[f] & board.getWhitePawns();
            if (fileSquares) {
                // Find closest pawn to king
                int pawnSquare = __builtin_clzll(fileSquares | 1) ^ 63;
                int pawnRank = pawnSquare / 8;
                int kingRank = board.whiteKingSquare / 8;
                
                if (pawnRank < kingRank) { // White pawns advance toward lower row indices
                    int distance = kingRank - pawnRank;
                    if (distance == 1) shieldBonus += 20;      // Pawn right in front
                    else if (distance == 2) shieldBonus += 10; // One square gap
                }
            } else {
                shieldBonus -= 25; // Missing pawn in shield
            }
        }
    }
    score += shieldBonus;
    
    // Open files near king penalty
    int openFilePenalty = 0;
    for (int f = whiteKingFile - 1; f <= whiteKingFile + 1; ++f) {
        if (f >= 0 && f < 8) {
            uint64_t allPawnsOnFile = (board.getWhitePawns() | board.getBlackPawns()) & FILES[f];
            if (!allPawnsOnFile) {
                openFilePenalty += (f == whiteKingFile) ? 30 : 15; // King file more dangerous
            }
        }
    }
    score -= openFilePenalty;
    
    // Evaluate black king safety (same logic, mirrored)
    shieldBonus = 0;
    for (int f = blackKingFile - 1; f <= blackKingFile + 1; ++f) {
        if (f >= 0 && f < 8) {
            uint64_t fileSquares = FILES[f] & board.getBlackPawns();
            if (fileSquares) {
                int pawnSquare = __builtin_ctzll(fileSquares);
                int pawnRank = pawnSquare / 8;
                int kingRank = board.blackKingSquare / 8;
                
                if (pawnRank > kingRank) { // Black pawns advance toward higher row indices
                    int distance = pawnRank - kingRank;
                    if (distance == 1) shieldBonus += 20;
                    else if (distance == 2) shieldBonus += 10;
                }
            } else {
                shieldBonus -= 25;
            }
        }
    }
    score -= shieldBonus; // Subtract because this is black's bonus
    
    openFilePenalty = 0;
    for (int f = blackKingFile - 1; f <= blackKingFile + 1; ++f) {
        if (f >= 0 && f < 8) {
            uint64_t allPawnsOnFile = (board.getWhitePawns() | board.getBlackPawns()) & FILES[f];
            if (!allPawnsOnFile) {
                openFilePenalty += (f == blackKingFile) ? 30 : 15;
            }
        }
    }
    score += openFilePenalty; // Add because this hurts black
    
    return score;
}

int evaluatePawnStructure(const BitBoard &board) {
    int score = 0;
    const uint64_t white_pawns = board.whitePawns;
    const uint64_t black_pawns = board.blackPawns;

    // Counters for pawn features
    int whiteIsolatedCount = 0, whiteDoubledCount = 0, whitePassedScore = 0;
    int blackIsolatedCount = 0, blackDoubledCount = 0, blackPassedScore = 0;

    // 1) Doubled and Isolated pawns by file
    for (int file = 0; file < 8; ++file) {
        uint64_t whitePawnsOnFile = white_pawns & FILES[file];
        uint64_t blackPawnsOnFile = black_pawns & FILES[file];

        int whitePawnCount = __builtin_popcountll(whitePawnsOnFile);
        if (whitePawnCount > 1) whiteDoubledCount += (whitePawnCount - 1);
        bool whiteIsolated = (white_pawns & ADJACENT_FILES[file]) == 0;
        if (whiteIsolated) whiteIsolatedCount += whitePawnCount;

        int blackPawnCount = __builtin_popcountll(blackPawnsOnFile);
        if (blackPawnCount > 1) blackDoubledCount += (blackPawnCount - 1);
        bool blackIsolated = (black_pawns & ADJACENT_FILES[file]) == 0;
        if (blackIsolated) blackIsolatedCount += blackPawnCount;
    }

    // 2) Passed pawns by square loop
    uint64_t tempPawns;

    // White passed pawns
    tempPawns = white_pawns;
    while (tempPawns) {
        int square = __builtin_ctzll(tempPawns);
        tempPawns &= tempPawns - 1;
        int file = square & 7;
        // White advances toward decreasing square indices in this board map.
        uint64_t span = SPAN_SOUTH[square] & (FILES[file] | ADJACENT_FILES[file]);
        if ((black_pawns & span) == 0) {
            int advancement = 6 - square / 8;
            whitePassedScore += passedPawnBonus(board, square, WHITE, advancement);
        }
    }

    // Black passed pawns
    tempPawns = black_pawns;
    while (tempPawns) {
        int square = __builtin_ctzll(tempPawns);
        tempPawns &= tempPawns - 1;
        int file = square & 7;
        // Black advances toward increasing square indices in this board map.
        uint64_t span = SPAN_NORTH[square] & (FILES[file] | ADJACENT_FILES[file]);
        if ((white_pawns & span) == 0) {
            int advancement = square / 8 - 1;
            blackPassedScore += passedPawnBonus(board, square, BLACK, advancement);
        }
    }

    // 3) Aggregate score (from White's perspective)
    score += whitePassedScore;
    score -= whiteIsolatedCount * 25;
    score -= whiteDoubledCount * 15;

    score -= blackPassedScore;
    score += blackIsolatedCount * 25;
    score += blackDoubledCount * 15;

    return score;
}
