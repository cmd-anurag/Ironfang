#include "board.h"
#include<iostream>

Board::Board() {
    // White Piece
    squares[56] = WR; squares[57] = WN; squares[58] = WB; squares[59] = WQ; squares[60] = WK;
    squares[61] = WB; squares[62] = WN; squares[63] = WR;

    for(int i = 48; i <= 55; ++i) {
        squares[i] = WP;
    }

    // Black Pieces
    squares[0] = BR; squares[1] = BN; squares[2] = BB; squares[3] = BQ; squares[4] = BK;
    squares[5] = BB; squares[6] = BN; squares[7] = BR;
    
    for(int i = 8; i <= 15; ++i) {
        squares[i] = BP;
    }

    // set remaining squares to none
    for(int i = 16; i <= 47; ++i) {
        squares[i] = NONE;
    }
}

Piece Board::getPiece(int square) const {
    return squares[square];
}

void Board::setPiece(Piece p, int square) {
    squares[square] = p;
}

void Board::print() const {
    for(int rank = 0; rank < 8; ++rank) {
        std::cout << 8 - rank << "  ";

        for(int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            std::cout << pieceToChar(getPiece(square)) << " ";
        }
        std::cout << '\n';
    }
    std::cout << "   a b c d e f g h" << '\n';
}