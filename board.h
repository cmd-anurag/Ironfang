#ifndef BOARD_H
#define BOARD_H

#include "types.h"

class Board {
    public:
        Board();
        void print() const;
        Piece getPiece(int square) const;
        void setPiece(Piece p, int square);
    private:
        Piece squares[64];
};

#endif