#ifndef MOVE_H
#define MOVE_H
#include <string>
#include "types.h"

struct Move
{
    Piece piece;

    int from;
    int to;
    int promotion = 0;
    bool capture = false;
    bool isKingSideCastle = false;
    bool isQueenSideCastle = false;

    Move(Piece p, int from, int to, int promotion = 0, bool capture = false, bool isKingSideCastle = false, bool isQueenSideCastle = false) : piece(p), from(from), to(to), promotion(promotion), capture(capture), isKingSideCastle(isKingSideCastle), isQueenSideCastle(isQueenSideCastle) {};
};

std::string moveToAlgrebraic(Move move);

#endif