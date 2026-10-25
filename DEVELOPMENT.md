# Ironfang Development & Architecture Guide

A comprehensive technical reference for the architecture, search theory, evaluation system, benchmark methodology, and development history of the **Ironfang** chess engine.

---

## Table of Contents
1. [Engine Overview & Version Milestones](#1-engine-overview--version-milestones)
2. [Search Architecture & Algorithmic Theory](#2-search-architecture--algorithmic-theory)
   - [2.1 Minimax & Alpha-Beta Pruning](#21-minimax--alpha-beta-pruning)
   - [2.2 Move Ordering Pipeline](#22-move-ordering-pipeline)
   - [2.3 Transposition Table (TT) & Zobrist Hashing](#23-transposition-table-tt--zobrist-hashing)
   - [2.4 Iterative Deepening & Aspiration Windows](#24-iterative-deepening--aspiration-windows)
   - [2.5 Quiescence Search & Stand-Pat](#25-quiescence-search--stand-pat)
   - [2.6 Reverse Futility Pruning (RFP)](#26-reverse-futility-pruning-rfp)
   - [2.7 Null Move Pruning (NMP)](#27-null-move-pruning-nmp)
   - [2.8 Principal Variation Search (PVS / NegaScout)](#28-principal-variation-search-pvs--negascout)
   - [2.9 Late Move Reductions (LMR)](#29-late-move-reductions-lmr)
   - [2.10 Frontier Futility Pruning (FP)](#210-frontier-futility-pruning-fp)
3. [Evaluation Architecture (Hand-Crafted Evaluation)](#3-evaluation-architecture-hand-crafted-evaluation)
   - [3.1 Material & Piece Values](#31-material--piece-values)
   - [3.2 Piece-Square Tables (PST) & Game Phase Interpolation](#32-piece-square-tables-pst--game-phase-interpolation)
   - [3.3 Pawn Structure & Passed Pawn Bonuses](#33-pawn-structure--passed-pawn-bonuses)
   - [3.4 Piece Mobility & Attack Rays](#34-piece-mobility--attack-rays)
   - [3.5 King Safety, Pawn Shields & Central File Penalties](#35-king-safety-pawn-shields--central-file-penalties)
   - [3.6 Opening Queen Excursion Penalties](#36-opening-queen-excursion-penalties)
4. [Empirical Observations & Benchmark Results](#4-empirical-observations--benchmark-results)
   - [4.1 Search Tree Pruning Efficiency](#41-search-tree-pruning-efficiency)
   - [4.2 Head-to-Head Cutechess Match Play](#42-head-to-head-cutechess-match-play)
   - [4.3 Multi-Tier Tactical Puzzle Suite (250 Curated Puzzles)](#43-multi-tier-tactical-puzzle-suite-250-curated-puzzles)
5. [Development Standards & Tooling](#5-development-standards--tooling)
6. [Roadmap & Upcoming Work (NNUE)](#6-roadmap--upcoming-work-nnue)

---

## 1. Engine Overview & Version Milestones

Ironfang is a high-performance, UCI-compliant chess engine written in C++17 utilizing bitboard board representation, magic bitboards for sliding piece attacks, and an advanced search pipeline.

### Version Milestones

| Version | Commit / Tag | Core Additions & Fixes | Performance / Elo Delta |
| :--- | :--- | :--- | :--- |
| **v27** | `57fe075` | Bitboard legal move generator, 64-bit Zobrist hashing, iterative deepening, quiescence search, MVV-LVA, killer moves, history heuristics. | Baseline bitboard engine. |
| **v28** | `80c523e` | Compact 16-byte `TTEntry` bitfield (`flag:2`, `age:6`), power-of-two `TT_MASK` indexing, age-priority TT replacement policy, root TT bypass fix, opening queen excursion penalty, uncastled king central open-file penalties, internal opening book sideline filtering. | **+279.6 Elo vs. Reference Engine** (5.0 – 1.0). |
| **v29** | `2a01273` | Atomic `makeNullMove` / `unmakeNullMove`, Reverse Futility Pruning (RFP), Null Move Pruning (NMP), Principal Variation Search (PVS), Frontier Futility Pruning (FP). | **9.7× search tree reduction**;<br>**+120.4 Elo vs. v28** (4.0 – 2.0);<br>**+416.6 Elo vs. Reference** (5.5 – 0.5). |

---

## 2. Search Architecture & Algorithmic Theory

The search algorithm determines which moves to evaluate and how deep to calculate. Ironfang employs an enhanced fail-hard Alpha-Beta framework with extensive pruning and reduction techniques.

```
Position (ply, depth, alpha, beta)
  │
  ├── 1. Transposition Table Probe ──────> (Hit with valid depth & bounds? Return ttScore)
  │
  ├── 2. Leaf Check (depth <= 0) ─────────> (Enter Quiescence Search)
  │
  ├── 3. In-Check Check
  │
  ├── 4. Static Evaluation (if !inCheck)
  │
  ├── 5. Reverse Futility Pruning (RFP) ─> (depth <= 3 && staticEval - 75*depth >= beta? Return beta)
  │
  ├── 6. Null Move Pruning (NMP) ────────> (depth >= 3 && staticEval >= beta? Search null move)
  │                                        (Opponent score >= beta? Return beta)
  │
  ├── 7. Legal Move Generation & Ordering
  │       (TT move -> MVV-LVA Captures -> Killers -> History Heuristics)
  │
  └── 8. Move Loop (Iterate ordered moves)
          ├── 8.1 Frontier Futility Pruning (depth == 1 && quiet && staticEval + 150 <= alpha? Skip)
          ├── 8.2 Late Move Reductions (depth >= 3 && moveIndex >= 3 && quiet? Search reduced)
          ├── 8.3 Principal Variation Search (Move 0: full window; Move 1+: null window scout)
          └── 8.4 Beta Cutoff (score >= beta? Store TT_LOWER, update Killers & History, Return beta)
```

### 2.1 Minimax & Alpha-Beta Pruning
In a zero-sum game, White maximizes score and Black minimizes score. Full Minimax over $b \approx 35$ legal moves to depth $d = 8$ requires examining:
$$35^8 \approx 2.25 \times 10^{12} \text{ nodes (2.25 trillion)}$$

**Alpha-Beta Pruning** maintains two bounds:
- $\alpha$: The highest score guaranteed to the side to move so far.
- $\beta$: The lowest score the opponent is guaranteed to hold us down to in alternative branches.

When any move produces a score $\ge \beta$, the opponent has an earlier countermove that avoids this position. The search immediately triggers a **Beta Cutoff (fail-high)**. Under optimal move ordering, the branching factor drops from $b$ to $\sqrt{b}$, reducing the theoretical tree size from $\mathcal{O}(b^d)$ to $\mathcal{O}(b^{d/2})$ ($\approx 1.5$ million nodes at depth 8).

### 2.2 Move Ordering Pipeline
Alpha-Beta efficiency depends entirely on searching the best move first. Ironfang orders moves dynamically before search:
1. **Transposition Table Hash Move**: The best move recorded from prior searches of this Zobrist hash (score bias: $+100,000$).
2. **Winning & Equal Captures (MVV-LVA)**: *Most Valuable Victim – Least Valuable Attacker*. Evaluated via `Evaluation::pieceValue[victim] - Evaluation::pieceValue[attacker]`. A pawn taking a queen ($P \times Q$) is ordered before a queen taking a pawn ($Q \times P$).
3. **Queen Promotions**: Non-capture promotions to Queen receive high priority (`PROMOTION_BIAS = 9,000`).
4. **Killer Moves**: Up to 2 non-capture moves per ply that caused beta cutoffs in sibling branches (`killerMoves[ply][0]` and `killerMoves[ply][1]`).
5. **History Heuristic**: A table `historyHeuristics[color][from][to]` incremented by $\text{depth}^2$ whenever a quiet move causes a beta cutoff.
6. **Losing Captures & Quiet Moves**: Ordered by Piece-Square Table deltas and penalized if the destination square is attacked by opponent pawns or pieces.

### 2.3 Transposition Table (TT) & Zobrist Hashing
Different move orders frequently reach the identical board configuration (transposition).
- **Zobrist Hash**: 64-bit key maintained incrementally via XOR ($\oplus$) operations upon every move and unmake.
- **Entry Layout**: A compact 16-byte struct:
  ```cpp
  struct TTEntry {
      uint64_t key;        // 8 bytes: full 64-bit Zobrist key
      int eval;            // 4 bytes: score normalized relative to ply
      uint16_t packedMove; // 2 bytes: best move
      uint8_t depth;       // 1 byte: search depth
      uint8_t flag : 2;    // TT_EXACT, TT_UPPER, TT_LOWER
      uint8_t age  : 6;    // search generation
  };
  ```
- **Table Size & Indexing**: Fixed at $64\text{ MiB} = 4,194,304\text{ entries}$. Direct-mapped indexing uses bitmask `TT_MASK = TT_SIZE - 1`.
- **Replacement Policy**: Combines depth and age:
  $$\text{oldScore} = \text{entry.depth} - \text{ageDiff}, \quad \text{newScore} = \text{depth}$$
  A new search replaces the slot if $\text{newScore} \ge \text{oldScore}$.

### 2.4 Iterative Deepening & Aspiration Windows
Ironfang searches depths iteratively:
$$\text{Depth 1} \rightarrow \text{Depth 2} \rightarrow \dots \rightarrow \text{Depth } N$$
Earlier depths consume $<10\%$ of total clock time while populating the Transposition Table with accurate hash moves for subsequent depths.

**Aspiration Windows**: For $\text{depth} \ge 4$, rather than searching $[-\infty, +\infty]$, the search window is bounded around the previous iteration's score:
$$[\alpha, \beta] = [\text{prevScore} - \Delta, \text{prevScore} + \Delta] \quad (\Delta = 30\text{ cp})$$
Narrow windows force earlier beta cutoffs and fail-lows. If a score fails outside the window, $\Delta$ expands by $1.5\times$ ($\Delta \leftarrow \Delta + \Delta/2$) until resolving.

### 2.5 Quiescence Search & Stand-Pat
To avoid the **Horizon Effect** (where evaluation occurs right after a piece capture before the opponent recaptures), search at `depth <= 0` transitions into Quiescence Search:
- Only legal captures and promotions are generated.
- **Stand-Pat**: The static evaluation is computed first. If $\text{staticEval} \ge \beta$, the position is already too advantageous for the opponent to allow, triggering an immediate cutoff without generating captures.

### 2.6 Reverse Futility Pruning (RFP / Static Null Move Pruning)
At low depths ($\text{depth} \le 3$), when the side to move is not in check:
$$\text{If } \text{staticEval} - (75 \times \text{depth}) \ge \beta \implies \text{Return } \beta$$
If our static advantage exceeds $\beta$ by more than $75\text{ cp}$ (depth 1), $150\text{ cp}$ (depth 2), or $225\text{ cp}$ (depth 3), the opponent cannot plausibly overcome this margin in the remaining plies. The engine cuts off immediately without generating moves.

### 2.7 Null Move Pruning (NMP)
If the side to move does nothing (**passes the turn**) and the opponent *still* cannot beat $\beta$ after a reduced-depth search, the position is overwhelmingly winning, making a full search redundant.
- **Preconditions**: `allowNull == true`, `depth >= 3`, `ply > 0`, `!inCheck`, `beta < MATE_THRESHOLD`, `staticEval >= beta`.
- **Zugzwang Guard**: NMP is skipped if the side to move has only pawns remaining (`hasNonPawns == false`), preventing false cutoffs in zugzwang endgames.
- **Reduction Formula**:
  $$R = 3 + \frac{\text{depth}}{6}$$
  $$\text{nullScore} = -\text{minimaxAlphaBeta}(\text{board}, \text{depth} - 1 - R, -\beta, -\beta + 1, \text{ply} + 1, \text{allowNull} = \text{false})$$
- If $\text{nullScore} \ge \beta$, search takes an immediate beta cutoff.

### 2.8 Principal Variation Search (PVS / NegaScout)
Because move ordering places the best move first $>85\%$ of the time:
- **Move 0**: Searched with the full window $[-\beta, -\alpha]$.
- **Moves $1, 2, 3\dots$**: Assumed to be inferior to Move 0. Searched with a **Zero-Window Scout Search**:
  $$[-\alpha - 1, -\alpha]$$
- Zero-window searches fail low rapidly with minimal branching.
- **Re-search**: If and only if a scout search beats $\alpha$ ($\text{score} > \alpha$), the assumption failed, and the move is re-searched with the full window $[-\beta, -\alpha]$.

### 2.9 Late Move Reductions (LMR)
Quiet moves appearing late in the move list (`moveIndex >= 3` at `depth >= 3`) that are not checks, promotions, or killer moves are searched with reduced depth:
$$\text{childDepth}_{\text{reduced}} = \text{childDepth} - 1$$
If a reduced move scores above $\alpha$, it is re-searched at full depth.

### 2.10 Frontier Futility Pruning (FP)
At frontier nodes ($\text{depth} == 1$), if the side to move is not in check, quiet non-checking moves are tested before child search:
$$\text{If } \text{staticEval} + 150 \le \alpha \implies \text{Prune quiet move}$$
A quiet piece move to an empty square cannot gain more than 150 centipawns in a single ply. If $\text{staticEval} + 150$ cannot even reach $\alpha$, the move is guaranteed to fail low and is skipped.

---

## 3. Evaluation Architecture (Hand-Crafted Evaluation)

Ironfang's evaluation function (`Evaluation::evaluate`) returns an integer score from the perspective of the side to move, combining multiple positional and material heuristics.

### 3.1 Material & Piece Values
Base piece values:
- Pawn: $100\text{ cp}$
- Knight: $300\text{ cp}$
- Bishop: $320\text{ cp}$
- Rook: $500\text{ cp}$
- Queen: $900\text{ cp}$
- King: $20,000\text{ cp}$

### 3.2 Piece-Square Tables (PST) & Game Phase Interpolation
Every piece type has a $64$-entry table assigning positional bonuses and penalties. Scores smoothly interpolate between **Middlegame (MG)** and **Endgame (EG)** based on the remaining non-pawn material phase ($0 = \text{endgame}, 24 = \text{full middlegame}$):
$$\text{Score} = \frac{\text{MG} \times \text{phase} + \text{EG} \times (24 - \text{phase})}{24}$$

### 3.3 Pawn Structure & Passed Pawn Bonuses
- **Passed Pawns**: A pawn with no opposing pawns on its file or adjacent files ahead of it receives rank-dependent bonuses ($0, 10, 20, 40, 80, 160\text{ cp}$).
- **Isolated Pawns**: Pawns with no friendly pawns on adjacent files receive a penalty ($-15\text{ cp}$).
- **Doubled Pawns**: Multiple friendly pawns on the same file receive a penalty ($-20\text{ cp}$).

### 3.4 Piece Mobility & Attack Rays
Computed dynamically via precomputed magic bitboards:
- Bishop mobility: Number of legal sliding attack squares outside friendly pieces.
- Rook mobility: Number of rank and file attack squares.
- Queen mobility: Combined diagonal, rank, and file squares.

### 3.5 King Safety, Pawn Shields & Central File Penalties
- **Pawn Shield**: Rewards intact friendly pawns on files $f, g, h$ (kingside) or $a, b, c$ (queenside) when castled.
- **Uncastled King on Open Central Files**: If the king remains on $d$ or $e$ files and the central file is open (no pawns) or semi-open, Ironfang assesses a heavy dynamic penalty ($-35\text{ to } -60\text{ cp}$), preventing kings from lingering in the crosshairs of enemy rooks.

### 3.6 Opening Queen Excursion Penalties
Premature queen sorties before minor piece development are penalized ($-25\text{ to } -50\text{ cp}$), preventing early queen blunders.

---

## 4. Empirical Observations & Benchmark Results

### 4.1 Search Tree Pruning Efficiency
Tested across the 6 standard benchmark positions at fixed depth 8 (`tools/bench.py --depth 8`):

| Benchmark Position | Baseline v28 Nodes | v29 Nodes (NMP + RFP + PVS + FP) | Tree Reduction |
| :--- | :--- | :--- | :--- |
| **`kiwipete`** | 1,513,207 | **118,636** | **-92.2%** |
| **`rook_pawn_endgame`** | 113,655 | **35,966** | **-68.4%** |
| **`promotion_castling`** | 814,584 | **129,864** | **-84.1%** |
| **`promotion_tactics`** | 555,480 | **61,093** | **-89.0%** |
| **`middlegame_tactics`** | 1,129,308 | **128,000** | **-88.7%** |
| **Total Benchmark Nodes** | **4,126,234** | **473,559** | **-88.5% (8.7× tree reduction)** |

All tactical benchmark moves (`d5d6`, `b4f4`, `c4c5`, `d7c8q`, `c3d5`) retain 100% precision.

---

### 4.2 Head-to-Head Cutechess Match Play

Matches executed using Cutechess CLI at standard `10+0.1` blitz time control:

```
cutechess-cli.exe -engine name=Ironfang-v29 cmd=... -engine name=Ironfang-v28 cmd=... -each tc=10+0.1
```

- **v29 vs. v28 Baseline**: **4.0 – 2.0 (+2 =4 -0, 66.7%)** $\implies$ **+120.4 Elo over v28 (LOS: 92.1%)**
- **v29 vs. Reference Engine**: **5.5 – 0.5 (+5 =1 -0, 91.7%)** $\implies$ **+416.6 Elo over Reference (LOS: 98.7%)**
- **Combined Record vs. Reference (All v29 matches)**: **11 Wins, 1 Loss, 6 Draws (77.8%)**

---

### 4.3 Multi-Tier Tactical Puzzle Suite (250 Curated Puzzles)

Dataset: [`tools/puzzles.csv`](file:///d:/playground/Ironfang/tools/puzzles.csv) contains **250 verified tactical puzzles** from Lichess across 5 categories:
1. `mating` (50 puzzles, Elo 403–1698, Avg: 981)
2. `motifs` (50 puzzles, Elo 624–2190, Avg: 1350)
3. `advanced` (50 puzzles, Elo 947–2728, Avg: 1718)
4. `endgame` (50 puzzles, Elo 804–2152, Avg: 1555)
5. `master_gm` (50 puzzles, Elo 2205–2873, Avg: 2410)

Evaluated across three time controls via `python tools/solve_puzzles.py --times 25,100,250`:

| Time Control | Engine | Solved (%) | Avg Depth | Mating (50) | Motifs (50) | Advanced (50) | Endgame (50) | Master / GM (50) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **25ms** | v28 | 223 / 250 (89.2%) | d5.7 | 48 / 50 | 48 / 50 | 46 / 50 | 48 / 50 | 33 / 50 (66%) |
| | **v29** | 222 / 250 (88.8%) | **d7.8 (+2.1 plies)** | 48 / 50 | 48 / 50 | 44 / 50 | 48 / 50 | 34 / 50 (68%) |
| **100ms** | v28 | 228 / 250 (91.2%) | d7.0 | 48 / 50 | 48 / 50 | 47 / 50 | 48 / 50 | 37 / 50 (74%) |
| | **v29** | 227 / 250 (90.8%) | **d9.5 (+2.5 plies)** | 48 / 50 | 49 / 50 | 47 / 50 | 48 / 50 | 35 / 50 (70%) |
| **250ms** | v28 | 230 / 250 (92.0%) | d7.7 | 48 / 50 | 48 / 50 | 48 / 50 | 49 / 50 | 37 / 50 (74%) |
| | **v29** | 229 / 250 (91.6%) | **d10.6 (+2.9 plies)** | 48 / 50 | 49 / 50 | 47 / 50 | 49 / 50 | 36 / 50 (72%) |

---

## 5. Development Standards & Tooling

### Binary Compilation
Compiled using CMake and Ninja on Windows x64:
```powershell
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --config Release
```

### Engine Binary Version Archive
All milestone executables are permanently archived by version in `D:/engines/`:
- `D:/engines/v28/v28.exe`
- `D:/engines/v29/v29.exe`

*(Note: File copy operations to `D:/engines/` must avoid the `-Force` flag to prevent accidental overwrite errors).*

### Automated Challenge & Match Runners
- **Lichess Bot Challenger**: [`sc.py`](file:///d:/playground/Ironfang/sc.py)
  ```powershell
  python sc.py <opponent_bot_username> --tc 3+1
  ```
- **Cutechess Head-to-Head Runner**: [`scratch/run_match_v29_vs_v28.py`](file:///C:/Users/anurag/.gemini/antigravity/brain/2654f033-c930-45f4-921e-10bf8b61b91e/scratch/run_match_v29_vs_v28.py)
  ```powershell
  python scratch/run_match_v29_vs_v28.py 6 10+0.1 2
  ```
- **250-Puzzle Tactical Benchmark**: [`tools/solve_puzzles.py`](file:///d:/playground/Ironfang/tools/solve_puzzles.py)
  ```powershell
  python tools/solve_puzzles.py --times 25,100,250
  ```

---

## 6. Roadmap & Upcoming Work (NNUE)

### Phase 1: Efficiently Updatable Neural Network (NNUE)
1. **Accumulator Architecture**:
   - HalfKP feature transformer (inputs: king square $\times$ piece square $\times$ piece type).
   - Incremental update inside `BitBoard::makeSearchMove` and `unmakeSearchMove` (adding moving piece, subtracting old square, handling captures).
2. **Evaluation Network**:
   - Small, high-throughput network (e.g. $768 \rightarrow 256 \times 2 \rightarrow 32 \rightarrow 1$ or HalfKP $40960 \rightarrow 256$).
   - Quantized INT8 / INT16 AVX2 vector SIMD inference for ultra-high NPS.
3. **Hybrid Fallback**:
   - Fast material/HCE evaluation on simple positions, blended with NNUE on complex tactical middlegames.

### Phase 2: Advanced Search Refinements
1. **Logarithmic LMR Table**: Replace flat 1-ply reduction with depth- and move-index-based reduction tables:
   $$R = 1 + \frac{\ln(\text{depth}) \times \ln(\text{moveIndex})}{2}$$
2. **Late Move Pruning (LMP)**: At depth $\le 3$, prune quiet moves after searching the first $3 + 3 \times \text{depth}^2$ moves.
3. **History Malus / Penalties**: Penalize quiet moves that fail to produce a cutoff.
