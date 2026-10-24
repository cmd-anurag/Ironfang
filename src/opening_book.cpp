#include "opening_book.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace {
std::mt19937_64& randomGenerator() {
    static std::mt19937_64 generator(std::random_device{}());
    return generator;
}

std::string enPassantName(int square) {
    const char file = static_cast<char>('a' + (square & 7));
    const char rank = static_cast<char>('8' - (square >> 3));
    return {file, rank};
}
}

OpeningBook::OpeningBook() {
    std::vector<std::filesystem::path> paths;
#ifdef IRONFANG_BOOK_PATH
    paths.emplace_back(IRONFANG_BOOK_PATH);
#endif
    paths.emplace_back("resources/Book.txt");
    paths.emplace_back("Book.txt");
    paths.emplace_back("../resources/Book.txt");

    std::ifstream input;
    for (const auto& path : paths) {
        input.open(path);
        if (input) break;
        input.clear();
    }
    if (!input) return;

    std::string position;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("pos ", 0) == 0) {
            position = line.substr(4);
            continue;
        }
        if (position.empty() || line.empty()) continue;

        std::istringstream entry(line);
        BookMove move;
        if (entry >> move.uci >> move.playCount && move.playCount > 0)
            movesByPosition[position].push_back(std::move(move));
    }
}

const OpeningBook& OpeningBook::instance() {
    static const OpeningBook book;
    return book;
}

void OpeningBook::initialize() {
    (void)instance();
}

std::string OpeningBook::positionKey(const BitBoard& board) const {
    std::string fen;
    fen.reserve(80);
    for (int rank = 0; rank < 8; ++rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = board.getPiece(rank * 8 + file);
            if (piece == NONE) {
                ++emptyCount;
                continue;
            }
            if (emptyCount) {
                fen += static_cast<char>('0' + emptyCount);
                emptyCount = 0;
            }
            fen += pieceToChar(piece);
        }
        if (emptyCount) fen += static_cast<char>('0' + emptyCount);
        if (rank != 7) fen += '/';
    }

    fen += board.sideToMove == WHITE ? " w " : " b ";
    std::string castling;
    if (board.whiteKingsideCastle) castling += 'K';
    if (board.whiteQueensideCastle) castling += 'Q';
    if (board.blackKingsideCastle) castling += 'k';
    if (board.blackQueensideCastle) castling += 'q';
    fen += castling.empty() ? "- " : castling + " ";

    int legalEnPassantSquare = -1;
    if (board.enPassantSquare >= 0) {
        BitBoard trial = board;
        const std::vector<Move> captures = board.generateCaptures();
        for (const Move& move : captures) {
            if (!move.isEnPassant) continue;
            const Gamestate previousState = {
                trial.sideToMove,
                trial.enPassantSquare,
                trial.whiteKingsideCastle,
                trial.whiteQueensideCastle,
                trial.blackKingsideCastle,
                trial.blackQueensideCastle,
                trial.whiteKingSquare,
                trial.blackKingSquare,
                trial.zobristKey,
                trial.halfmoveClock
            };
            if (trial.makeMove(move)) {
                trial.unmakeMove(move, previousState);
                legalEnPassantSquare = move.to;
                break;
            }
        }
    }
    fen += legalEnPassantSquare < 0 ? "-" : enPassantName(legalEnPassantSquare);
    return fen;
}

std::string OpeningBook::chooseMove(const BitBoard& board, int gamePly) {
    if (gamePly > 16) return {};

    const OpeningBook& book = instance();
    const auto found = book.movesByPosition.find(book.positionKey(board));
    if (found == book.movesByPosition.end() || found->second.empty()) return {};

    std::vector<int> weights;
    weights.reserve(found->second.size());
    int totalWeight = 0;
    for (const BookMove& move : found->second) {
        const int weight = static_cast<int>(std::ceil(std::sqrt(move.playCount)));
        weights.push_back(weight);
        totalWeight += weight;
    }

    std::uniform_real_distribution<double> distribution(0.0, static_cast<double>(totalWeight));
    const double selection = distribution(randomGenerator());
    double cumulativeWeight = 0.0;
    for (size_t i = 0; i < found->second.size(); ++i) {
        cumulativeWeight += weights[i];
        if (selection <= cumulativeWeight) return found->second[i].uci;
    }
    return found->second.back().uci;
}
