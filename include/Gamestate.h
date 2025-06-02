#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "types.h"
#include <cstdint>

struct Gamestate {
    Color sideToMove;
    int enPassantSquare;

    bool whiteKingsideCastle;
    bool whiteQueensideCastle;
    bool blackKingsideCastle;
    bool blackQueensideCastle;

    int whiteKingSquare;
    int blackKingSquare;
    uint64_t zobristKey;
};

#endif