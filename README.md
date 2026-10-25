
# Ironfang ♖

IronFang is a UCI-compatible chess engine written from scratch in C++.

> 📖 **Comprehensive Architecture & Development Guide**: See [DEVELOPMENT.md](DEVELOPMENT.md) for full algorithmic theory (Alpha-Beta, NMP, RFP, PVS, Futility Pruning), evaluation design, version history, and empirical benchmark results.


## Project Structure
```bash
.
├── build/          # CMake build output
├── include/        # Header files (.h files)
├── src/            # Source code (.cpp files)
├── .gitignore
└── CMakeLists.txt  # CMake build configuration

```
## Build Instructions

Clone the project

```bash
  git clone https://github.com/cmd-anurag/Ironfang.git
  cd Ironfang
```

Create and enter a build directory

```bash
  mkdir build
  cd build
```

Generate build files with CMake

```bash
  cmake ..
```

Compile the engine

```bash
  make
```
## Running the Engine
IronFang implements the UCI (Universal Chess Interface) protocol and can be used in any UCI-compatible environment, such as

- cutechess-cli and GUI

- BanksiaGUI

- Arena

- Other Chess GUIs or test harnesses

For example, to test the engine via cutechess-cli (a 2 minutes + 0 increment match)
```bash
cd build
cutechess-cli \
  -engine cmd=./chess name=IronFang proto=uci \
  -each tc=120+0 \
  -games 10 \
  -repeat \
  -concurrency 1 \
  -resign score=800 movecount=3 \
  -draw movenumber=50 movecount=5 score=10
```

## Fixed-Depth Benchmark

The benchmark runner searches a fixed set of FENs at a fixed depth and writes
one CSV row per position. It starts a fresh engine process for each position
so transposition-table contents do not leak between positions.

```powershell
cmake --build build-windows --config Release
python tools/bench.py --engine build-windows/ironfang.exe --label candidate --depth 8 --output bench-results-candidate.csv
python tools/compare_bench.py bench-results-v5.csv bench-results-candidate.csv
```

Save the current version's CSV before replacing its executable. The comparison
is for regression diagnosis: fixed-depth node counts, speed, score, and move
changes are useful signals, but score changes alone do not establish a strength
gain. Use paired-color self-play for playing strength.

The runner can also collect a deeper move/PV reference from another UCI engine:

```powershell
python tools/bench.py --engine D:/path/to/stockfish.exe --label stockfish --depth 14 --output bench-results-stockfish.csv
```

Use Stockfish's move and PV as analysis references; its NPS, node counts, and
centipawn scores are not directly comparable to Ironfang's.

The engine also reports `seldepth`, elapsed `time`, and per-search counters on
an `info string stats` line. `tt_keyhits / tt_probes` is the full-key match rate;
`tt_cutoffs / tt_probes` is the rate of probes that returned a usable bound.
These are diagnostics, not universal strength targets, and should be compared
on the same position suite, hash setup, build, and hardware.


#### Current Capabilitites

- Full legal move generation, including castling, en passant, and promotion

- Magic Bitboards for sliding pieces

- Simple evaluation based on material balance and piece-square tables

- Negamax search with alpha-beta pruning

- UCI protocol support for GUI integration

- Iterative deepening and time management
 
- Quiescence search

- Move ordering heuristics (MVV-LVA, killer moves, etc)

- Improved evaluation: king safety, pawn structure and positional understanding.

- Transposition tables using Zobrist Hashing

#### Future Plans

- Syzygy/endgame tablebase integration

- Opening Books

## Authors

- [Me](https://www.github.com/cmd-anurag)

Feel free to fork, tinker, or contribute. Pull requests and issues are welcome.


## License

[MIT](https://choosealicense.com/licenses/mit/)

