#include "evaluate.h"

const int pawnPST[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

const int knightPST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

const int bishopPST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

const int rookPST[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

const int queenPST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

const int kingMdPST[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

const int kingEgPST[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

const int *PST[7] = {pawnPST, rookPST, knightPST, bishopPST, queenPST, kingMdPST, kingEgPST};

class Board;
class BitBoard;

int Evaluation::evaluate(const BitBoard &board) {
    int phaseMaterial = 0;
    int score = 0;

    // Handle repetitions (same as your current code)
    auto it = board.repetitionMap.find(board.zobristKey);
    if (it != board.repetitionMap.end() && it->second > 1) {
        int repetitionPenalty = 500 * (it->second - 1);
        
        if (board.sideToMove == WHITE) {
            score -= repetitionPenalty;
        } else {
            score += repetitionPenalty;
        }
    }
    
    // First pass: calculate phase material (same as your current code)
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
    
    // Get bitboards for special evaluations
    uint64_t whitePawns = board.getWhitePawns();
    uint64_t blackPawns = board.getBlackPawns();
    uint64_t whiteRooks = board.getWhiteRooks();
    uint64_t blackRooks = board.getBlackRooks();
    uint64_t whiteBishops = board.getWhiteBishops();
    uint64_t blackBishops = board.getBlackBishops();
    uint64_t whiteKnights = board.getWhiteKnights();
    uint64_t blackKnights = board.getBlackKnights();
    uint64_t whiteQueens = board.getWhiteQueens();
    uint64_t blackQueens = board.getBlackQueens();
    
    // Standard piece evaluation (no changes here)
    uint64_t whitePieces = board.getWhitePieces();
    while(whitePieces) {
        int square = board.popLSB(whitePieces);
        Piece p = board.getPiece(square);
        int basePiece = p & 7;
        
        if(endgame && basePiece == 6) {
            score += pieceValue[basePiece] + PST[6][square];    
        }
        else {
            score += pieceValue[basePiece] + PST[basePiece-1][square];
        }
    }
    
    uint64_t blackPieces = board.getBlackPieces();
    while(blackPieces) {
        int square = board.popLSB(blackPieces);
        Piece p = board.getPiece(square);
        int basePiece = p & 7;
        
        if(endgame && basePiece == 6) {
            score -= pieceValue[basePiece] + PST[6][mirror(square)];    
        }
        else {
            score -= pieceValue[basePiece] + PST[basePiece - 1][mirror(square)];
        }
    }

    // ADDITIONAL EVALUATIONS:
    
    // 1. Bishop pair bonus
    if (__builtin_popcountll(whiteBishops) >= 2) {
        score += 25;  // Bishop pair bonus
    }
    if (__builtin_popcountll(blackBishops) >= 2) {
        score -= 25;  // Bishop pair bonus
    }
    
    // 2. Rooks on open files
    uint64_t whiteRooksCopy = whiteRooks;
    while (whiteRooksCopy) {
        int square = board.popLSB(whiteRooksCopy);
        int file = square & 7;
        
        // Check if this file has any pawns
        uint64_t fileMask = 0x0101010101010101ULL << file;
        uint64_t pawnsInFile = (whitePawns | blackPawns) & fileMask;
        
        if (pawnsInFile == 0) {
            score += 15;  // Open file bonus
        } else if ((blackPawns & fileMask) == 0) {
            score += 10;  // Semi-open file bonus (no enemy pawns)
        }
    }
    
    uint64_t blackRooksCopy = blackRooks;
    while (blackRooksCopy) {
        int square = board.popLSB(blackRooksCopy);
        int file = square & 7;
        
        // Check if this file has any pawns
        uint64_t fileMask = 0x0101010101010101ULL << file;
        uint64_t pawnsInFile = (whitePawns | blackPawns) & fileMask;
        
        if (pawnsInFile == 0) {
            score -= 15;  // Open file bonus
        } else if ((whitePawns & fileMask) == 0) {
            score -= 10;  // Semi-open file bonus (no enemy pawns)
        }
    }

    // 3. Knight outpost evaluation - using getWhiteKnights() and getBlackKnights()
    uint64_t whiteKnightsCopy = whiteKnights;
    while (whiteKnightsCopy) {
        int square = board.popLSB(whiteKnightsCopy);
        int rank = square >> 3;
        int file = square & 7;
        
        // Knight outpost: knight on ranks 4-6, supported by a pawn, not attackable by enemy pawn
        if (rank >= 3 && rank <= 5) {
            // Check if protected by friendly pawn
            bool protectedByPawn = false;
            if (file > 0 && (whitePawns & (1ULL << (square - 9)))) protectedByPawn = true;
            if (file < 7 && (whitePawns & (1ULL << (square - 7)))) protectedByPawn = true;
            
            // Check if attackable by enemy pawn
            bool attackableByPawn = false;
            if (file > 0 && (blackPawns & (1ULL << (square + 7)))) attackableByPawn = true;
            if (file < 7 && (blackPawns & (1ULL << (square + 9)))) attackableByPawn = true;
            
            if (protectedByPawn && !attackableByPawn) {
                score += 15;  // Knight outpost bonus
            }
        }
    }
    
    uint64_t blackKnightsCopy = blackKnights;
    while (blackKnightsCopy) {
        int square = board.popLSB(blackKnightsCopy);
        int rank = square >> 3;
        int file = square & 7;
        
        // Knight outpost: knight on ranks 3-5, supported by a pawn, not attackable by enemy pawn
        if (rank >= 2 && rank <= 4) {
            // Check if protected by friendly pawn
            bool protectedByPawn = false;
            if (file > 0 && (blackPawns & (1ULL << (square + 7)))) protectedByPawn = true;
            if (file < 7 && (blackPawns & (1ULL << (square + 9)))) protectedByPawn = true;
            
            // Check if attackable by enemy pawn
            bool attackableByPawn = false;
            if (file > 0 && (whitePawns & (1ULL << (square - 9)))) attackableByPawn = true;
            if (file < 7 && (whitePawns & (1ULL << (square - 7)))) attackableByPawn = true;
            
            if (protectedByPawn && !attackableByPawn) {
                score -= 15;  // Knight outpost bonus
            }
        }
    }
    
    // 4. Queen mobility and activity - using getWhiteQueens() and getBlackQueens()
    uint64_t whiteQueensCopy = whiteQueens;
    while (whiteQueensCopy) {
        int square = board.popLSB(whiteQueensCopy);
        
        // Queen activity: bonus for central squares
        int rank = square >> 3;
        int file = square & 7;
        
        if (rank >= 2 && rank <= 5 && file >= 2 && file <= 5) {
            score += 10;  // Bonus for central queen
        }
        
        // Queen mobility: bonus for number of legal moves
        uint64_t queenAttacks = board.getQueenAttacks(square, board.getAllPieces());
        
        // Count attacks that aren't on friendly pieces
        int mobility = __builtin_popcountll(queenAttacks & ~board.getWhitePieces());
        score += mobility * 2;  // 2 points per mobility square
    }
    
    uint64_t blackQueensCopy = blackQueens;
    while (blackQueensCopy) {
        int square = board.popLSB(blackQueensCopy);
        
        // Queen activity: bonus for central squares
        int rank = square >> 3;
        int file = square & 7;
        
        if (rank >= 2 && rank <= 5 && file >= 2 && file <= 5) {
            score -= 10;  // Bonus for central queen
        }
        
        // Queen mobility: bonus for number of legal moves
        uint64_t queenAttacks = board.getQueenAttacks(square, board.getAllPieces());
        
        // Count attacks that aren't on friendly pieces
        int mobility = __builtin_popcountll(queenAttacks & ~board.getBlackPieces());
        score -= mobility * 2;  // 2 points per mobility square
    }
    
    // 5. Doubled pawns penalty
    for (int file = 0; file < 8; file++) {
        uint64_t fileMask = 0x0101010101010101ULL << file;
        
        int whitePawnCount = __builtin_popcountll(whitePawns & fileMask);
        if (whitePawnCount > 1) {
            score -= 10 * (whitePawnCount - 1);  // Penalty for doubled pawns
        }
        
        int blackPawnCount = __builtin_popcountll(blackPawns & fileMask);
        if (blackPawnCount > 1) {
            score += 10 * (blackPawnCount - 1);  // Penalty for doubled pawns
        }
    }
    
    // 6. King safety in middlegame
    if (!endgame) {
        // White king pawn shield
        int whiteKingFile = board.whiteKingSquare & 7;
        int whiteKingRank = board.whiteKingSquare >> 3;
        
        // Check for pawns in front of the king (assuming king on back rank)
        if (whiteKingRank == 0) {
            for (int f = std::max(0, whiteKingFile - 1); f <= std::min(7, whiteKingFile + 1); f++) {
                uint64_t shieldMask = 1ULL << (8 + f);  // Pawn on 2nd rank
                if (whitePawns & shieldMask) {
                    score += 10;  // Bonus for each shield pawn
                }
            }
        }
        
        // Black king pawn shield
        int blackKingFile = board.blackKingSquare & 7;
        int blackKingRank = board.blackKingSquare >> 3;
        
        // Check for pawns in front of the king (assuming king on back rank)
        if (blackKingRank == 7) {
            for (int f = std::max(0, blackKingFile - 1); f <= std::min(7, blackKingFile + 1); f++) {
                uint64_t shieldMask = 1ULL << (48 + f);  // Pawn on 7th rank
                if (blackPawns & shieldMask) {
                    score -= 10;  // Bonus for each shield pawn
                }
            }
        }
    }

    return (board.sideToMove == WHITE) ? score : -score;
}