#include "board.h"
#include "evaluate.h"
#include "search.h"
#include "mailbox.h"
#include "uci.h"
#include <iostream>
#include<vector>


int main() {

    uciLoop();
    
    // Board b;
    // b.setStartPosition();

    // Move bestMove = Search::findBestMove(b, 6);
    // std::cout << '\n' << bestMove << '\n';
}