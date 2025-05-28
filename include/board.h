#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "move.h"
#include <vector>

class Board {
    public:
        Color sideToMove;
        bool whiteKingsideCastle;
        bool whiteQueensideCastle;
        bool blackKingsideCastle;
        bool blackQueensideCastle;


        Board();
        void print() const;
        Piece getPiece(int square) const;
        void setPiece(Piece p, int square);
        std::vector<Move> generateMoves() const;
        bool isSquareAttacked(int square, Color opponentColor) const;

    private:
        Piece squares[64];
        void generatePawnMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateRookMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateBishopMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateQueenMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateKingMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateKnightMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
};

#endif