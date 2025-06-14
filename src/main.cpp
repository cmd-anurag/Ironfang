#include "board.h"
#include "bitboard.h"
#include "evaluate.h"
#include "search.h"
#include "mailbox.h"
#include "uci.h"
#include "zobrist.h"
#include "perft.h"
#include <iostream>
#include <vector>

int main() {
    uciLoop();
}