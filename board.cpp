#include "board.h"
#include "mailbox.h"
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

    sideToMove = WHITE;
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

std::vector<Move> Board::generateMoves() const {
    std::vector<Move> moves;

    for(int square = 0; square < 64; ++square) {
        Piece p = getPiece(square);
        Color color = getPieceColor(p);

        if(p == NONE || color != sideToMove) continue;

        switch(p) {
            case WP:
            case BP:
                generatePawnMoves(square, moves);
                break;

            case WR:
            case BR:
                // generateRookMoves(squaes, moves);
                break;
        }
    }

    return moves;
}

void Board::generatePawnMoves(int square, std::vector<Move> &moves) const {
    Piece p = getPiece(square);
    Color color = getPieceColor(p);

    int moveDir;
    int rightCapture;
    int leftCapture; 

    if(color == WHITE) {
        moveDir = -8;
        rightCapture = -7;
        leftCapture = -9;
    }
    else {
        moveDir = 8;
        rightCapture = 9;
        leftCapture = 7;
    }

    // Forward 1 step Movement
    int newSquare = square + moveDir;
    
    
    if(MailBox::isOnboard(newSquare) && getPiece(newSquare) == NONE) {
        bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
        Move move(p, square, newSquare, promotion, false);
        moves.push_back(move);
    }

    // Left Capture
    newSquare = square + leftCapture;
   

    if(MailBox::isOnboard(newSquare)) {

        Piece occupiedPiece = getPiece(newSquare);

        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
            bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
            Move move(p, square, newSquare, promotion, true);
            moves.push_back(move);
        }
    }

    // Right Capture
    newSquare = square + rightCapture;
    

    if(MailBox::isOnboard(newSquare)) {
        Piece occupiedPiece = getPiece(newSquare);
        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
            bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
            Move move(p, square, newSquare, promotion, true);
            moves.push_back(move);
        }
    }

    // Forward 2 step movement
    newSquare = square + moveDir + moveDir;
    
}