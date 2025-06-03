#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib> 
#include <ctime>
#include "board.h"
#include "bitboard.h"
#include "search.h"

// bro why does c++ not have a built in trim function lol
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}


void uciLoop() {
    srand(time(NULL));
    std::string line;
    BitBoard board;

    std::vector<std::string> lastUCIMoves;
    std::string lastFEN;

    while(std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if(token == "uci") {
            std::cout << "id name IronfangT-Tablev5\n" << std::flush;
            std::cout << "id author dark\n" << std::flush;
            std::cout << "uciok\n" << std::flush;
        }
        else if(token == "isready") {
            std::cout << "readyok\n" << std::flush;
        }
        else if(token == "ucinewgame") {
            board.setStartPosition();
        }
        else if (token == "position") {
            std::string sub;
            iss >> sub;

            std::string currentFEN;
            std::vector<std::string> currentUCIMoves;

            if(sub == "startpos") {
                currentFEN = "startpos";
            }
            else if(sub == "fen") {
                // FEN has 6 parts (piece placement, active color, castling,
                // en passant, halfmove clock, fullmove counter)
                std::string fenParts[6];
                for(int i = 0; i < 6 && iss >> fenParts[i]; i++) {}
                // Combine all parts into complete FEN
                currentFEN = fenParts[0];
                for(int i = 1; i < 6; i++) {
                    if(!fenParts[i].empty()) {
                        currentFEN += " " + fenParts[i];
                    }
                }
            }

            // Check for "moves" token
            std::string nextToken;
            if(iss >> nextToken && nextToken == "moves") {
                // Parse all moves
                std::string moveStr;
                while(iss >> moveStr) {
                    currentUCIMoves.push_back(moveStr);
                }
            }

            //  whether this is a continuation of the last position:
            bool fenMatch = (currentFEN == lastFEN);
            bool isContinuation =
                fenMatch &&
                currentUCIMoves.size() >= lastUCIMoves.size() &&
                std::equal(
                    lastUCIMoves.begin(),
                    lastUCIMoves.end(),
                    currentUCIMoves.begin()
                );

            // either replay delta or do a full rebuild:
            if (isContinuation) {
                
                for (size_t i = lastUCIMoves.size();
                    i < currentUCIMoves.size(); ++i)
                {
                    Move m = uciToMove(currentUCIMoves[i], board);
                    board.makeMove(m);
                }
            }
            else {
                // Full rebuild from scratch:
                if (sub == "startpos") {
                    board.setStartPosition();
                }
                else { // sub == "fen"
                    board.setPositionFromFEN(currentFEN);
                }

                for (const std::string &mv : currentUCIMoves) {
                    Move m = uciToMove(mv, board);
                    board.makeMove(m);
                }
            }

            // “new baseline” for the next UCI command:
            lastFEN = currentFEN;
            lastUCIMoves = currentUCIMoves;
        }

        else if(token == "go") {
            int movetime = -1;
            int wtime = -1, btime = -1;
            
            std::string subtoken;
            while (iss >> subtoken) {
                if (subtoken == "movetime") {
                    iss >> movetime;
                }
                else if (subtoken == "wtime") {
                    iss >> wtime;
                }
                else if (subtoken == "btime") {
                    iss >> btime;
                }
            }
             
            Move bestMove = Search::findBestMove(board, MAX_DEPTH, -1);

            if (bestMove.from == -1) {
                std::cout << "bestmove 0000\n" << std::flush;
            } else {
                std::cout << "bestmove " << moveToUCI(bestMove) << "\n" << std::flush;
            }
        }
        else if (token == "quit") {
            break;
        }
    }
}