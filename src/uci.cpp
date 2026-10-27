#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib> 
#include <ctime>
#include <algorithm>
#include <cmath>
#include <thread>
#include "bitboard.h"
#include "search.h"
#include "timeman.h"
#include "opening_book.h"
#include "evaluate.h"

// bro why does c++ not have a built in trim function lol
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

namespace {
constexpr int MAX_GAME_REPETITION_HISTORY = 1024 - MAX_PLY - 1;
bool useOpeningBook = true;

void printUciLine(const std::string& line) {
    std::lock_guard<std::mutex> outputLock(uciOutputMutex);
    std::cout << line << '\n' << std::flush;
}

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
    OpeningBook::initialize();
    std::string line;
    BitBoard board;

    std::vector<std::string> lastUCIMoves;
    std::string lastFEN;
    int gamePly = 0;
    std::thread searchThread;

    auto stopAndJoinSearch = [&]() {
        if (searchThread.joinable()) {
            Search::requestStop();
            searchThread.join();
        }
    };

    while(std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if(token == "uci") {
            printUciLine("id name Ironfangv33");
            printUciLine("id author dark");
            printUciLine("option name UseOpeningBook type check default true");
            printUciLine("uciok");
        }
        else if (token == "setoption") {
            stopAndJoinSearch();
            std::string nameToken;
            std::string optionName;
            std::string valueToken;
            std::string value;
            iss >> nameToken >> optionName >> valueToken >> value;
            if (nameToken == "name" && optionName == "UseOpeningBook" &&
                     valueToken == "value") {
                useOpeningBook = value == "true";
            }
        }
        else if(token == "isready") {
            printUciLine("readyok");
        }
        else if(token == "ucinewgame") {
            stopAndJoinSearch();
            board.setStartPosition();
            lastFEN.clear();
            lastUCIMoves.clear();
            gamePly = 0;
            Search::newGame();
        }
        else if (token == "position") {
            stopAndJoinSearch();
            std::string sub;
            iss >> sub;

            std::string currentFEN;
            std::vector<std::string> currentUCIMoves;
            bool movesAlreadyParsed = false;
            int currentGamePlyBase = 0;

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
                std::istringstream fenFields(currentFEN);
                std::string fenPlacement, fenActive, fenCastling, fenEnPassant;
                int halfmoveClock = 0;
                int fullmoveNumber = 0;
                fenFields >> fenPlacement >> fenActive >> fenCastling >> fenEnPassant;
                fenFields >> halfmoveClock >> fullmoveNumber;
                currentGamePlyBase = (fullmoveNumber - 1) * 2 + (fenActive == "b" ? 1 : 0);
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
                        printUciLine("info string invalid FEN position");
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
            gamePly = currentGamePlyBase + static_cast<int>(currentUCIMoves.size());
        }

        else if(token == "go") {
            stopAndJoinSearch();
            int movetime = -1;
            int requestedDepth = -1;
            int wtime = -1, btime = -1;
            int winc = 0, binc = 0;
            
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
            }
            
            // Allocate time using Stockfish-style TimeManager
            TimeManager timeManager;
            timeManager.init(board.sideToMove, movetime, wtime, btime, winc, binc);
            int maxDepth = requestedDepth > 0 ? std::min(requestedDepth, MAX_DEPTH) : MAX_DEPTH;

            const std::string bookMove = useOpeningBook
                ? OpeningBook::chooseMove(board, gamePly)
                : std::string{};
            if (!bookMove.empty()) {
                printUciLine("bestmove " + bookMove);
                continue;
            }
            
            Search::clearStopRequest();
            searchThread = std::thread([&board, maxDepth, timeManager]() {
                try {
                    const Move bestMove = Search::findBestMove(board, maxDepth, timeManager);
                    printUciLine("bestmove " + moveToUCI(bestMove));
                }
                catch (...) {
                    printUciLine("info string search failed");
                    printUciLine("bestmove 0000");
                }
            });
        }
        else if (token == "stop") {
            stopAndJoinSearch();
        }
        else if (token == "eval") {
            stopAndJoinSearch();
            const Evaluation::Breakdown breakdown = Evaluation::evaluateBreakdown(board);
            std::ostringstream result;
            result << "info string eval cp " << breakdown.score
                   << " white material=" << breakdown.whiteMaterial
                   << " pst=" << breakdown.whitePieceSquare
                   << " pawn=" << breakdown.whitePawn
                   << " shield=" << breakdown.whiteKingSafety
                   << " mopup=" << breakdown.whiteMopUp
                   << " positional=" << breakdown.whitePositional
                   << " black material=" << breakdown.blackMaterial
                   << " pst=" << breakdown.blackPieceSquare
                   << " pawn=" << breakdown.blackPawn
                   << " shield=" << breakdown.blackKingSafety
                   << " mopup=" << breakdown.blackMopUp
                   << " positional=" << breakdown.blackPositional;
            printUciLine(result.str());
        }
        else if (token == "hash") {
            stopAndJoinSearch();
            printUciLine("info string hash " + std::to_string(board.zobristKey));
        }
        else if (token == "see") {
            stopAndJoinSearch();
            std::string moveStr;
            if (iss >> moveStr) {
                Move m = uciToMove(moveStr, board);
                if (m.from >= 0) {
                    bool res = board.see(m.from, m.to, m.piece, m.capture, 0);
                    printUciLine(std::string("info string see ") + (res ? ">= 0" : "< 0"));
                }
            }
        }
        else if (token == "quit") {
            stopAndJoinSearch();
            break;
        }
    }

    stopAndJoinSearch();
}
