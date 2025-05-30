#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>  // for rand()
#include <ctime>
#include "board.h"

void uciLoop() {
    srand(time(NULL));
    std::string line;
    Board board;
    while(std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if(token == "uci") {
            std::cout << "id name Ironfangv1\n";
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
            
            std::vector<Move> moves = board.generateMoves();
            if(!moves.empty()) {
                Move randomMove = moves.at(0);
                while(true) {
                    int index = rand() % moves.size();
                    randomMove = moves.at(index);

                    Gamestate prevdata = {
                        board.sideToMove, 
                        board.enPassantSquare,
                        board.whiteKingsideCastle,
                        board.whiteQueensideCastle,
                        board.blackKingsideCastle,
                        board.blackQueensideCastle,
                        board.whiteKingSquare,
                        board.blackKingSquare
                    };

                    bool ok = board.makeMove(randomMove);
                    if(ok) {
                        board.unmakeMove(randomMove, prevdata);
                        std::cout << "bestmove " << moveToUCI(randomMove) << "\n";
                        break;
                    }
                }
            }
            else {
                std::cout << "bestmove 0000\n"; // No legal move
            }
        }
        else if (token == "quit") {
            break;
        }
    }

}