#include "evaluate.h"
#include "bitboard.h"
#include "precomputedEvalTables.h"
#include <unordered_map>


const int *PST[7] = {pawnPST, rookPST, knightPST, bishopPST, queenPST, kingMdPST, kingEgPST};



class Board;
class BitBoard;
int evaluatePawnStructure(uint64_t white_pawns, uint64_t black_pawns);
int evaluateKingSafety(const BitBoard &board);

int Evaluation::evaluate(const BitBoard &board) {
    int phaseMaterial = 0;
    int midgameScore = 0;
    int endgameScore = 0;
    
    // First pass: calculate phase material
    uint64_t allPieces = board.getAllPieces();
    uint64_t tempPieces = allPieces;
    while(tempPieces) {
        int square = board.popLSB(tempPieces);
        Piece piece = board.getPiece(square);
        int basePiece = piece & 7;
        
        if(basePiece != 1 && basePiece != 6) { // Not pawn or king
            phaseMaterial += pieceValue[basePiece];
        }
    }
    
    // Define phase boundaries
    const int totalPhase = 24;
    const int endgamePhase = 0;
    // const int midgamePhase = totalPhase;
    
    // Calculate current phase
    int currentPhase = (phaseMaterial * totalPhase) / 6400;
    currentPhase = std::min(currentPhase, totalPhase);
    currentPhase = std::max(currentPhase, endgamePhase);
    
    // Evaluate white pieces
    uint64_t whitePieces = board.getWhitePieces();
    while(whitePieces) {
        int square = board.popLSB(whitePieces);
        Piece piece = board.getPiece(square);
        int basePiece = piece & 7;
        
        // Material value
        int materialValue = pieceValue[basePiece];
        midgameScore += materialValue;
        endgameScore += materialValue;
        
        // Positional value
        if(basePiece == 6) { // King
            midgameScore += PST[5][square];
            endgameScore += PST[6][square];
        } else {
            int positionalValue = PST[basePiece-1][square];
            midgameScore += positionalValue;
            endgameScore += positionalValue;
        }
    }
    
    // Evaluate black pieces
    uint64_t blackPieces = board.getBlackPieces();
    while(blackPieces) {
        int square = board.popLSB(blackPieces);
        Piece piece = board.getPiece(square);
        int basePiece = piece & 7;
        
        // Material value
        int materialValue = pieceValue[basePiece];
        midgameScore -= materialValue;
        endgameScore -= materialValue;
        
        // Positional value
        if(basePiece == 6) { // King
            midgameScore -= PST[5][mirror(square)];
            endgameScore -= PST[6][mirror(square)];
        } else {
            int positionalValue = PST[basePiece-1][mirror(square)];
            midgameScore -= positionalValue;
            endgameScore -= positionalValue;
        }
    }

    // Pawn Structure
    int pawnScore = evaluatePawnStructure(board.whitePawns, board.blackPawns);
    midgameScore += pawnScore;
    endgameScore += pawnScore;

    // King Safety
    int kingSafetyPacked = evaluateKingSafety(board);
    int kingSafetyMidgame = kingSafetyPacked >> 16;
    int kingSafetyEndgame = (int16_t)(kingSafetyPacked & 0xFFFF);

    midgameScore += kingSafetyMidgame;
    endgameScore += kingSafetyEndgame;

    // Taper the scores based on game phase
    int taperedScore = (midgameScore * currentPhase + endgameScore * (totalPhase - currentPhase)) / totalPhase;

    // Draw penalty
    int occurrences = 0;
    for (int i = 0; i < board.pathDepth; ++i) {
        if (board.repetitionPath[i] == board.zobristKey) {
            occurrences++;
        }
    }

    if (occurrences == 2) {
        int penalty = board.sideToMove == WHITE ? -100 : 100;
        taperedScore += penalty;
    }
    
    return (board.sideToMove == WHITE) ? taperedScore : -taperedScore;
}


int evaluatePawnStructure(uint64_t white_pawns, uint64_t black_pawns) {
    int score = 0;

    // Counters for pawn features
    int whiteIsolatedCount = 0, whiteDoubledCount = 0, whitePassedCount = 0;
    int blackIsolatedCount = 0, blackDoubledCount = 0, blackPassedCount = 0;

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
        uint64_t span = SPAN_NORTH[square] & (FILES[file] | ADJACENT_FILES[file]);
        if ((black_pawns & span) == 0) {
            whitePassedCount++;
        }
    }

    // Black passed pawns
    tempPawns = black_pawns;
    while (tempPawns) {
        int square = __builtin_ctzll(tempPawns);
        tempPawns &= tempPawns - 1;
        int file = square & 7;
        uint64_t span = SPAN_SOUTH[square] & (FILES[file] | ADJACENT_FILES[file]);
        if ((white_pawns & span) == 0) {
            blackPassedCount++;
        }
    }

    // 3) Aggregate score (from White's perspective)
    score += whitePassedCount * 15;
    score -= whiteIsolatedCount * 15;
    score -= whiteDoubledCount * 10;

    score -= blackPassedCount * 15;
    score += blackIsolatedCount * 15;
    score += blackDoubledCount * 10;

    return score;
}

int evaluateKingSafety(const BitBoard &board) {
    constexpr int PAWN_SHIELD_BONUS    = 15;
    constexpr int PAWN_SHIELD_MISSING  = -25;
    constexpr int OPEN_FILE_PENALTY    = -20;
    constexpr int KING_FILE_EXTRA_PENALTY = -10;
    constexpr int CENTRALITY_MID_PENALTY = -20;
    constexpr int CENTRALITY_END_BONUS = 15;
    constexpr int PROXIMITY_PENALTY    = -8;

    int whiteMid = 0, whiteEnd = 0;
    int blackMid = 0, blackEnd = 0;

    int wksq = board.whiteKingSquare;
    int bksq = board.blackKingSquare;

    int wfile = wksq & 7, wrank = wksq >> 3;
    int bfile = bksq & 7, brank = bksq >> 3;

    uint64_t whitePawns = board.getWhitePawns();
    uint64_t blackPawns = board.getBlackPawns();
    uint64_t allPawns   = whitePawns | blackPawns;

    uint64_t whiteShieldMask = 0;
    if (wrank <= 6) {
        int shieldRank = wrank + 1;
        for (int df = -1; df <= 1; ++df) {
            int file = wfile + df;
            if (file >= 0 && file < 8) {
                whiteShieldMask |= (1ULL << (shieldRank * 8 + file));
            }
        }
    }

    uint64_t blackShieldMask = 0;
    if (brank >= 1) {
        int shieldRank = brank - 1;
        for (int df = -1; df <= 1; ++df) {
            int file = bfile + df;
            if (file >= 0 && file < 8) {
                blackShieldMask |= (1ULL << (shieldRank * 8 + file));
            }
        }
    }

    for (int df = -1; df <= 1; ++df) {
        int wf = wfile + df;
        int bf = bfile + df;

        if (wf >= 0 && wf < 8) {
            uint64_t fileMask = FILES[wf];
            bool hasShield = (whitePawns & whiteShieldMask & fileMask) != 0;
            whiteMid += hasShield ? PAWN_SHIELD_BONUS : PAWN_SHIELD_MISSING;

            bool isOpen = (allPawns & fileMask) == 0;
            whiteMid += isOpen * (OPEN_FILE_PENALTY + (df == 0) * KING_FILE_EXTRA_PENALTY);
        }

        if (bf >= 0 && bf < 8) {
            uint64_t fileMask = FILES[bf];
            bool hasShield = (blackPawns & blackShieldMask & fileMask) != 0;
            blackMid += hasShield ? PAWN_SHIELD_BONUS : PAWN_SHIELD_MISSING;

            bool isOpen = (allPawns & fileMask) == 0;
            blackMid += isOpen * (OPEN_FILE_PENALTY + (df == 0) * KING_FILE_EXTRA_PENALTY);
        }
    }

    // Centrality (4 central files and ranks)
    bool whiteCentral = (wfile >= 2 && wfile <= 5) && (wrank >= 2 && wrank <= 5);
    bool blackCentral = (bfile >= 2 && bfile <= 5) && (brank >= 2 && brank <= 5);

    whiteMid += whiteCentral * CENTRALITY_MID_PENALTY;
    whiteEnd += whiteCentral * CENTRALITY_END_BONUS;

    blackMid += blackCentral * CENTRALITY_MID_PENALTY;
    blackEnd += blackCentral * CENTRALITY_END_BONUS;

    // Piece proximity to king
    uint64_t wZone = board.getKingAttacks(wksq);
    uint64_t bZone = board.getKingAttacks(bksq);

    int blackNearWhite = __builtin_popcountll(board.getBlackPieces() & wZone);
    int whiteNearBlack = __builtin_popcountll(board.getWhitePieces() & bZone);

    whiteMid += blackNearWhite * PROXIMITY_PENALTY;
    blackMid += whiteNearBlack * PROXIMITY_PENALTY;

    // Final scores
    int mid = whiteMid - blackMid;
    int end = whiteEnd - blackEnd;

    // Safer packing (cast to unsigned to avoid sign-extension)
    return (static_cast<uint32_t>(mid) << 16) | (static_cast<uint16_t>(end) & 0xFFFF);
}