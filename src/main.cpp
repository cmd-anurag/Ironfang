#include "board.h"
#include "mailbox.h"
#include <iostream>
#include<vector>


int main() {
 
    Board b;
    if(b.setPositionFromFEN("4b3/2R3qp/3P1kB1/2P5/Q7/1P5K/4npP1/3R1N2 w - - 0 1")) {
        std::cout << "Loaded Successfully\n";
    }
    else {
        std::cout << "invalid fen";
        return 0;
    }
    
    b.print();
    std::vector<Move> moves = b.generateMoves();
    std::vector<Move> legalMoves;

    for(auto move : moves) {
        std::cout << moveToAlgrebraic(move) << " ";
        Gamestate prevState = {
            b.sideToMove, 
            b.enPassantSquare,
            b.whiteKingsideCastle,
            b.whiteQueensideCastle,
            b.blackKingsideCastle,
            b.blackQueensideCastle,
            b.whiteKingSquare,
            b.blackKingSquare
        };

        if(b.makeMove(move)) {
            legalMoves.push_back(move);
            b.unmakeMove(move, prevState);
        }
    }
    std::cout << "\nLegal Moves\n";

    for(auto legal : legalMoves) {
        std::cout << moveToAlgrebraic(legal) << " ";
    }

    // std::cout << moves.size() << " " << legalMoves.size();
    // BUG - For the above FEN, g4 is not being recognized as a legal move. Though it is being generated as a pseudo legal move. 
    // i'll fix it tommorow.

}