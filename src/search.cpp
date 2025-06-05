// search.cpp
#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>
#include <assert.h>


constexpr int INF = 1000000;

std::vector<std::vector<Move>> killerMoves(MAX_DEPTH, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<int, 64>, 64> historyHeuristics{};
TranspositionTable TT;

uint64_t nodeCount = 0;

Move Search::findBestMove(BitBoard& board, int maxDepth, int timeLimit) {
    // Reset counters
    nodeCount = 0;
    TT.hitCount = 0;
    TT.lookupCount = 0;
    
    // Start timing
    auto startTime = std::chrono::steady_clock::now();
    int timeForThinking = timeLimit;
    
    // Set a hard cutoff time slightly below the time limit (95%)
    int hardTimeLimit = timeLimit > 0 ? static_cast<int>(timeLimit * 0.95) : -1;
    
    // Default move (will be overwritten soon)
    Move bestMove{NONE, -1, -1};
    int bestScore = -INF;
    
    // Generate all legal moves
    std::vector<Move> moves = board.generateMoves();
    
    if (moves.empty()) {
        return Move{NONE, -1, -1}; // No moves available
    }
    
    // Set a default move immediately for safety
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
        
        if (board.makeMove(move)) {
            board.unmakeMove(move, prevdata);
            bestMove = move; // Set default move in case we run out of time
            break;
        }
    }
    
    // Iterative Deepening Search
    for (int depth = 1; depth <= maxDepth; depth++) {
        // Skip time check on depth 1, always do at least one ply
        if (depth > 1 && timeForThinking > 0) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count();
                
            // Check if we have enough time for another iteration
            // Use progressively more conservative estimates as we go deeper
            int timeNeededFactor = (depth >= 6) ? 4 : (depth >= 4) ? 3 : 2;
            
            // If the elapsed time * factor > time limit, stop searching
            if (elapsedMs * timeNeededFactor > timeForThinking) {
                break;
            }
        }
        
        // Start a new iteration
        int alpha = -INF;
        int beta = INF;
        int iterationBestScore = -INF;
        Move iterationBestMove = bestMove; // Start with previous best
        
        // Move ordering: start with previous best move
        std::vector<Move> orderedMoves = moves;
        auto it = std::find(orderedMoves.begin(), orderedMoves.end(), bestMove);
        if (it != orderedMoves.end()) {
            std::iter_swap(orderedMoves.begin(), it);
        }
        
        // Search all moves at this depth
        for (const Move& move : orderedMoves) {
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
            
            // Normal search with full alpha-beta window
            int score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha);
            
            board.unmakeMove(move, prevdata);
            
            // Check if this is a better move
            if (score > iterationBestScore) {
                iterationBestScore = score;
                iterationBestMove = move;
                
                if (score > alpha) {
                    alpha = score;
                }
            }
            
            // Time check after each move - use hard time limit
            if (hardTimeLimit > 0) {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - startTime).count();
                    
                if (elapsedMs >= hardTimeLimit) {
                    // If we're out of time, exit without updating bestMove
                    // to use results from the previous completed depth
                    return bestMove;
                }
            }
        }
        
        // Only update bestMove if we completed the iteration
        bestMove = iterationBestMove;
        bestScore = iterationBestScore;

        int hashFull = TT.hashfull();
        // Output information
        std::cout << "info depth " << depth 
                  << " score cp " << bestScore
                  << " nodes " << nodeCount << " hashfull " << hashFull
                  << " pv " << moveToUCI(bestMove) << "\n" << std::flush;
    }
    
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

    if (depth >= 3 && !inCheck && hasNonPawnMaterial(board, board.sideToMove) && 
    beta < MATE_THRESHOLD && alpha > -MATE_THRESHOLD) {
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
        // uint64_t nullKey = board.zobristKey;
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
        // assert(board.zobristKey == nullKey);
        // If null-move search indicates position is too good, prune this branch
        if (nullMoveScore >= beta)
            return beta;
    }

    if (depth <= 0) {
        return quiescenceSearch(board, alpha, beta);
    }

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
            // Checkmate: encode ply distance so mate in 1 is better than mate in 2
            return -INF + (MAX_DEPTH - depth);
        }
        return 0; // stalemate
    }

    // Move Ordering
    for(Move &move : moves) {
        int score = 0;

        if(move.capture) {
            score += 1000 + (Evaluation::pieceValue[move.capture] * 10 - Evaluation::pieceValue[move.piece]);
        }
        if(move == tempMove) {
            score += 1500;
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

    int moveCount = 0;
    int moveIndex = 0;
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

        // uint64_t originalKey = board.zobristKey;
        if (!board.makeMove(move)) {
            continue;
        }
        foundLegalMove = true;
        
        // Increment move counter
        moveCount++;

        int score;
        
        // Late Move Reduction
        bool isReducible = !inCheck && !move.capture &&
                    move != killerMoves[depth][0] &&
                    move != killerMoves[depth][1];

        int reduction = 0;
        if (depth >= 3 && moveIndex > 0 && isReducible) {
            reduction = 1;
            if (depth >= 6 && moveIndex >= 4) reduction++;
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
        // assert(board.zobristKey == originalKey);
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
        : 0;             // stalemate

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

    Move probeMove;
    int probeEval;
    if (TT.probe(board.zobristKey, -qdepth, alpha, beta, probeEval, probeMove)) {
        return probeEval;
    }
    
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
    captures.reserve(36); // there can be a maximuum of 35 capture options 

    for(Move &move : allMoves) {
        if(move == probeMove) {
            move.heuristicScore += 10000;   
        }


        if(move.capture) {
            move.heuristicScore += 1000 + (Evaluation::pieceValue[move.capture] * 10 - Evaluation::pieceValue[move.piece]);
            captures.push_back(move);
        }
    }
    
    // 4. More aggressive delta pruning
    const int FUTILITY_MARGIN = 150; // Reduce from 200 to 150
    
    // Order captures by MVV-LVA
    std::sort(captures.begin(), captures.end(), [](const Move& a, const Move& b) {
        return a.heuristicScore > b.heuristicScore;
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