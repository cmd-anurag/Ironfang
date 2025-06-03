#ifndef MOVE_H
#define MOVE_H
#include <string>
#include <iostream>
#include "types.h"

class Board;
class BitBoard;

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

    int heuristicScore = 0;

    Move() {};

    Move(Piece p, int from, int to, int promotion = 0, int capture = 0, bool isKingSideCastle = false, bool isQueenSideCastle = false, bool isEnPassant = false) : piece(p), from(from), to(to), promotion(promotion), capture(capture), isKingSideCastle(isKingSideCastle), isQueenSideCastle(isQueenSideCastle), isEnPassant(isEnPassant) {};
};

std::string moveToAlgrebraic(Move move);
std::string moveToUCI(Move move);
Move uciToMove(const std::string& uci, BitBoard &board);
std::ostream& operator<<(std::ostream& os, const Move &move);
bool operator==(const Move &lhs, const Move &rhs);

#endif