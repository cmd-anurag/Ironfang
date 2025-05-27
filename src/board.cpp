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
                generatePawnMoves(square, moves, p, color);
                break;
            case WR:
            case BR:
                generateRookMoves(square, moves, p, color);
                break;
            default:
                break;
        }
    }

    return moves;
}

void Board::generatePawnMoves(int square, std::vector<Move> &moves, Piece p, Color color) const {

    int moveDir;
    int rightCapture;
    int leftCapture; 

    if(color == WHITE) {
        moveDir = -10;
        rightCapture = -9;
        leftCapture = -11;
    }
    else {
        moveDir = 10;
        rightCapture = 11;
        leftCapture = 9;
    }

    // Forward 1 step Movement

    int mailboxIndex = MailBox::mailbox64[square];
    int newSquare = MailBox::mailbox[mailboxIndex + moveDir];

    if(newSquare != -1 && getPiece(newSquare) == NONE) {
        
        bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
        Move move(p, square, newSquare, promotion, false);
        moves.push_back(move);
    }

    

    // Left Capture
    newSquare = MailBox::mailbox[mailboxIndex + leftCapture];
    if(newSquare != -1) {
        Piece occupiedPiece = getPiece(newSquare);
        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
            bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
            Move move(p, square, newSquare, promotion, true);
            moves.push_back(move);
        }
    }

    // Right Capture
    
    newSquare = MailBox::mailbox[mailboxIndex + rightCapture];
    if(newSquare != -1) {
        Piece occupiedPiece = getPiece(newSquare);
        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
            bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);
            Move move(p, square, newSquare, promotion, true);
            moves.push_back(move);
        }
    }

    // Forward 2 step movement
    if(color == WHITE && square >= 48 && square <= 55) {
        newSquare = MailBox::mailbox[mailboxIndex + moveDir + moveDir];
        if(getPiece(newSquare) == NONE) {
            Move move(p, square, newSquare, false, false);
            moves.push_back(move);
        }
    }

    if(color == BLACK && square >= 8 && square <= 15) {
        newSquare = MailBox::mailbox[mailboxIndex + moveDir + moveDir];
        if(getPiece(newSquare) == NONE) {
            Move move(p, square, newSquare, false, false);
            moves.push_back(move);
        }
    }
}

void Board::generateRookMoves(int square, std::vector<Move> &moves, Piece p, Color color) const {
    const int directions[4] = {-10, 1, 10, -1};
    int mailBoxIndex = MailBox::mailbox64[square];

    for(int direction : directions) {

        for(int factor = 1; factor < 8; ++factor) {

            int newSquare = MailBox::mailbox[mailBoxIndex + direction*factor];
            if(newSquare == -1) break;

            Piece occupied = getPiece(newSquare);

            if(occupied == NONE) {
                Move move(p, square, newSquare);
                moves.push_back(move);
            }
            else if(getPieceColor(occupied) == color) break;
            else {
                Move move(p, square, newSquare, 0, true);
                // lmao who will add this to moves?
                moves.push_back(move);
                break;
            }
        }
    }
}