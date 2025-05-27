#ifndef MOVE_H
#define MOVE_H

struct Move {
    Piece piece;

    int from;
    int to;
    int promotion = 0;
    bool capture = false;

    Move(Piece p, int from, int to, int promotion = 0, bool capture = false) : piece(p), from(from), to(to), promotion(promotion), capture(capture) {};
    
};

#endif