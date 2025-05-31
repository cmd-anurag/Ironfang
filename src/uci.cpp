#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>  // for rand()
#include <ctime>
#include "board.h"
#include "search.h"

void uciLoop() {
    srand(time(NULL));
    std::string line;
    Board board;
    while(std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if(token == "uci") {
            std::cout << "id name IronfangPureAlphaBetav2\n";
            std::cout << "id author dark\n";
            std::cout << "uciok\n";
        }
        else if(token == "isready") {
            std::cout << "readyok\n";
        }
        else if(token == "ucinewgame") {
            board.setStartPosition();
        }
        else if(token == "position") {
            std::string sub;
            iss >> sub;
            if(sub == "startpos") {
                board.setStartPosition();
            }
            else if(sub == "fen") {
                std::string fen;
                std::getline(iss, fen);
                board.setPositionFromFEN(fen);
            }

            std::string moveToken;
            while(iss >> moveToken) {
                if(moveToken == "moves") continue;
                Move move = uciToMove(moveToken, board);
                board.makeMove(move);
            }
        }
        else if(token == "go") {
            Move bestMove = Search::findBestMove(board, 5);
            if (bestMove.from == -1) {
                std::cout << "bestmove 0000\n";
            } else {
                std::cout << "bestmove " << moveToUCI(bestMove) << "\n";
            }
        }
        else if (token == "quit") {
            break;
        }
    }
}