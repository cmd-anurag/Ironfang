#include "bitboard.h"
#include "zobrist.h"
#include "magic.h"
#include<iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <cstdint>

// Precomputed attack tables
static uint64_t knightAttacks[64];
static uint64_t kingAttacks[64];
static uint64_t pawnAttacks[2][64]; // [color][square]


void initAttackTables() {
    // Initialize knight attacks
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t attacks = 0ULL;
        int rank = sq / 8, file = sq & 7;
        
        int knightMoves[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
        for (auto& move : knightMoves) {
            int newRank = rank + move[0], newFile = file + move[1];
            if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                attacks |= 1ULL << (newRank * 8 + newFile);
            }
        }
        knightAttacks[sq] = attacks;
    }
    
    // Initialize king attacks
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t attacks = 0ULL;
        int rank = sq / 8, file = sq & 7;
        
        for (int dr = -1; dr <= 1; ++dr) {
            for (int df = -1; df <= 1; ++df) {
                if (dr == 0 && df == 0) continue;
                int newRank = rank + dr, newFile = file + df;
                if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                    attacks |= 1ULL << (newRank * 8 + newFile);
                }
            }
        }
        kingAttacks[sq] = attacks;
    }
    
    // Initialize pawn attacks
    for (int sq = 0; sq < 64; ++sq) {
        int rank = sq / 8, file = sq & 7;
        
        // White pawn attacks
        if (rank > 0) {
            if (file > 0) pawnAttacks[WHITE][sq] |= 1ULL << ((rank-1) * 8 + file-1);
            if (file < 7) pawnAttacks[WHITE][sq] |= 1ULL << ((rank-1) * 8 + file+1);
        }
        
        // Black pawn attacks  
        if (rank < 7) {
            if (file > 0) pawnAttacks[BLACK][sq] |= 1ULL << ((rank+1) * 8 + file-1);
            if (file < 7) pawnAttacks[BLACK][sq] |= 1ULL << ((rank+1) * 8 + file+1);
        }
    }
}

BitBoard::BitBoard() {
    static bool tablesInitialized = false;
    if (!tablesInitialized) {
        initAttackTables();
        tablesInitialized = true;
    }
    initMagicTables();
    initZobrist();
    setStartPosition();
}

void BitBoard::setStartPosition() {
    // Clear all bitboards
    whitePawns = blackPawns = 0;
    whiteRooks = blackRooks = 0;
    whiteBishops = blackBishops = 0;
    whiteQueens = blackQueens = 0;
    whiteKnights = blackKnights = 0;
    whiteKing = blackKing = 0;

    // Set starting position
    whiteRooks = 0x8100000000000000ULL;    // a1, h1
    whiteKnights = 0x4200000000000000ULL;  // b1, g1
    whiteBishops = 0x2400000000000000ULL;  // c1, f1
    whiteQueens = 0x0800000000000000ULL;   // d1
    whiteKing = 0x1000000000000000ULL;     // e1
    whitePawns = 0x00FF000000000000ULL;    // rank 2

    blackRooks = 0x0000000000000081ULL;    // a8, h8
    blackKnights = 0x0000000000000042ULL;  // b8, g8
    blackBishops = 0x0000000000000024ULL;  // c8, f8
    blackQueens = 0x0000000000000008ULL;   // d8
    blackKing = 0x0000000000000010ULL;     // e8
    blackPawns = 0x000000000000FF00ULL;    // rank 7

    sideToMove = WHITE;
    whiteKingsideCastle = true;
    whiteQueensideCastle = true;
    blackKingsideCastle = true;
    blackQueensideCastle = true;

    whiteKingSquare = 60; // e1
    blackKingSquare = 4;  // e8

    zobristKey = generateZobristHashKey(*this);
}





uint64_t BitBoard::getRookAttacks(int square, uint64_t occupied) const {
    return ::getRookAttacks(square, occupied); // use magic-based version
}

uint64_t BitBoard::getBishopAttacks(int square, uint64_t occupied) const {
    return ::getBishopAttacks(square, occupied); // use magic-based version
}

uint64_t BitBoard::getQueenAttacks(int square, uint64_t occupied) const {
    return getRookAttacks(square, occupied) | getBishopAttacks(square, occupied);
}

uint64_t BitBoard::getKnightAttacks(int square) const {
    return knightAttacks[square];
}

uint64_t BitBoard::getKingAttacks(int square) const {
    return kingAttacks[square];
}

uint64_t BitBoard::getPawnAttacks(int square, Color color) const {
    return pawnAttacks[color][square];
}

bool BitBoard::isSquareAttacked(int square, Color opponentColor) const {
    uint64_t occupied = getAllPieces();
    
    // Check pawn attacks
    uint64_t opponentPawns = (opponentColor == WHITE) ? whitePawns : blackPawns;
    if (getPawnAttacks(square, static_cast<Color>(1 - opponentColor)) & opponentPawns) return true;
    
    // Check knight attacks
    uint64_t opponentKnights = (opponentColor == WHITE) ? whiteKnights : blackKnights;
    if (getKnightAttacks(square) & opponentKnights) return true;
    
    // Check bishop/queen attacks
    uint64_t opponentBishops = (opponentColor == WHITE) ? (whiteBishops | whiteQueens) : (blackBishops | blackQueens);
    if (getBishopAttacks(square, occupied) & opponentBishops) return true;
    
    // Check rook/queen attacks
    uint64_t opponentRooks = (opponentColor == WHITE) ? (whiteRooks | whiteQueens) : (blackRooks | blackQueens);
    if (getRookAttacks(square, occupied) & opponentRooks) return true;
    
    // Check king attacks
    uint64_t opponentKing = (opponentColor == WHITE) ? whiteKing : blackKing;
    if (getKingAttacks(square) & opponentKing) return true;
    
    return false;
}

std::vector<Move> BitBoard::generateMoves() const {
    std::vector<Move> moves;
    moves.reserve(218);
    
    uint64_t pieces = (sideToMove == WHITE) ? getWhitePieces() : getBlackPieces();
    
    while (pieces) {
        int square = popLSB(pieces);
        Piece piece = getPiece(square);
        
        switch (piece) {
            case WP: case BP:
                generatePawnMoves(square, moves, piece, sideToMove);
                break;
            case WR: case BR:
                generateRookMoves(square, moves, piece, sideToMove);
                break;
            case WB: case BB:
                generateBishopMoves(square, moves, piece, sideToMove);
                break;
            case WQ: case BQ:
                generateQueenMoves(square, moves, piece, sideToMove);
                break;
            case WK: case BK:
                generateKingMoves(square, moves, piece, sideToMove);
                break;
            case WN: case BN:
                generateKnightMoves(square, moves, piece, sideToMove);
                break;
            default:
                break;
        }
    }
    
    return moves;
}

std::vector<Move> BitBoard::generateCaptures() const
{
    std::vector<Move> captures;
    captures.reserve(36);

    Color color = sideToMove;
    uint64_t pieces = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    uint64_t occupied = getAllPieces();
    
    // For each piece of the side to move
    while (pieces) {
        int square = popLSB(pieces);
        Piece piece = getPiece(square);
        
        switch (piece) {
            case WP: case BP: {
                // Pawn captures (including promotions and en passant)
                int promotionRank = (color == WHITE) ? 0 : 7;
                
                // Regular pawn captures
                uint64_t attacks = getPawnAttacks(square, color) & enemies;
                
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    
                    if (targetSquare / 8 == promotionRank) {
                        // Promotion captures
                        Piece baseQueen = (color == WHITE) ? WQ : BQ;
                        Piece baseRook = (color == WHITE) ? WR : BR;
                        Piece baseBishop = (color == WHITE) ? WB : BB;
                        Piece baseKnight = (color == WHITE) ? WN : BN;
                        
                        captures.push_back(Move(piece, square, targetSquare, baseQueen, captured));
                        captures.push_back(Move(piece, square, targetSquare, baseRook, captured));
                        captures.push_back(Move(piece, square, targetSquare, baseBishop, captured));
                        captures.push_back(Move(piece, square, targetSquare, baseKnight, captured));
                    } else {
                        captures.push_back(Move(piece, square, targetSquare, 0, captured));
                    }
                }
                
                // En passant capture
                if (enPassantSquare != -1) {
                    if (getPawnAttacks(square, color) & (1ULL << enPassantSquare)) {
                        Piece capturedPawn = (color == WHITE) ? BP : WP;
                        captures.push_back(Move(piece, square, enPassantSquare, 0, capturedPawn, false, false, true));
                    }
                }
                break;
            }
                
            case WR: case BR: {
                uint64_t attacks = getRookAttacks(square, occupied) & enemies;
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    captures.push_back(Move(piece, square, targetSquare, 0, captured));
                }
                break;
            }
                
            case WB: case BB: {
                uint64_t attacks = getBishopAttacks(square, occupied) & enemies;
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    captures.push_back(Move(piece, square, targetSquare, 0, captured));
                }
                break;
            }
                
            case WQ: case BQ: {
                uint64_t attacks = getQueenAttacks(square, occupied) & enemies;
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    captures.push_back(Move(piece, square, targetSquare, 0, captured));
                }
                break;
            }
                
            case WN: case BN: {
                uint64_t attacks = getKnightAttacks(square) & enemies;
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    captures.push_back(Move(piece, square, targetSquare, 0, captured));
                }
                break;
            }
                
            case WK: case BK: {
                uint64_t attacks = getKingAttacks(square) & enemies;
                while (attacks) {
                    int targetSquare = popLSB(attacks);
                    Piece captured = getPiece(targetSquare);
                    captures.push_back(Move(piece, square, targetSquare, 0, captured));
                }
                break;
            }
                
            default:
                break;
        }
    }
    return captures;
}

void BitBoard::generatePawnMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t occupied = getAllPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    
    int forward = (color == WHITE) ? -8 : 8;
    int startRank = (color == WHITE) ? 6 : 1;
    int promotionRank = (color == WHITE) ? 0 : 7;
    
    // Forward moves
    int frontSquare = square + forward;
    if (frontSquare >= 0 && frontSquare < 64 && !(occupied & (1ULL << frontSquare))) {
        if (frontSquare / 8 == promotionRank) {
            // Promotion - (piece, from, to, promotion, capture, kCastle, qCastle, enPassant)
            // Correct version:
            Piece baseQueen = (color == WHITE) ? WQ : BQ;
            Piece baseRook = (color == WHITE) ? WR : BR;
            Piece baseBishop = (color == WHITE) ? WB : BB;
            Piece baseKnight = (color == WHITE) ? WN : BN;

            moves.push_back(Move(p, square, frontSquare, baseQueen, 0));
            moves.push_back(Move(p, square, frontSquare, baseRook, 0));
            moves.push_back(Move(p, square, frontSquare, baseBishop, 0));
            moves.push_back(Move(p, square, frontSquare, baseKnight, 0));
        } else {
            moves.push_back(Move(p, square, frontSquare));
            
            // Double push from starting position
            if (square / 8 == startRank) {
                int doublePush = frontSquare + forward;
                if (!(occupied & (1ULL << doublePush))) {
                    moves.push_back(Move(p, square, doublePush));
                }
            }
        }
    }
    
    // Captures
    uint64_t attacks = getPawnAttacks(square, color);
    uint64_t attacksCopy = attacks;
    while (attacksCopy) {
        int targetSquare = popLSB(attacksCopy);
        if (enemies & (1ULL << targetSquare)) {
            Piece captured = getPiece(targetSquare);
            if (targetSquare / 8 == promotionRank) {
                // Promotion with capture
                Piece baseQueen = (color == WHITE) ? WQ : BQ;
                Piece baseRook = (color == WHITE) ? WR : BR;
                Piece baseBishop = (color == WHITE) ? WB : BB;
                Piece baseKnight = (color == WHITE) ? WN : BN;
                
                moves.push_back(Move(p, square, targetSquare, baseQueen, captured));
                moves.push_back(Move(p, square, targetSquare, baseRook, captured));
                moves.push_back(Move(p, square, targetSquare, baseBishop, captured));
                moves.push_back(Move(p, square, targetSquare, baseKnight, captured));
            } else {
                moves.push_back(Move(p, square, targetSquare, 0, captured));
            }
        }
    }
    
    // En passant
    if (enPassantSquare != -1) {
        uint64_t epAttacks = getPawnAttacks(square, color);
        if (epAttacks & (1ULL << enPassantSquare)) {
            // (piece, from, to, promotion, capture, kCastle, qCastle, enPassant)
            moves.push_back(Move(p, square, enPassantSquare, 0, 0, false, false, true));
        }
    }
}

void BitBoard::generateRookMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t occupied = getAllPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    uint64_t allies = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    
    uint64_t attacks = getRookAttacks(square, occupied);
    attacks &= ~allies; // Remove squares occupied by own pieces
    
    while (attacks) {
        int targetSquare = popLSB(attacks);
        Piece captured = (enemies & (1ULL << targetSquare)) ? getPiece(targetSquare) : NONE;
        moves.push_back(Move(p, square, targetSquare, 0, captured));
    }
}

void BitBoard::generateBishopMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t occupied = getAllPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    uint64_t allies = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    
    uint64_t attacks = getBishopAttacks(square, occupied);
    attacks &= ~allies; // Remove squares occupied by own pieces
    
    while (attacks) {
        int targetSquare = popLSB(attacks);
        Piece captured = (enemies & (1ULL << targetSquare)) ? getPiece(targetSquare) : NONE;
        moves.push_back(Move(p, square, targetSquare, 0, captured));
    }
}

void BitBoard::generateQueenMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t occupied = getAllPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    uint64_t allies = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    
    uint64_t attacks = getQueenAttacks(square, occupied);
    attacks &= ~allies; // Remove squares occupied by own pieces
    
    while (attacks) {
        int targetSquare = popLSB(attacks);
        Piece captured = (enemies & (1ULL << targetSquare)) ? getPiece(targetSquare) : NONE;
        moves.push_back(Move(p, square, targetSquare, 0, captured));
    }
}

void BitBoard::generateKnightMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t allies = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    
    uint64_t attacks = getKnightAttacks(square);
    attacks &= ~allies; // Remove squares occupied by own pieces
    
    while (attacks) {
        int targetSquare = popLSB(attacks);
        Piece captured = (enemies & (1ULL << targetSquare)) ? getPiece(targetSquare) : NONE;
        moves.push_back(Move(p, square, targetSquare, 0, captured));
    }
}

void BitBoard::generateKingMoves(int square, std::vector<Move>& moves, Piece p, Color color) const {
    uint64_t allies = (color == WHITE) ? getWhitePieces() : getBlackPieces();
    uint64_t enemies = (color == WHITE) ? getBlackPieces() : getWhitePieces();
    
    uint64_t attacks = getKingAttacks(square);
    attacks &= ~allies; // Remove squares occupied by own pieces
    
    while (attacks) {
        int targetSquare = popLSB(attacks);
        Piece captured = (enemies & (1ULL << targetSquare)) ? getPiece(targetSquare) : NONE;
        moves.push_back(Move(p, square, targetSquare, 0, captured));
    }
    
    // Castling moves
    if (color == WHITE) {
        // White kingside castling
        if (whiteKingsideCastle && square == 60) {
            if (!(getAllPieces() & 0x6000000000000000ULL)) { // f1 and g1 empty
                if (!isSquareAttacked(60, BLACK) && !isSquareAttacked(61, BLACK) && !isSquareAttacked(62, BLACK)) {
                    moves.push_back(Move(p, square, 62, 0, 0, true, false, false));
                }
            }
        }
        // White queenside castling
        if (whiteQueensideCastle && square == 60) {
            if (!(getAllPieces() & 0x0E00000000000000ULL)) { // b1, c1, d1 empty
                if (!isSquareAttacked(60, BLACK) && !isSquareAttacked(59, BLACK) && !isSquareAttacked(58, BLACK)) {
                    moves.push_back(Move(p, square, 58, 0, 0, false, true, false));
                }
            }
        }
    } else {
        // Black kingside castling
        if (blackKingsideCastle && square == 4) {
            if (!(getAllPieces() & 0x0000000000000060ULL)) { // f8 and g8 empty
                if (!isSquareAttacked(4, WHITE) && !isSquareAttacked(5, WHITE) && !isSquareAttacked(6, WHITE)) {
                    moves.push_back(Move(p, square, 6, 0, 0, true, false, false));
                }
            }
        }
        // Black queenside castling
        if (blackQueensideCastle && square == 4) {
            if (!(getAllPieces() & 0x000000000000000EULL)) { // b8, c8, d8 empty
                if (!isSquareAttacked(4, WHITE) && !isSquareAttacked(3, WHITE) && !isSquareAttacked(2, WHITE)) {
                    moves.push_back(Move(p, square, 2, 0, 0, false, true, false));
                }
            }
        }
    }
}

bool BitBoard::makeMove(const Move &move) {
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
        int file = enPassantSquare & 7;
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
                int file = enPassantSquare & 7;
                zobristKey ^= zobristEnPassant[file];
            }

            if(move.promotion) {
                if(move.capture) {
                    setPiece(NONE, move.to);
                    // zobrist changes
                    xorPiece(zobristKey, (Piece)move.capture, move.to);
                } 

                setPiece(NONE, move.from);
                // The promotion piece is already correct from move generation
                Piece promoteTo = (Piece)move.promotion;
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
    repetitionMap[zobristKey]++;

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

void BitBoard::unmakeMove(const Move &move, const Gamestate &prevState) {
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
    if (--repetitionMap[zobristKey] == 0)
        repetitionMap.erase(zobristKey);
        
    zobristKey = prevState.zobristKey;
}

bool BitBoard::tryMove(const Move& move) {
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

void BitBoard::print() const {
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

bool BitBoard::setPositionFromFEN(const std::string& fen) {
    // Clear all bitboards
    whitePawns = blackPawns = 0;
    whiteRooks = blackRooks = 0;
    whiteBishops = blackBishops = 0;
    whiteQueens = blackQueens = 0;
    whiteKnights = blackKnights = 0;
    whiteKing = blackKing = 0;
    
    std::istringstream ss(fen);
    std::string token;
    
    // 1. Piece placement
    if (!(ss >> token)) return false;
    
    int square = 0;
    for (char c : token) {
        if (c == '/') {
            continue;
        } else if (isdigit(c)) {
            square += (c - '0');
        } else {
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
                default: return false;
            }
            
            if (p != NONE) {
                setPiece(p, square);
                
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