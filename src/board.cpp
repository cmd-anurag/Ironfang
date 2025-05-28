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
    whiteKingsideCastle = true;
    whiteQueensideCastle = true;
    blackKingsideCastle = true;
    blackQueensideCastle = true;

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

bool Board::isSquareAttacked(int square, Color opponentColor) const {

    const int directions[8] = {-11, -10, -9, 1, 11, 10, 9, -1};
    int mailBoxIndex = MailBox::mailbox64[square];

    for(int direction : directions) {
        for(int factor = 1; factor < 8; ++factor) {
            int newSquare = MailBox::mailbox[mailBoxIndex + direction*factor];
            if(newSquare == -1) break;

            Piece occupied = getPiece(newSquare);
            if(occupied != NONE) {
                Color c = getPieceColor(occupied);
                if(c == opponentColor) {
                    return true;
                }
                else {
                    break;
                }
            }
            
        }
    }
    const int knightDirections[8] = { -21, -19,-12, -8, 8, 12, 19, 21 };
    for(int direction : knightDirections) {
        int newSquare = MailBox::mailbox[mailBoxIndex + direction];
        if(newSquare == -1) break;
        Piece occupied = getPiece(newSquare);
        if(occupied != NONE) {
            Color c = getPieceColor(occupied);
            if(c == opponentColor) {
                return true;
            }
            else {
                continue;
            }
        }
    }
    return false;
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
            case WB:
            case BB:
                generateBishopMoves(square, moves, p, color);
                break;
            case WQ:
            case BQ:
                generateQueenMoves(square, moves, p, color);
                break;
            case WK:
            case BK:
                generateKingMoves(square, moves, p, color);
                break;
            case WN:
            case BN:
                generateKnightMoves(square, moves, p, color);
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
                moves.push_back(move);
                break;
            }
        }
    }
}

void Board::generateBishopMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    const int directions[4] = {-11, -9, 11, 9};
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
                moves.push_back(move);
                break;
            }
        }
    }
}

void Board::generateQueenMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    const int directions[8] = {-11, -10, -9, 1, 11, 10, 9, -1};
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
                moves.push_back(move);
                break;
            }
        }
    }
}

void Board::generateKingMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    const int directions[8] = {-11, -10, -9, 1, 11, 10, 9, -1};
    int mailBoxIndex = MailBox::mailbox64[square];

    for(int direction : directions) {
            int newSquare = MailBox::mailbox[mailBoxIndex + direction];
            if(newSquare == -1) continue;

            Piece occupied = getPiece(newSquare);

            if(occupied == NONE) {
                Move move(p, square, newSquare);
                moves.push_back(move);
            }
            else if(getPieceColor(occupied) == color) continue;
            else {
                Move move(p, square, newSquare, 0, true);
                moves.push_back(move);
                continue;
            }
    }
    // right.. king can castle too
    

    if(color == WHITE) {
        // no castling from check
        if(isSquareAttacked(square, BLACK)) return;

        
        if(whiteKingsideCastle && square==60 && getPiece(63)==WR) {
            bool ok=true;
            for(int sq : {61,62}) {
                if(isSquareAttacked(sq, BLACK) || getPiece(sq)!=NONE) {
                    ok=false; break;
                }
            }
            if(ok) moves.emplace_back(p, 60, 62, false, false, true, false);
        }

        // queenside: check d1(59) & c1(58) for both attack+occupancy, b1(57) for occupancy only
        if(whiteQueensideCastle && square==60 && getPiece(56)==WR) {
            bool ok=true;
            // d1 & c1 must be empty & safe
            for(int sq : {59,58}) {
                if(isSquareAttacked(sq, BLACK) || getPiece(sq)!=NONE) {
                    ok=false; break;
                }
            }
            // b1 need only be empty
            if(ok && getPiece(57)!=NONE) ok=false;

            if(ok) moves.emplace_back(p, 60, 58, 0,0, false, true);
        }

    } else {
        if(isSquareAttacked(square, WHITE)) return;

        
        if(blackKingsideCastle && square==4 && getPiece(7)==BR) {
            bool ok=true;
            for(int sq : {5,6}) {
                if(isSquareAttacked(sq, WHITE) || getPiece(sq)!=NONE) {
                    ok=false; break;
                }
            }
            if(ok) moves.emplace_back(p, 4, 6, 0,0, true, false);
        }

        // black queenside: d8(3) & c8(2) for attack+occupancy, b8(1) for occupancy
        if(blackQueensideCastle && square==4 && getPiece(0)==BR) {
            bool ok=true;
            for(int sq : {3,2}) {
                if(isSquareAttacked(sq, WHITE) || getPiece(sq)!=NONE) {
                    ok=false; break;
                }
            }
            if(ok && getPiece(1)!=NONE) ok=false;

            if(ok) moves.emplace_back(p, 4, 2, 0,0, false, true);
        }
    }
    // this was annoying

}

void Board::generateKnightMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    const int directions[8] = { -21, -19,-12, -8, 8, 12, 19, 21 };
    int mailBoxIndex = MailBox::mailbox64[square];

    for(int direction : directions) {
            int newSquare = MailBox::mailbox[mailBoxIndex + direction];
            if(newSquare == -1) continue;

            Piece occupied = getPiece(newSquare);

            if(occupied == NONE) {
                Move move(p, square, newSquare);
                moves.push_back(move);
            }
            else if(getPieceColor(occupied) == color) continue;
            else {
                Move move(p, square, newSquare, 0, true);
                moves.push_back(move);
                continue;
            }
    }
}

