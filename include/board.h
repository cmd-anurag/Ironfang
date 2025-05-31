#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "Gamestate.h"
#include "move.h"
#include <vector>

class Evaluation;

class Board {
    public:
        Color sideToMove;
        int enPassantSquare = -1;

        bool whiteKingsideCastle;
        bool whiteQueensideCastle;
        bool blackKingsideCastle;
        bool blackQueensideCastle;

        int whiteKingSquare;
        int blackKingSquare;


        Board();
        void setStartPosition();
        void print() const;
        Piece getPiece(int square) const;
        void setPiece(Piece p, int square);
        bool setPositionFromFEN(const std::string& fen);
        std::vector<Move> generateMoves() const;
        bool isSquareAttacked(int square, Color opponentColor) const;

        bool makeMove(const Move &move);
        void unmakeMove(const Move &move, const Gamestate &prevState);
        bool tryMove(const Move& move);
        friend class Evaluation;

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