#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"
#include "Gamestate.h"
#include "move.h"
#include <vector>
#include <cstdint>
#include <unordered_map>

class Evaluation;

class BitBoard {
    public:
        Color sideToMove;
        int enPassantSquare = -1;

        bool whiteKingsideCastle;
        bool whiteQueensideCastle;
        bool blackKingsideCastle;
        bool blackQueensideCastle;

        int whiteKingSquare;
        int blackKingSquare;

        uint64_t zobristKey;
        std::unordered_map<uint64_t, int> repetitionMap;

        BitBoard();
        void setStartPosition();
        void print() const;
        inline Piece getPiece(int square) const {
            uint64_t mask = 1ULL << square;
            if (whitePawns & mask) return WP;
            if (blackPawns & mask) return BP;
            if (whiteRooks & mask) return WR;
            if (blackRooks & mask) return BR;
            if (whiteBishops & mask) return WB;
            if (blackBishops & mask) return BB;
            if (whiteQueens & mask) return WQ;
            if (blackQueens & mask) return BQ;
            if (whiteKnights & mask) return WN;
            if (blackKnights & mask) return BN;
            if (whiteKing & mask) return WK;
            if (blackKing & mask) return BK;
            return NONE;
        }
        inline void setPiece(Piece p, int square) {
            uint64_t mask = 1ULL << square;
            clearSquare(square);
            switch(p) {
                case WP: whitePawns |= mask; break;
                case BP: blackPawns |= mask; break;
                case WR: whiteRooks |= mask; break;
                case BR: blackRooks |= mask; break;
                case WB: whiteBishops |= mask; break;
                case BB: blackBishops |= mask; break;
                case WQ: whiteQueens |= mask; break;
                case BQ: blackQueens |= mask; break;
                case WN: whiteKnights |= mask; break;
                case BN: blackKnights |= mask; break;
                case WK: whiteKing |= mask; break;
                case BK: blackKing |= mask; break;
                default: break;
            }
        }
        bool setPositionFromFEN(const std::string& fen);
        std::vector<Move> generateMoves() const;
        std::vector<Move> generateCaptures() const;
        
        bool isSquareAttacked(int square, Color opponentColor) const;

        bool makeMove(const Move &move);
        void unmakeMove(const Move &move, const Gamestate &prevState);
        bool tryMove(const Move& move);
        
        uint64_t getWhitePieces() const { return whitePawns | whiteRooks | whiteBishops | whiteQueens | whiteKnights | whiteKing; }

        uint64_t getBlackPieces() const { return blackPawns | blackRooks | blackBishops | blackQueens | blackKnights | blackKing; }

        inline int getLSB(uint64_t bb) const {
            return __builtin_ctzll(bb);
        }

        inline int popLSB(uint64_t& bb) const {
            int lsb = getLSB(bb);
            bb &= bb - 1;
            return lsb;
        }

        friend class Evaluation;

        inline uint64_t getWhitePawns() const {
            return whitePawns;
        }
        inline uint64_t getWhiteBishops() const {
            return whiteBishops;
        }
        inline uint64_t getWhiteRooks() const {
            return whiteRooks;
        }
        inline uint64_t getBlackPawns() const {
            return blackPawns;
        }
        inline uint64_t getBlackBishops() const {
            return blackBishops;
        }
        inline uint64_t getBlackRooks() const {
            return blackRooks;
        }
        inline uint64_t getWhiteQueens() const {
            return whiteQueens;
        }
        inline uint64_t getBlackQueens() const {
            return blackQueens;
        }
        inline uint64_t getWhiteKnights() const {
            return whiteKnights;
        }
        inline uint64_t getBlackKnights() const {
            return blackKnights;
        }
        inline uint64_t getAllPieces() const { return getWhitePieces() | getBlackPieces(); }

    private:
        uint64_t whitePawns, blackPawns;
        uint64_t whiteRooks, blackRooks;
        uint64_t whiteBishops, blackBishops;
        uint64_t whiteQueens, blackQueens;
        uint64_t whiteKnights, blackKnights;
        uint64_t whiteKing, blackKing;

        
        
        inline void clearSquare(int square) {
            uint64_t mask = ~(1ULL << square);
            whitePawns &= mask;
            blackPawns &= mask;
            whiteRooks &= mask;
            blackRooks &= mask;
            whiteBishops &= mask;
            blackBishops &= mask;
            whiteQueens &= mask;
            blackQueens &= mask;
            whiteKnights &= mask;
            blackKnights &= mask;
            whiteKing &= mask;
            blackKing &= mask;
        }

        void generatePawnMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateRookMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateBishopMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateQueenMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateKingMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        void generateKnightMoves(int square, std::vector<Move>& moves, Piece p, Color color) const;
        
        // Bitboard utility functions
        uint64_t getRookAttacks(int square, uint64_t occupied) const;
        uint64_t getBishopAttacks(int square, uint64_t occupied) const;
        uint64_t getQueenAttacks(int square, uint64_t occupied) const;
        uint64_t getKnightAttacks(int square) const;
        uint64_t getKingAttacks(int square) const;
        uint64_t getPawnAttacks(int square, Color color) const;
        
};

#endif