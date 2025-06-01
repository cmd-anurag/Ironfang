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

int Evaluation::evaluate(const Board &board) {

    int phaseMaterial = 0;
    for(int i = 0; i < 64; ++i) {
        Piece p = board.getPiece(i);
        if(p == NONE) continue;

        int basePiece = p % 8;

        if(basePiece != 1 && basePiece != 6) phaseMaterial += pieceValue[basePiece];
    }
    bool endgame = phaseMaterial <= 1300;
    
    int score = 0;
    for(int i = 0; i < 64; ++i) {
        Piece p = board.getPiece(i);
        if(p == NONE) continue;

        int basePiece = p % 8;
        Color pieceColor = getPieceColor(p);
        
        if(pieceColor == WHITE) {
            if(endgame && basePiece == 6) {
                score += pieceValue[basePiece] + PST[basePiece][i];    
            }
            else {
                score += pieceValue[basePiece] + PST[basePiece-1][i];
            }
            // yeah the PST indexing for base pieces are weird, the explanation is that i put pawn at index 0 instead of 1, rook at index 1 instead of 2 and the basepiece is starting from 1, and since King has two PST entries, it does not need a decrement when i'm trying to access its endgame table
        }
        else {
            if(endgame && basePiece == 6) {
                score -= pieceValue[basePiece] + PST[basePiece][mirror(i)];    
            }
            else {
                score -= pieceValue[basePiece] + PST[basePiece - 1][mirror(i)];
            }
        }
    }

    return (board.sideToMove == WHITE) ? score : -score;
}