#include "evaluate.h"

class Board;

int Evaluation::evaluate(const Board &board) {
    
    int score = 0;
    for(int i = 0; i < 64; ++i) {
        Piece p = board.getPiece(i);

        if(p == NONE) continue;
        else if(getPieceColor(p) == WHITE) {
            score += pieceValue[p];
        }
        else {
            score -= pieceValue[p-8];
        }
    }

    return score;
}