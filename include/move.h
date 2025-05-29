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
    int capture = 0;
    bool isKingSideCastle = false;
    bool isQueenSideCastle = false;
    bool isEnPassant = false;

    Move(Piece p, int from, int to, int promotion = 0, int capture = 0, bool isKingSideCastle = false, bool isQueenSideCastle = false, bool isEnPassant = false) : piece(p), from(from), to(to), promotion(promotion), capture(capture), isKingSideCastle(isKingSideCastle), isQueenSideCastle(isQueenSideCastle), isEnPassant(isEnPassant) {};
};

std::string moveToAlgrebraic(Move move);

#endif