#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "types.h"

struct Gamestate {
    Color sideToMove;
    int enPassantSquare;

    bool whiteKingsideCastle;
    bool whiteQueensideCastle;
    bool blackKingsideCastle;
    bool blackQueensideCastle;

    int whiteKingSquare;
    int blackKingSquare;
};

#endif