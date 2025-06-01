
# Ironfang ♞

IronFang is a simple UCI-compatible chess engine written in C++. It was born out of boredom and curiosity and is a hobby-project.


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


## Status
The engine is under active developement.

#### Current Capabilitites

- Full legal move generation, including castling, en passant, and promotion

- Simple evaluation based on material balance

- Negamax search with alpha-beta pruning

- UCI protocol support for GUI integration

#### Future Plans

- Quiescence search

- Move ordering heuristics (MVV-LVA, killer moves, history heuristic)

- Transposition tables using Zobrist Hashing

- Improved evaluation: piece-square tables, king safety, pawn structure

- Iterative deepening and time management

- Syzygy/endgame tablebase integration
## Authors

- [Me](https://www.github.com/cmd-anurag)

Feel free to fork, tinker, or contribute. Pull requests and issues are welcome.


## License

[MIT](https://choosealicense.com/licenses/mit/)

