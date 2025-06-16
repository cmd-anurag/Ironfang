#include "evaluate.h"
#include "bitboard.h"
#include "precomputedEvalTables.h"
#include <unordered_map>


const int *PST[7] = {pawnPST, rookPST, knightPST, bishopPST, queenPST, kingMdPST, kingEgPST};



class Board;
class BitBoard;
int evaluatePawnStructure(uint64_t white_pawns, uint64_t black_pawns);

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

    // Draw penalty
    int occurrences = 0;

    for (int i = 0; i < board.pathDepth; ++i) {
        if (board.repetitionPath[i] == board.zobristKey) {
            occurrences++;
        }
    }

    // If this is the second time this position appears in the current search path,
    // apply a penalty. The search itself handles the 3rd occurrence as a draw (score 0).
    if (occurrences == 2) {
        int penalty = board.sideToMove == WHITE? -100 : 100;
        score += penalty;
    }
    
    return (board.sideToMove == WHITE) ? score : -score;
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
    score += whitePassedCount * 30;
    score -= whiteIsolatedCount * 20;
    score -= whiteDoubledCount * 10;

    score -= blackPassedCount * 30;
    score += blackIsolatedCount * 20;
    score += blackDoubledCount * 10;

    return score;
}