#include "board.h"
#include "mailbox.h"
#include <iostream>
#include<vector>


int main() {
 
    Board b;

    for(int i = 0; i < 64; ++i) {
        b.setPiece(NONE, i);
    }
    // b.sideToMove = BLACK;

    b.setPiece(WK, 60);
    b.setPiece(WR, 63);

    b.setPiece(BR, 7);

    b.print();

    std::vector<Move> moves = b.generateMoves();
    if(moves.empty()) {
        std::cout << "what the fuck";
    }
    for(auto move : moves) {
        // std::cout << "FROM: " << move.from << '\n';
        // std::cout << "TO: " << move.to << '\n';
        // std::cout << "CAPTURE: " << move.capture << '\n';
        // std::cout << "PROMOTION: " << move.promotion << '\n';
        std::cout << moveToAlgrebraic(move) << " ";
    }

}