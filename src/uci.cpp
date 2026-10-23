#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib> 
#include <ctime>
#include <algorithm>
#include "board.h"
#include "bitboard.h"
#include "search.h"

// bro why does c++ not have a built in trim function lol
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

namespace {
constexpr int MAX_GAME_REPETITION_HISTORY = 1024 - MAX_PLY - 1;

void recordGamePosition(BitBoard& board, bool irreversible) {
    // A pawn move or capture makes all earlier positions unreachable under
    // legal play, so only retain positions since the latest irreversible move.
    if (irreversible) board.pathDepth = 0;

    // Leave room in the fixed board history array for the search variation.
    if (board.pathDepth >= MAX_GAME_REPETITION_HISTORY) {
        for (int i = 1; i < board.pathDepth; ++i)
            board.repetitionPath[i - 1] = board.repetitionPath[i];
        --board.pathDepth;
    }
    board.repetitionPath[board.pathDepth++] = board.zobristKey;
}

bool applyUciMoveAndRecord(BitBoard& board, const std::string& moveText) {
    Move move = uciToMove(moveText, board);
    if (move.from < 0 || !board.makeMove(move)) return false;

    const bool irreversible = move.capture || move.isEnPassant ||
                              move.piece == WP || move.piece == BP;
    recordGamePosition(board, irreversible);
    return true;
}
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
            std::cout << "id name Ironfangv13\n" << std::flush;
            std::cout << "id author dark\n" << std::flush;
            std::cout << "option name RootDiagnostics type check default false\n" << std::flush;
            std::cout << "uciok\n" << std::flush;
        }
        else if (token == "setoption") {
            std::string nameToken;
            std::string optionName;
            std::string valueToken;
            std::string value;
            iss >> nameToken >> optionName >> valueToken >> value;
            if (nameToken == "name" && optionName == "RootDiagnostics" &&
                valueToken == "value") {
                Search::setRootDiagnostics(value == "true");
            }
        }
        else if(token == "isready") {
            std::cout << "readyok\n" << std::flush;
        }
        else if(token == "ucinewgame") {
            board.setStartPosition();
            lastFEN.clear();
            lastUCIMoves.clear();
        }
        else if (token == "position") {
            std::string sub;
            iss >> sub;

            std::string currentFEN;
            std::vector<std::string> currentUCIMoves;
            bool movesAlreadyParsed = false;

            if(sub == "startpos") {
                currentFEN = "startpos";
            }
            else if(sub == "fen") {
                // The first four FEN fields define the board state used here.
                std::string placement, active, castling, ep;
                if (!(iss >> placement >> active >> castling >> ep)) continue;
                currentFEN = placement + " " + active + " " + castling + " " + ep;
                std::string trailing;
                int clockFields = 0;
                while (iss >> trailing) {
                    if (trailing == "moves") {
                        movesAlreadyParsed = true;
                        break;
                    }
                    if (clockFields++ < 2) currentFEN += " " + trailing;
                }
                if (movesAlreadyParsed) {
                    while (iss >> trailing) currentUCIMoves.push_back(trailing);
                }
            }

            // Check for "moves" token
            std::string nextToken;
            if(!movesAlreadyParsed && iss >> nextToken && nextToken == "moves") {
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
                    applyUciMoveAndRecord(board, currentUCIMoves[i]);
                }
            }
            else {
                // Full rebuild from scratch:
                if (sub == "startpos") {
                    board.setStartPosition();
                }
                else { // sub == "fen"
                    if (!board.setPositionFromFEN(currentFEN)) {
                        std::cout << "info string invalid FEN position\n" << std::flush;
                        board.setStartPosition();
                        lastFEN.clear();
                        lastUCIMoves.clear();
                        continue;
                    }
                }

                // A FEN gives only the current position. For startpos, seed
                // the initial position and then record each successfully
                // replayed move so threefold checks include game history.
                recordGamePosition(board, true);

                for (const std::string &mv : currentUCIMoves) {
                    applyUciMoveAndRecord(board, mv);
                }
            }

            // “new baseline” for the next UCI command:
            lastFEN = currentFEN;
            lastUCIMoves = currentUCIMoves;
        }

        else if(token == "go") {
            int movetime = -1;
            int requestedDepth = -1;
            int wtime = -1, btime = -1;
            int winc = 0, binc = 0;
            int movestogo = 30; // Default assumption
            
            std::string subtoken;
            while (iss >> subtoken) {
                if (subtoken == "movetime") {
                    iss >> movetime;
                }
                else if (subtoken == "depth") {
                    iss >> requestedDepth;
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
                if (timeLeft > 0) timeForMove = std::max(1, timeForMove);
                
                

                // Give extra time in the opening to prepare strategic advantage
                if (pieceCount > 28) {
                    timeForMove = static_cast<int>(timeForMove * 1.3);
                }
            }
            
            // Set a dynamic maximum depth based on time
            int maxDepth = requestedDepth > 0 ? std::min(requestedDepth, MAX_DEPTH) : MAX_DEPTH;
            if (requestedDepth <= 0) {
                if (timeForMove > 0 && timeForMove < 100) {
                    maxDepth = 6;
                } else if (timeForMove > 0 && timeForMove < 500) {
                    maxDepth = 8;
                }
            }
            
            // Log intentions
            // std::cout << "info string Starting search with time=" << timeForMove 
            //           << "ms maxDepth=" << maxDepth << "\n" << std::flush;
            
            try {
                
                Move bestMove = Search::findBestMove(board, maxDepth, timeForMove);
                
                std::cout << "bestmove " << moveToUCI(bestMove) << "\n" << std::flush;
            }
            catch (...) {
                std::cout << "info string search failed\n" << std::flush;
                std::cout << "bestmove 0000\n" << std::flush;
            }
        }
        else if (token == "quit") {
            break;
        }
    }
}
