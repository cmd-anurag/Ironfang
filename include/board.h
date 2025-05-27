#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "move.h"
#include <vector>

class Board {
    public:
        Color sideToMove;

        Board();
        void print() const;
        Piece getPiece(int square) const;
        void setPiece(Piece p, int square);
        std::vector<Move> generateMoves() const;

    private:
        Piece squares[64];
        void generatePawnMoves(int square, std::vector<Move>& moves) const;
};

#endif