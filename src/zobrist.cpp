#include "zobrist.h"
#include <random> 

uint64_t zobristPieces[12][64];
uint64_t zobristCastling[4];
uint64_t zobristEnPassant[8];
uint64_t zobristBlackToMove;


void initZobrist() {
    std::mt19937_64 rng(1337);      // Random number generator
    std::uniform_int_distribution<uint64_t> dist;

    for(int piece = 0; piece < 12; ++piece) {
        for(int square = 0; square < 64; ++square) {
            zobristPieces[piece][square] = dist(rng);
        }
    }

    for(int i = 0; i < 4; ++i) {
        zobristCastling[i] = dist(rng);
    }

    for(int i = 0; i < 8; ++i) {
        zobristEnPassant[i] = dist(rng);
    }

    zobristBlackToMove = dist(rng);

}

uint64_t generateZobristHashKey(Board b)
{
    uint64_t key = 0;

    for(int square = 0; square < 64; ++square) {
        Piece p = b.getPiece(square);
        int index = pieceToZobristIndex(p);

        if(index != -1) {
            key ^= zobristPieces[index][square];
        }
    }

    if(b.whiteKingsideCastle) key ^= zobristCastling[0];
    if(b.whiteQueensideCastle) key ^= zobristCastling[1];
    if(b.blackKingsideCastle) key ^= zobristCastling[2];
    if(b.blackQueensideCastle) key ^= zobristCastling[3];

    if(b.enPassantSquare != -1) {
        int file = b.enPassantSquare % 8;
        key ^= zobristEnPassant[file];
    }

    if(b.sideToMove == BLACK) {
        key ^= zobristBlackToMove;
    }

    return key;
}