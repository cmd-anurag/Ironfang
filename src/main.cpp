#include "board.h"
#include "mailbox.h"
#include "uci.h"
#include <iostream>
#include<vector>


int main() {

    uciLoop();
    
    // Board b;
    // if(b.setPositionFromFEN("r1bqk2r/2pp1ppp/p1n2n2/1pb1p3/4P3/1BPP1N2/PP3PPP/RNBQK2R b KQkq - 1 7")) {
    //     std::cout << "Loaded Successfully\n";
    // }
    // else {
    //     std::cout << "invalid fen";
    //     return 0;
    // }
    
    // b.print();
    // std::vector<Move> moves = b.generateMoves();
    // std::vector<Move> legalMoves;

    
    // for(auto move : moves) {
    //     std::cout << moveToAlgrebraic(move) << " ";
    //     Gamestate prevState = {
    //         b.sideToMove, 
    //         b.enPassantSquare,
    //         b.whiteKingsideCastle,
    //         b.whiteQueensideCastle,
    //         b.blackKingsideCastle,
    //         b.blackQueensideCastle,
    //         b.whiteKingSquare,
    //         b.blackKingSquare
    //     };

    //     if(b.makeMove(move)) {
    //         legalMoves.push_back(move);
    //         b.unmakeMove(move, prevState);
    //     }
    // }
    // std::cout << "\n\nLegal Moves\n";

    // for(auto legal : legalMoves) {
    //     std::cout << moveToAlgrebraic(legal) << " ";
    // }
    // std::cout << '\n';
    // std::cout << moves.size() << " " << legalMoves.size();

}