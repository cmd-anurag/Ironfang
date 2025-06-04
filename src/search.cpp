// search.cpp
#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>


bool timeUP = false;
std::chrono::steady_clock::time_point startTime;
constexpr int INF = 1000000;

std::vector<std::vector<Move>> killerMoves(MAX_DEPTH, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<int, 64>, 64> historyHeuristics{};
TranspositionTable TT;

uint64_t nodeCount = 0;

Move Search::findBestMove(BitBoard& board, int maxDepth, int timeLimit) {
    Move bestMove{NONE, -1, -1};
    nodeCount = 0;
    TT.hitCount = 0;
    TT.lookupCount = 0;
    timeUP = false;

    // startTime = std::chrono::steady_clock::now();

    // std::vector<Move> initialMoves = board.generateMoves();
    // for (const Move& move : initialMoves) {
    //     // Find the first legal move as a fallback
    //     Gamestate prevdata = {
    //         board.sideToMove, board.enPassantSquare, board.whiteKingsideCastle,
    //         board.whiteQueensideCastle, board.blackKingsideCastle, board.blackQueensideCastle,
    //         board.whiteKingSquare, board.blackKingSquare, board.zobristKey
    //     };
        
    //     if (board.makeMove(move)) {
    //         board.unmakeMove(move, prevdata);
    //         bestMove = move; // Store the first legal move as fallback
    //         break;
    //     }
    // }

    // Iterative Deepening Search from depth 1 to maxDepth
    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        int alpha = -INF;
        int beta = INF;
        int bestScore = -INF;
        bool foundLegalMove = false;
        
        std::vector<Move> moves = board.generateMoves();
        
        if (moves.empty()) {
            return Move{NONE, -1, -1};
        }
        
        // If engine gets a best move from previous iteration, search it first
        if (!(bestMove == Move{NONE, -1, -1})) {
            auto it = std::find(moves.begin(), moves.end(), bestMove);
            if (it != moves.end()) {
                std::swap(*it, moves.front());
            }
        }

        for (const Move& move : moves) {
            Gamestate prevdata = {
                board.sideToMove,
                board.enPassantSquare,
                board.whiteKingsideCastle,
                board.whiteQueensideCastle,
                board.blackKingsideCastle,
                board.blackQueensideCastle,
                board.whiteKingSquare,
                board.blackKingSquare,
                board.zobristKey
            };

            if (!board.makeMove(move)) {
                continue;
            }

            int score = -minimaxAlphaBeta(board, currentDepth - 1, -beta, -alpha);
            board.unmakeMove(move, prevdata);

            // if (timeLimit > 0) {
            //     auto currentTime = std::chrono::steady_clock::now();
            //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            //         currentTime - startTime).count();
                    
            //     if (elapsed >= timeLimit) {
            //         timeUP = true;
            //         break;
            //     }
            // }

            if (!foundLegalMove || score > bestScore) {
                bestScore = score;
                bestMove = move;
                foundLegalMove = true;
            }
            
            if (score > alpha) {
                alpha = score;
            }
        }

        if (!foundLegalMove) {
            return Move{NONE, -1, -1};
        }

        // if(timeUP) break;
        
        // Display info after each depth iteration
        // double hitRate = (TT.lookupCount == 0) ? 0.0 : 
        //     (100.0 * double(TT.hitCount) / double(TT.lookupCount));
            
        // std::cerr << "Depth " << currentDepth 
        //           << " score: " << bestScore 
        //           << " nodes: " << nodeCount
        //           << " hit rate: " << hitRate << "%" << std::endl << std::endl;
    }

    // Final statistics
    std::cerr << "Nodes searched: " << nodeCount << '\n';
    double hitRate = (TT.lookupCount == 0) ? 0.0 : 
                  (100.0 * double(TT.hitCount) / double(TT.lookupCount));
    std::cerr << "TT lookups: " << TT.lookupCount
              << "  hits: " << TT.hitCount
              << "  hit rate: " << hitRate << "%" << std::endl;

    std::cerr << "Total Store Attempts: " << TT.totalStoreAttempts << "\n";
    std::cerr << "Actual Stores:        " << TT.actualStores << "\n";
    std::cerr << "Overwritten Entries:  " << TT.overwritten << "\n";
    std::cerr << "Currently Occupied:   " << TT.countOccupied() << "\n";
    std::cerr << "TT Utilization:       " << (100.0 * TT.countOccupied() / TT_SIZE) << "%\n";

    return bestMove;
}

int Search::minimaxAlphaBeta(BitBoard& board, int depth, int alpha, int beta) {
    ++nodeCount; // an efficiency metric

    // before doing anything check T-table
    Move tempMove = Move(NONE, -1, -1);
    int tempEval;
    if(TT.probe(board.zobristKey, depth, alpha, beta, tempEval, tempMove)) {
        return tempEval;
    }

    int kingSq = (board.sideToMove == WHITE) ? board.whiteKingSquare : board.blackKingSquare;
    Color opponent = (board.sideToMove == WHITE) ? BLACK : WHITE;
    bool inCheck = board.isSquareAttacked(kingSq, opponent);

    if (depth >= 3 && !inCheck && hasNonPawnMaterial(board, board.sideToMove)) {
        // Make a null move (skip turn)
        Gamestate prevState = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey
        };
        
        // Skip turn by just changing side to move and zobrist key
        board.sideToMove = (board.sideToMove == WHITE) ? BLACK : WHITE;
        board.zobristKey ^= zobristBlackToMove;
        board.enPassantSquare = -1; // Reset en passant
        
        // Reduced depth for null-move - R=2 works well as a starting point
        int R = 2;
        if (depth > 6) R = 3; // Deeper reduction for deeper searches
        
        // Search with reduced depth
        int nullMoveScore = -minimaxAlphaBeta(board, depth - 1 - R, -beta, -beta + 1);
        
        // Restore board state
        board.sideToMove = prevState.sideToMove;
        board.enPassantSquare = prevState.enPassantSquare;
        board.zobristKey = prevState.zobristKey;
        
        // If null-move search indicates position is too good, prune this branch
        if (nullMoveScore >= beta)
            return beta;
    }

    if (depth <= 0) {
        return quiescenceSearch(board, alpha, beta);
    }
    // In your alphaBeta function
    if (board.repetitionMap[board.zobristKey] >= 3) {
        return 0; // Draw score
    }

    std::vector<Move> moves = board.generateMoves();
    if (moves.empty()) {
        // No pseudo‐legal moves at all → either checkmate or stalemate
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        if (board.isSquareAttacked(kingSq, opp)) {
            return -INF + (MAX_DEPTH - depth); // checkmate
        }
        return 0; // stalemate
    }

    // Move Ordering
    for(Move &move : moves) {
        int score = 0;
        
        // if(move == tempMove) {
        //     score += 0;
        // }

        if(move.capture) {
            score += 1000 + (Evaluation::pieceValue[move.capture] * 10 - Evaluation::pieceValue[move.piece]);
        }

        // Color c = getPieceColor(move.piece);
        // int targetKingSquare = c == BLACK? board.whiteKingSquare : board.blackKingSquare;
        // if(board.tryMove(move) && board.isSquareAttacked(targetKingSquare, c)) {
        //     score += 500;
        // }

        if(move == killerMoves[depth][0]) score += 800;
        if(move == killerMoves[depth][1]) score += 700;

        score += historyHeuristics[move.from][move.to];
        // so much dynamic programming here oof

        move.heuristicScore = score;
    }

    // sort it in descending
    std::sort(moves.begin(), moves.end(), [](const Move &m1, const Move &m2) {
        return m1.heuristicScore > m2.heuristicScore;
    });

    bool foundLegalMove = false;

    int originalAlpha = alpha;
    Move bestMove(NONE, -1, -1);

    // Add just before the for-loop over moves in minimaxAlphaBeta
    int moveCount = 0;

    for (const Move& move : moves) {
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey
        };

        if (!board.makeMove(move)) {
            continue;
        }
        foundLegalMove = true;
        
        // Increment move counter
        moveCount++;

        int score;
        
        // Late Move Reduction
        int reduction = 0;
        if (depth >= 3 && moveCount >= 4 && !inCheck && !move.capture && move != killerMoves[depth][0] && move != killerMoves[depth][1]) {
            reduction = 1;
            if (moveCount >= 6) reduction++;
            if (depth >= 6) reduction++;
        }
        
        if (reduction > 0) {
            // Try reduced depth search first
            score = -minimaxAlphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha);
            
            // If it looks promising, do a full search
            if (score > alpha) {
                score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha);
            }
        } else {
            // Regular search for important moves
            score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha);
        }
        
        board.unmakeMove(move, prevdata);

        // Beta cutoff
        if (score >= beta) {
        
            // record KILLER moves here (nice name)
            if(!move.capture && !(move == killerMoves[depth][0])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = move;
            }

            // record in history heuristics
            if(!move.capture) {
                historyHeuristics[move.from][move.to] += depth * depth; // i'll try favoring deeper nodes
            }

            // record a lower bound entry in TT
            TT.store(board.zobristKey,depth, beta, TT_LOWER, move);

            return beta;
        }

        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }

    //  there were pseudo‐legal moves but none was actually legal
    if (!foundLegalMove) {
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        int eval = board.isSquareAttacked(kingSq, opp)
        ? -INF + (MAX_DEPTH - depth)  // checkmate
        : 0;                           // stalemate

        TT.store(board.zobristKey, depth, eval, TT_EXACT, Move(NONE, -1, -1));

        return eval;
    }

    TT_FLAG flag;
    if(alpha <= originalAlpha) {
        flag = TT_UPPER;
    }
    else {
        flag = TT_EXACT;
    }

    TT.store(board.zobristKey, depth, alpha, flag, bestMove);

    return alpha;
}

int Search::quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth) {
    ++nodeCount;
    
    // 1. More aggressive depth limit
    if (qdepth >= 4) {  // Reduce from 6 to 4
        int eval = Evaluation::evaluate(board);
        TT.store(board.zobristKey, -qdepth, eval, TT_EXACT, Move(NONE, -1, -1));
        return eval;
    }
    
    // 2. Stand pat evaluation
    int standPat = Evaluation::evaluate(board);
    
    // Stand pat cutoff
    if (standPat >= beta) {
        // Store lower bound in TT
        TT.store(board.zobristKey, -qdepth, beta, TT_LOWER, Move(NONE, -1, -1));
        return beta;
    }
    
    // Update alpha if stand pat is better
    if (alpha < standPat)
        alpha = standPat;
    
    // Generate capture moves
    std::vector<Move> allMoves = board.generateMoves();
    std::vector<Move> captures;
    for(const Move &move : allMoves) {
        if(move.capture) {
            captures.push_back(move);
        }
    }
    
    // 4. More aggressive delta pruning
    const int FUTILITY_MARGIN = 150; // Reduce from 200 to 150
    
    // Order captures by MVV-LVA
    std::sort(captures.begin(), captures.end(), [](const Move& a, const Move& b) {
        return (Evaluation::pieceValue[a.capture] - Evaluation::pieceValue[a.piece & 7]) >
               (Evaluation::pieceValue[b.capture] - Evaluation::pieceValue[b.piece & 7]);
    });
    
    Move bestMove(NONE, -1, -1);
    
    for (const Move& move : captures) {
        // Delta pruning - skip captures that can't improve alpha
        if (standPat + Evaluation::pieceValue[move.capture] + FUTILITY_MARGIN <= alpha) {
            continue;
        }
        
        // Skip obviously bad captures (losing material)
        if (Evaluation::pieceValue[move.capture] < Evaluation::pieceValue[move.piece & 7] - 100) {
            continue;
        }
        
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey
        };
        
        if (!board.makeMove(move))
            continue;
            
        int score = -quiescenceSearch(board, -beta, -alpha, qdepth + 1);
        board.unmakeMove(move, prevdata);
        
        if (score >= beta) {
            // Store lower bound in TT
            TT.store(board.zobristKey, -qdepth, beta, TT_LOWER, move);
            return beta;
        }
            
        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }
    
    TT_FLAG finalFlag = (alpha > standPat) ? TT_EXACT : TT_UPPER;
    TT.store(board.zobristKey, -qdepth, alpha, finalFlag, bestMove);

    return alpha;
}

// Helper function to detect positions where null-move is unsafe
bool Search::hasNonPawnMaterial(const BitBoard& board, Color side) {
    if (side == WHITE) {
        return board.getWhiteKnights() || board.getWhiteBishops() || 
               board.getWhiteRooks() || board.getWhiteQueens();
    } else {
        return board.getBlackKnights() || board.getBlackBishops() || 
               board.getBlackRooks() || board.getBlackQueens();
    }
}