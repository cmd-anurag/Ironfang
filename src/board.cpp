#include "board.h"
#include "mailbox.h"
#include "zobrist.h"
#include<iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <cstdint>
#include <fstream>

Board::Board() {
    initZobrist();  // Initialzie the zobrist tables
    setStartPosition();
}

void Board::setStartPosition() {
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

    whiteKingSquare = 60;
    blackKingSquare = 4;

    
    zobristKey = generateZobristHashKey(*this);
}

Piece Board::getPiece(int square) const {
    return squares[square];
}

void Board::setPiece(Piece p, int square) {
    squares[square] = p;
}

bool Board::setPositionFromFEN(const std::string& fen) {
    // Clear the board first
    for (int i = 0; i < 64; ++i) {
        setPiece(NONE, i);
    }
    
    std::istringstream ss(fen);
    std::string token;
    
    // 1. Piece placement
    if (!(ss >> token)) return false;
    
    int square = 0;
    for (char c : token) {
        if (c == '/') {
            // Skip to the next rank
            continue;
        } else if (isdigit(c)) {
            // Skip empty squares
            square += (c - '0');
        } else {
            // Place a piece
            Piece p = NONE;
            switch (c) {
                case 'P': p = WP; break;
                case 'N': p = WN; break;
                case 'B': p = WB; break;
                case 'R': p = WR; break;
                case 'Q': p = WQ; break;
                case 'K': p = WK; break;
                case 'p': p = BP; break;
                case 'n': p = BN; break;
                case 'b': p = BB; break;
                case 'r': p = BR; break;
                case 'q': p = BQ; break;
                case 'k': p = BK; break;
                default: return false; // Invalid character
            }
            
            if (p != NONE) {
                setPiece(p, square);
                
                // Track king positions
                if (p == WK) whiteKingSquare = square;
                if (p == BK) blackKingSquare = square;
                
                square++;
            }
        }
    }
    
    // 2. Active color
    if (!(ss >> token)) return false;
    sideToMove = (token == "w") ? WHITE : BLACK;
    
    // 3. Castling availability
    if (!(ss >> token)) return false;
    whiteKingsideCastle = (token.find('K') != std::string::npos);
    whiteQueensideCastle = (token.find('Q') != std::string::npos);
    blackKingsideCastle = (token.find('k') != std::string::npos);
    blackQueensideCastle = (token.find('q') != std::string::npos);
    
    // 4. En passant target square
    if (!(ss >> token)) return false;
    if (token == "-") {
        enPassantSquare = -1;
    } else {
        int file = token[0] - 'a';
        int rank = 8 - (token[1] - '0');
        enPassantSquare = rank * 8 + file;
    }
    
    return true;
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

    int mailBoxIndex = MailBox::mailbox64[square];
    
    const int rookDirections[4] = {-10, 1, 10, -1};
    for(int direction : rookDirections) {
        for(int factor = 1; factor < 8; ++factor) {
            int newSquare = MailBox::mailbox[mailBoxIndex + direction*factor];
            if(newSquare == -1) break;

            Piece occupied = getPiece(newSquare);
            if(occupied == NONE){
                continue;
            }
            else if(occupied % 8 == 2 || occupied % 8 == 5) {
                if(getPieceColor(occupied) == opponentColor) {
                    return true;
                }
            }
            break;
        }
    }

    const int bishopDirections[4] = {-11, -9, 11, 9};
    for(int direction : bishopDirections) {
        for(int factor = 1; factor < 8; ++factor) {
            int newSquare = MailBox::mailbox[mailBoxIndex + direction*factor];
            if(newSquare == -1) break;

            Piece occupied = getPiece(newSquare);
            if(occupied == NONE){
                continue;
            }
            else if(occupied % 8 == 4 || occupied % 8 == 5) {
                if(getPieceColor(occupied) == opponentColor) {
                    return true;
                }
            }
            break;
            
        }
    }

    const int kingDirections[8] = {-10, 1, 10, -1, -11, -9, 11, 9};
    for(int direction : kingDirections) {
        int newSquare = MailBox::mailbox[mailBoxIndex + direction];
        if(newSquare == -1) continue;

        Piece occupied = getPiece(newSquare);
        if(occupied % 8 == 6 && getPieceColor(occupied) == opponentColor) {
            return true;
        }
        else {
            continue;
        }
    }

    if(opponentColor == BLACK) {
        int leftSquare = MailBox::mailbox[mailBoxIndex - 11];
        int rightSquare = MailBox::mailbox[mailBoxIndex - 9];

        if(leftSquare != -1 && getPiece(leftSquare) == BP) return true;
        if(rightSquare != -1 && getPiece(rightSquare) == BP) return true;
    } 
    else {
        int leftSquare = MailBox::mailbox[mailBoxIndex + 9];
        int rightSquare = MailBox::mailbox[mailBoxIndex + 11];

        if(leftSquare != -1 && getPiece(leftSquare) == WP) return true;
        if(rightSquare != -1 && getPiece(rightSquare) == WP) return true;
    }


    const int knightDirections[8] = { -21, -19,-12, -8, 8, 12, 19, 21 };
    for(int direction : knightDirections) {
        int newSquare = MailBox::mailbox[mailBoxIndex + direction];
        if(newSquare == -1) continue;
        Piece occupied = getPiece(newSquare);
        if(occupied % 8 == 3) {
            Color c = getPieceColor(occupied);
            if(c == opponentColor ) {
                return true;
            }
            else {
                continue;
            }
        }
    }
    return false;
}

bool Board::makeMove(const Move &move) {
    Gamestate prevState = {
        sideToMove, 
        enPassantSquare,
        whiteKingsideCastle,
        whiteQueensideCastle,
        blackKingsideCastle,
        blackQueensideCastle,
        whiteKingSquare,
        blackKingSquare,
        zobristKey
    };

    if(enPassantSquare != -1) {
        int file = enPassantSquare % 8;
        zobristKey ^= zobristEnPassant[file];
    }
    enPassantSquare = -1;
    
    // Castle updates
    if(move.isKingSideCastle) {
        if(sideToMove == WHITE) {

            setPiece(WR, 61);
            setPiece(WK, 62);
            setPiece(NONE, 60);
            setPiece(NONE, 63);
            whiteKingSquare = 62;
            whiteKingsideCastle = false;
            whiteQueensideCastle = false;

            // Zobrist changes
            xorPiece(zobristKey, WK, 60);
            xorPiece(zobristKey, WK, 62);
            xorPiece(zobristKey, WR, 63);
            xorPiece(zobristKey, WR, 61);

            // zobristKey ^= zobristCastling[0];
            // zobristKey ^= zobristCastling[1];
        }
        else {
            setPiece(BR, 5);
            setPiece(BK, 6);
            setPiece(NONE, 4);
            setPiece(NONE, 7);
            blackKingSquare = 6;
            blackKingsideCastle = false;
            blackQueensideCastle = false;

            // Zobrist changes
            xorPiece(zobristKey, BK, 4);
            xorPiece(zobristKey, BK, 6);
            xorPiece(zobristKey, BR, 7);
            xorPiece(zobristKey, BR, 5);

            // zobristKey ^= zobristCastling[2];
            // zobristKey ^= zobristCastling[3];
        }
    }
    else if(move.isQueenSideCastle) {
        if(sideToMove == WHITE) {
            setPiece(WR, 59);
            setPiece(WK, 58);
            setPiece(NONE, 60);
            setPiece(NONE, 56);
            whiteKingSquare = 58;
            whiteKingsideCastle = false;
            whiteQueensideCastle = false;

            // Zobrist Changes
            xorPiece(zobristKey, WK, 60);
            xorPiece(zobristKey, WK, 58);
            xorPiece(zobristKey, WR, 56);
            xorPiece(zobristKey, WR, 59);

            // zobristKey ^= zobristCastling[0];
            // zobristKey ^= zobristCastling[1];
        }
        else {
            setPiece(BR, 3);
            setPiece(BK, 2);
            setPiece(NONE, 4);
            setPiece(NONE, 0);
            blackKingSquare = 2;
            blackKingsideCastle = false;
            blackQueensideCastle = false;

            // Zobrist Changes
            xorPiece(zobristKey, BK, 4);
            xorPiece(zobristKey, BK, 2);
            xorPiece(zobristKey, BR, 0);
            xorPiece(zobristKey, BR, 3);

            // zobristKey ^= zobristCastling[2];
            // zobristKey ^= zobristCastling[3];
        }
    }
    else if(move.isEnPassant) { // en passant

        setPiece(NONE, move.from);
        setPiece(move.piece, move.to);

        int capturedPawnSquare = move.to + (sideToMove == WHITE? 8 : -8);
        Piece capturedPiece = getPiece(capturedPawnSquare);
        setPiece(NONE, capturedPawnSquare);

        // Zobrist Changes
        xorPiece(zobristKey, move.piece, move.from);
        xorPiece(zobristKey, move.piece, move.to);
        xorPiece(zobristKey, capturedPiece, capturedPawnSquare);
        
    }
    else {
        // normal moves and captures
        if(move.piece == WP || move.piece == BP) {
            if(abs(move.from - move.to) == 16) {
                enPassantSquare = move.from + (sideToMove == WHITE? -8 : 8);
                // zobrist changes
                int file = enPassantSquare % 8;
                zobristKey ^= zobristEnPassant[file];
            }

            if(move.promotion) {

                if(move.capture) {
                    setPiece(NONE, move.to);
                    // zobrist changes
                    xorPiece(zobristKey, (Piece)move.capture, move.to);
                } 

                setPiece(NONE, move.from);
                Piece promoteTo = sideToMove==WHITE? (Piece)move.promotion : (Piece)(move.promotion+8);
                setPiece(promoteTo, move.to);

                // Zobrist changes
                xorPiece(zobristKey, move.piece, move.from);
                xorPiece(zobristKey, promoteTo, move.to);
            }
            else {
                if(move.capture) xorPiece(zobristKey, (Piece)move.capture, move.to);

                setPiece(NONE, move.from);
                setPiece(move.piece, move.to);

                // Zobrist changes
                xorPiece(zobristKey, move.piece, move.from);
                xorPiece(zobristKey, move.piece, move.to);

            }
        }
        else {
            if(move.capture) xorPiece(zobristKey, (Piece)move.capture, move.to);

                setPiece(NONE, move.from);
                setPiece(move.piece, move.to);

                // Zobrist changes
                xorPiece(zobristKey, move.piece, move.from);
                xorPiece(zobristKey, move.piece, move.to);
        }

        if (move.piece == WK) {
            whiteKingsideCastle = false;
            whiteQueensideCastle = false;
            whiteKingSquare = move.to;
            
        } else if (move.piece == BK) {
            blackKingsideCastle = false;
            blackQueensideCastle = false;
            blackKingSquare = move.to;
        } else if (move.piece == WR) {
            if (move.from == 56) whiteQueensideCastle = false;
            if (move.from == 63) whiteKingsideCastle = false;
        } else if (move.piece == BR) {
            if (move.from == 0) blackQueensideCastle = false;
            if (move.from == 7) blackKingsideCastle = false;
        }
        if (move.capture) {
            if (move.to == 56) whiteQueensideCastle = false;
            if (move.to == 63) whiteKingsideCastle = false;
            if (move.to == 0) blackQueensideCastle = false;
            if (move.to == 7) blackKingsideCastle = false;
        }

        
    }

    // zobrist changes
    if(prevState.whiteKingsideCastle != whiteKingsideCastle) zobristKey ^= zobristCastling[0];
    if(prevState.whiteQueensideCastle != whiteQueensideCastle) zobristKey ^= zobristCastling[1];
    if(prevState.blackKingsideCastle != blackKingsideCastle) zobristKey ^= zobristCastling[2];
    if(prevState.blackQueensideCastle != blackQueensideCastle) zobristKey ^= zobristCastling[3];

    sideToMove = (sideToMove == WHITE)? BLACK : WHITE;
    zobristKey ^= zobristBlackToMove;

    if(sideToMove == BLACK) {
        if(isSquareAttacked(whiteKingSquare, BLACK)){
           
            unmakeMove(move, prevState);
            return false;
        }
    }
    else {
        if(isSquareAttacked(blackKingSquare, WHITE)) {
            unmakeMove(move, prevState);
            return false;
        }
    } 
    return true;
}

void Board::unmakeMove(const Move &move, const Gamestate &prevState) {

    whiteKingsideCastle = prevState.whiteKingsideCastle;
    whiteQueensideCastle = prevState.whiteQueensideCastle;
    blackKingsideCastle = prevState.blackKingsideCastle;
    blackQueensideCastle = prevState.blackQueensideCastle;
    whiteKingSquare = prevState.whiteKingSquare;
    blackKingSquare = prevState.blackKingSquare;
    enPassantSquare = prevState.enPassantSquare;
    sideToMove = (sideToMove == WHITE) ? BLACK : WHITE;


    if (move.isKingSideCastle) {
        if (sideToMove == WHITE) {
            setPiece(WK, 60); 
            setPiece(WR, 63); 
            setPiece(NONE, 62); 
            setPiece(NONE, 61);
        } else {
            setPiece(BK, 4);  
            setPiece(BR, 7);  
            setPiece(NONE, 6); 
            setPiece(NONE, 5);
        }
    }
    else if (move.isQueenSideCastle) {
        if (sideToMove == WHITE) {
            setPiece(WK, 60); 
            setPiece(WR, 56); 
            setPiece(NONE, 58); 
            setPiece(NONE, 59);
        } else {
            setPiece(BK, 4);  
            setPiece(BR, 0);  
            setPiece(NONE, 2); 
            setPiece(NONE, 3);
        }
    }
    else if (move.isEnPassant) {
        setPiece(move.piece, move.from);
        setPiece(NONE, move.to);
        
        int capturedPawnSquare = move.to + (sideToMove == WHITE ? 8 : -8);
        setPiece((sideToMove == WHITE) ? BP : WP, capturedPawnSquare);
    }
    else {
        setPiece(move.piece, move.from);
        if (move.promotion && move.capture) {

            Piece capturedPiece = (Piece)move.capture;
            setPiece(capturedPiece, move.to);

        }
        else if (move.promotion) {
            setPiece(NONE, move.to);

        } else if (move.capture) {

            Piece capturedPiece = (Piece)move.capture;
            setPiece(capturedPiece, move.to);

        } else {

            setPiece(NONE, move.to);

        }
    }

    zobristKey = prevState.zobristKey;
}

bool Board::tryMove(const Move& move) {
    Gamestate prevdata = {
        sideToMove, 
        enPassantSquare,
        whiteKingsideCastle,
        whiteQueensideCastle,
        blackKingsideCastle,
        blackQueensideCastle,
        whiteKingSquare,
        blackKingSquare,
        zobristKey
    };

    if(!makeMove(move)) return false;
    unmakeMove(move, prevdata);
    return true;
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
        if(promotion) {
            Move move1(p, square, newSquare, 5);
            Move move2(p, square, newSquare, 4);
            Move move3(p, square, newSquare, 3);
            Move move4(p, square, newSquare, 2);

            moves.push_back(move1);
            moves.push_back(move2);
            moves.push_back(move3);
            moves.push_back(move4);
        }
        else {
            Move move(p, square, newSquare, 0);
            moves.push_back(move);
        }
    }

    

    // Left Capture
    newSquare = MailBox::mailbox[mailboxIndex + leftCapture];
    if(newSquare != -1) {
        Piece occupiedPiece = getPiece(newSquare);
        bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);

        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
            if(promotion) {
                Move move1(p, square, newSquare, 5, occupiedPiece);
                Move move2(p, square, newSquare, 4, occupiedPiece);
                Move move3(p, square, newSquare, 3, occupiedPiece);
                Move move4(p, square, newSquare, 2, occupiedPiece);

                moves.push_back(move1);
                moves.push_back(move2);
                moves.push_back(move3);
                moves.push_back(move4);
            } else {
                Move move(p, square, newSquare, promotion, occupiedPiece);
                moves.push_back(move);
            }
        }
        
        if(enPassantSquare == newSquare) {
            int capturedPawnSquare = newSquare + (color == WHITE ? 8 : -8);
            Move move(p, square, newSquare, 0, getPiece(capturedPawnSquare), false, false, true);
            moves.push_back(move);
        }

    }

    // Right Capture
    
    newSquare = MailBox::mailbox[mailboxIndex + rightCapture];
    if(newSquare != -1) {
        Piece occupiedPiece = getPiece(newSquare);
        bool promotion = (newSquare >=0 && newSquare <= 7) || (newSquare >= 56 && newSquare <= 63);

        if(occupiedPiece != NONE && getPieceColor(occupiedPiece) != color) {
           if(promotion) {
                Move move1(p, square, newSquare, 5, occupiedPiece);
                Move move2(p, square, newSquare, 4, occupiedPiece);
                Move move3(p, square, newSquare, 3, occupiedPiece);
                Move move4(p, square, newSquare, 2, occupiedPiece);

                moves.push_back(move1);
                moves.push_back(move2);
                moves.push_back(move3);
                moves.push_back(move4);
            } else {
                Move move(p, square, newSquare, promotion, occupiedPiece);
                moves.push_back(move);
            }
        }

        if(enPassantSquare == newSquare) {
            int capturedPawnSquare = newSquare + (color == WHITE ? 8 : -8);
            Move move(p, square, newSquare, 0, getPiece(capturedPawnSquare), false, false, true);
            moves.push_back(move);
        }
    }

    // Forward 2 step movement
    if(color == WHITE && square >= 48 && square <= 55) {
        int intermediateSquare = square - 8;  // One square forward
        newSquare = MailBox::mailbox[mailboxIndex + moveDir + moveDir];
        
        // Check BOTH squares are empty
        if(newSquare != -1 && getPiece(intermediateSquare) == NONE && getPiece(newSquare) == NONE) {
            Move move(p, square, newSquare, 0, 0);
            moves.push_back(move);
        }
    }

    if(color == BLACK && square >= 8 && square <= 15) {
        int intermediateSquare = square + 8;  // One square forward
        newSquare = MailBox::mailbox[mailboxIndex + moveDir + moveDir];
        
        // Check BOTH squares are empty
        if(newSquare != -1 && getPiece(intermediateSquare) == NONE && getPiece(newSquare) == NONE) {
            Move move(p, square, newSquare, 0, 0);
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
                Move move(p, square, newSquare, 0, occupied);
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
                Move move(p, square, newSquare, 0, occupied);
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
                Move move(p, square, newSquare, 0, occupied);
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
                Move move(p, square, newSquare, 0, occupied);
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
                Move move(p, square, newSquare, 0, occupied);
                moves.push_back(move);
                continue;
            }
    }
}

