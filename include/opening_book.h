#pragma once

#include "bitboard.h"
#include <string>
#include <unordered_map>
#include <vector>

class OpeningBook {
public:
    static void initialize();
    static std::string chooseMove(const BitBoard& board, int gamePly);

private:
    struct BookMove {
        std::string uci;
        int playCount;
    };

    OpeningBook();
    std::string positionKey(const BitBoard& board) const;
    static const OpeningBook& instance();

    std::unordered_map<std::string, std::vector<BookMove>> movesByPosition;
};
