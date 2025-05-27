#include "board.h"
#include "mailbox.h"
#include <iostream>
#include<vector>


int main() {
 
    Board b;

    for(int i = 0; i < 64; ++i) {
        b.setPiece(NONE, i);
    }

    ;
    b.setPiece(BP, 63);

    b.sideToMove = BLACK;
    b.print();


    std::vector<Move> moves = b.generateMoves();
    
    for(auto move : moves) {
        std::cout << "FROM: " << move.from << '\n';
        std::cout << "TO: " << move.to << '\n';
        std::cout << "CAPTURE: " << move.capture << '\n';
        std::cout << "PROMOTION: " << move.promotion << '\n';
    }

}