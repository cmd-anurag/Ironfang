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
            std::cout << "id name Ironfangv8\n" << std::flush;
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
            int winc = 0, binc = 0;
            int movestogo = 30; // Default assumption
            
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
                else if (subtoken == "winc") {
                    iss >> winc;
                }
                else if (subtoken == "binc") {
                    iss >> binc;
                }
                else if (subtoken == "movestogo") {
                    iss >> movestogo;
                }
            }
            
            // Enhanced time allocation
            int timeForMove = -1;
            
            if (movetime > 0) {
                timeForMove = movetime;
            }
            else if ((board.sideToMove == WHITE && wtime > 0) || 
                     (board.sideToMove == BLACK && btime > 0)) {
                
                int timeLeft = (board.sideToMove == WHITE) ? wtime : btime;
                int increment = (board.sideToMove == WHITE) ? winc : binc;
                int pieceCount = 0;
                
                // More aggressive time allocation strategy
                if (movestogo > 0) {
                    // If we know moves to go, allocate more time per move
                    timeForMove = (timeLeft / movestogo) + increment;  // Removed +1 from divisor, use full increment
                } else {
                    // Use more aggressive estimates for remaining moves
                    uint64_t allPieces = board.getAllPieces();
                    pieceCount = __builtin_popcountll(allPieces);
                    
                    int estimatedMoves;
                    if (pieceCount > 24) {
                        estimatedMoves = 30; // Early game (was 40)
                    } else if (pieceCount > 12) {
                        estimatedMoves = 20; // Middle game (was 30)
                    } else {
                        estimatedMoves = 12; // Endgame (was 20)
                    }
                    
                    timeForMove = (timeLeft / estimatedMoves) + increment; // Use full increment
                }
                
                // More aggressive safety margin - allow up to 25% of remaining time (was ~14%)
                timeForMove = std::min(timeForMove, timeLeft / 4);
                
                

                // Give extra time in the opening to prepare strategic advantage
                if (pieceCount > 28) {
                    timeForMove = static_cast<int>(timeForMove * 1.3);
                }
            }
            
            // Set a dynamic maximum depth based on time
            int maxDepth = MAX_DEPTH; // Very high base value - time will stop search before this
            if (timeForMove > 0 && timeForMove < 100) {
                maxDepth = 6; // For very fast time controls
            } else if (timeForMove > 0 && timeForMove < 500) {
                maxDepth = 8; // For bullet time controls
            }
            
            // Log intentions
            // std::cout << "info string Starting search with time=" << timeForMove 
            //           << "ms maxDepth=" << maxDepth << "\n" << std::flush;
            
            try {
                
                Move bestMove = Search::findBestMove(board, maxDepth, timeForMove);
                
                if (bestMove.from != -1) {
                    std::cout << "bestmove " << moveToUCI(bestMove) << "\n" << std::flush;
                }
                else {
                    std::cout << "info string Using a1a1 fallback\n" << std::flush;
                    std::cout << "bestmove a1a1\n" << std::flush;
                }
            }
            catch (...) {
                // Emergency fallback
                std::cout << "info string Using a1a1 fallback\n" << std::flush;
                std::cout << "bestmove a1a1\n" << std::flush;
            }
        }
        else if (token == "quit") {
            break;
        }
    }
}