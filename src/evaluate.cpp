#include "evaluate.h"
#include "bitboard.h"
#include "precomputedEvalTables.h"
#include <unordered_map>


const int *PST[7] = {pawnPST, knightPST, bishopPST, rookPST, queenPST, kingMdPST, kingEgPST};



class Board;
class BitBoard;
int evaluatePawnStructure(uint64_t white_pawns, uint64_t black_pawns);
int evaluateKingSafety(const BitBoard &board);

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
    
    // Evaluate white pieces
    uint64_t whitePieces = board.getWhitePieces();
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
    uint64_t blackPieces = board.getBlackPieces();
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

    // Pawn Structure
    score += evaluatePawnStructure(board.whitePawns, board.blackPawns);

    // King Safety (only in middlegame)
    if (!endgame) {
        score += evaluateKingSafety(board);
    }

    // Draw penalty
    int occurrences = 0;

    for (int i = 0; i < board.pathDepth; ++i) {
        if (board.repetitionPath[i] == board.zobristKey) {
            occurrences++;
        }
    }

    // If this is the second time this position appears in the current search path,
    // apply a penalty. The search itself handles the 3rd occurrence as a draw (score 0).
    if (occurrences >= 2) {
        int penalty = board.sideToMove == WHITE? -100 : +100;
        score += penalty;
    }
    
    return (board.sideToMove == WHITE) ? score : -score;
}

int evaluateKingSafety(const BitBoard &board) {
    int score = 0;
    
    // Evaluate white king safety
    int whiteKingFile = board.whiteKingSquare & 7;
    
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
                
                if (pawnRank > kingRank) { // Pawn is in front of king
                    int distance = pawnRank - kingRank;
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
    int blackKingFile = board.blackKingSquare & 7;
    
    shieldBonus = 0;
    for (int f = blackKingFile - 1; f <= blackKingFile + 1; ++f) {
        if (f >= 0 && f < 8) {
            uint64_t fileSquares = FILES[f] & board.getBlackPawns();
            if (fileSquares) {
                int pawnSquare = __builtin_ctzll(fileSquares);
                int pawnRank = pawnSquare / 8;
                int kingRank = board.blackKingSquare / 8;
                
                if (pawnRank < kingRank) { // Pawn is in front of black king
                    int distance = kingRank - pawnRank;
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
    score += whitePassedCount * 40;
    score -= whiteIsolatedCount * 25;
    score -= whiteDoubledCount * 15;

    score -= blackPassedCount * 40;
    score += blackIsolatedCount * 25;
    score += blackDoubledCount * 15;

    return score;
}