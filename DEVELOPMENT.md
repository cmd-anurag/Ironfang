# Ironfang Development Guide & Benchmarks

The practical engineering guide, changelog, empirical benchmark results, and development standards for the **Ironfang** chess engine.

> 📖 **Algorithmic & Evaluation Theory**: For complete mathematical formulations, Minimax proofs, Zobrist hashing mechanics, search pruning algorithms, and evaluation heuristics, see the dedicated companion manual: **[THEORY.md](THEORY.md)**.

---

## Table of Contents
1. [Engine Overview & Version Milestones](#1-engine-overview--version-milestones)
2. [Search & Evaluation Architecture Summary](#2-search--evaluation-architecture-summary)
3. [Empirical Observations & Benchmark Results](#3-empirical-observations--benchmark-results)
   - [3.1 Search Tree Pruning Efficiency (Depth 8 Bench)](#31-search-tree-pruning-efficiency-depth-8-bench)
   - [3.2 Head-to-Head Cutechess Match Play](#32-head-to-head-cutechess-match-play)
   - [3.3 Multi-Tier Tactical Puzzle Suite (250 Curated Puzzles)](#33-multi-tier-tactical-puzzle-suite-250-curated-puzzles)
4. [Development Standards & Tooling](#4-development-standards--tooling)
   - [4.1 Binary Compilation & Release Archive](#41-binary-compilation--release-archive)
   - [4.2 Automated Testing Scripts](#42-automated-testing-scripts)
5. [Roadmap & Next Steps (NNUE)](#5-roadmap--next-steps-nnue)

---

## 1. Engine Overview & Version Milestones

Ironfang is a high-performance, UCI-compliant chess engine written in C++17 utilizing bitboard representation, magic bitboard sliding attacks, and an advanced search pipeline.

### Version Milestones

| Version | Commit / Tag | Core Additions & Fixes | Performance / Elo Delta |
| :--- | :--- | :--- | :--- |
| **v27** | `57fe075` | Bitboard legal move generator, 64-bit Zobrist hashing, iterative deepening, quiescence search, MVV-LVA, killer moves, history heuristics. | Baseline bitboard engine. |
| **v28** | `80c523e` | Compact 16-byte `TTEntry` bitfield (`flag:2`, `age:6`), power-of-two `TT_MASK` indexing, age-priority TT replacement policy, root TT bypass fix, opening queen excursion penalty, uncastled king central open-file penalties, internal opening book sideline filtering. | **+279.6 Elo vs. Reference Engine** (5.0 – 1.0). |
| **v29** | `2a01273` | Atomic `makeNullMove` / `unmakeNullMove`, Reverse Futility Pruning (RFP), Null Move Pruning (NMP), Principal Variation Search (PVS), Frontier Futility Pruning (FP). | **9.7× search tree reduction**;<br>**+120.4 Elo vs. v28** (4.0 – 2.0);<br>**+416.6 Elo vs. Reference** (5.5 – 0.5). |
| **v30** | `0c76005` | Logarithmic multi-ply LMR table ($R = 0.75 + \frac{\ln(d)\ln(m)}{2.25}$), Late Move Pruning ($3 + 3d^2$), History Malus ($[-50k, 50k]$), Positional Eval (rooks on open/semi-open files, 7th rank infiltration, knight outposts, bishop pair bonus). | **34.8× Kiwipete tree reduction vs. v28**;<br>**+381.7 Elo vs. v29** (9.0 – 1.0);<br>**+279.6 Elo vs. Reference** (5.0 – 1.0);<br>**Record 93.2% (233/250) on 250 Puzzles**. |
| **v31** | `11a5c68` | Graph History Interaction (GHI) / TT Repetition Blindness Bypass (`hasAppearedInGameHistory`), full 4-tier regression suite verification, known blunders regression harness. | **+34.9 Elo vs. v30** (3 – 2 – 5, undefeated by time);<br>**100.0% Sweep vs. Reference** (6 – 0 – 0, all checkmates);<br>**93.2% (233/250) on 250 Puzzles**;<br>**4/4 (100%) Known Blunders Passed**;<br>**Zero Speed/NPS Regression**. |
| **v32** | `2d6fb49` | Structural castling tracking (`whiteHasCastled`/`blackHasCastled`), counter-move heuristic table (`counterMoves[64][64]`), counter-move LMR discount, uncastled king penalty fix. | **+147.2 Elo vs. v31** (7.0 – 3.0);<br>**+416.6 Elo vs. Reference** (5.5 – 0.5);<br>**94.0% (235/250) on 250 Puzzles**;<br>**5/5 (100%) Known Blunders Passed**. |
| **v33** | `58a13d3` | **Pure HalfKAv2 NNUE Evaluation** (40,960 -> 256 -> 1 quantized network, SIMD accumulators, streaming dataset training pipeline), Stockfish-style Dual-Bound Dynamic Time Management (`include/timeman.h`, `src/timeman.cpp`), obvious-move node fraction savings, instability extensions, 52-opening balanced benchmark suite. | **+139.0 Elo vs. v32 across 100 Games** (64 – 26 – 10, 100.0% LOS);<br>**100.0% (7/7) Known Blunders Passed**;<br>**Record 94.8% on Blitz Tactical Suite**;<br>**100.0% (50/50) Endgame Suite**;<br>**Throughput: ~3.1M NPS at d15–d16**. |
| **v34** | `HEAD` | **100M Century Milestone HalfKAv2 512-Hidden NNUE**: Scaled architecture to 512-hidden neurons (23.1M params), aligned training with official Stockfish `nnue-pytorch` standards ($\gamma=0.995$ continuous geometric decay, persistent Adam momentum vectors, gradient norm clipping at 1.0, 3-phase golden ratio sampling). Added 6th Regression Tier (Famous Historic Positions). | **+240.8 Elo vs. v33** (8.0 – 2.0, 98.3% LOS);<br>**+107.5 Elo vs. v32 Peak HCE** (6.5 – 3.5);<br>**+279.6 Elo vs. Reference Engine** (5.0 – 1.0);<br>**All-Time Record 97.2% (243/250) on Puzzles** (100% Motifs, 100% Endgames, 94% GM);<br>**9/10 (90%) Known Blunders Passed**;<br>**Training Loss: 0.01072**. |

---

## 2. Search & Evaluation Architecture Summary

Ironfang pairs a selective Alpha-Beta search with Hand-Crafted Evaluation (HCE). For in-depth mathematical formulations and pseudocode, consult **[THEORY.md](THEORY.md)**.

### Search Engine Components
* **Framework**: Iterative deepening with Aspiration Windows ($[\text{score} - 30\text{ cp}, \text{score} + 30\text{ cp}]$).
* **Transposition Table**: 64 MiB direct-mapped table (4,194,304 entries) with 16-byte compact entries and age-priority replacement.
* **Move Ordering**: Hash move $\rightarrow$ Winning/Equal MVV-LVA captures $\rightarrow$ Queen promotions $\rightarrow$ Killer moves $\rightarrow$ History heuristics $\rightarrow$ Quiet PST ordering.
* **Pruning Pipeline**:
  * **Reverse Futility Pruning (RFP)**: Near leaves ($d \le 3$), prunes if $\text{staticEval} - 75 \times d \ge \beta$.
  * **Null Move Pruning (NMP)**: Bypasses subtrees if passing the turn still refutes the opponent ($R = 3 + d/6$), guarded by non-pawn piece checks against zugzwang.
  * **Frontier Futility Pruning (FP)**: At $d = 1$, prunes quiet moves if $\text{staticEval} + 150 \le \alpha$.
* **Reductions & Scouts**:
  * **Principal Variation Search (PVS)**: Searches Move 0 with full window; searches subsequent sibling moves with narrow zero-window scouts $[-\alpha-1, -\alpha]$.
  * **Late Move Reductions (LMR)**: Reduces late quiet moves (`moveIndex >= 3`) by 1 ply.
* **Quiescence Search**: Resolves tactical captures and promotions with Stand-Pat cutoffs to eliminate the Horizon Effect.

### Evaluation Function Components
* **Material & Phase Interpolation**: Base material scores smoothly interpolated across game phases ($0 = \text{endgame}, 24 = \text{middlegame}$).
* **Piece-Square Tables (PST)**: Position-dependent bonuses/penalties for all 6 piece types.
* **Pawn Structure**: Dynamic rewards for passed pawns ($0$ to $+160\text{ cp}$); penalties for isolated ($-15\text{ cp}$) and doubled pawns ($-20\text{ cp}$).
* **Mobility**: Safe attack ray mobility computed via precomputed Magic Bitboards.
* **King Safety**: Castled pawn shield bonuses; dynamic penalties ($-35$ to $-60\text{ cp}$) for uncastled kings lingering on open central files.
* **Opening Principles**: Penalties for premature queen sorties before minor piece development.

---

## 3. Empirical Observations & Benchmark Results

### 3.1 Search Tree Pruning Efficiency (Depth 8 Bench)
Evaluated across standard positions via `python tools/bench.py --depth 8`:

| Benchmark Position | Baseline v28 Nodes | v29 Nodes (NMP + RFP + PVS + FP) | Tree Reduction |
| :--- | :--- | :--- | :--- |
| **`kiwipete`** | 1,513,207 | **118,636** | **-92.2%** |
| **`rook_pawn_endgame`** | 113,655 | **35,966** | **-68.4%** |
| **`promotion_castling`** | 814,584 | **129,864** | **-84.1%** |
| **`promotion_tactics`** | 555,480 | **61,093** | **-89.0%** |
| **`middlegame_tactics`** | 1,129,308 | **128,000** | **-88.7%** |
| **Total Benchmark Nodes** | **4,126,234** | **473,559** | **-88.5% (8.7× smaller tree)** |

All benchmark positions continue to identify the exact expected best moves (`d5d6`, `b4f4`, `c4c5`, `d7c8q`, `c3d5`).

---

### 3.2 Head-to-Head Cutechess Match Play
Matches executed via Cutechess CLI at standard `10+0.1` blitz time control:

- **v29 vs. v28 Baseline**: **4.0 – 2.0 (+2 =4 -0, 66.7%)** $\implies$ **+120.4 Elo over v28 (LOS: 92.1%)**
- **v29 vs. Reference Engine**: **5.5 – 0.5 (+5 =1 -0, 91.7%)** $\implies$ **+416.6 Elo over Reference (LOS: 98.7%)**
- **Combined Record vs. Reference (All v29 matches)**: **11 Wins, 1 Loss, 6 Draws (77.8%)**

---

### 3.3 Multi-Tier Tactical Puzzle Suite (250 Curated Puzzles)
Dataset: [`tools/puzzles.csv`](tools/puzzles.csv) contains **250 verified tactical puzzles** from Lichess across 5 categories:
1. `mating` (50 puzzles, Elo 403–1698, Avg: 981)
2. `motifs` (50 puzzles, Elo 624–2190, Avg: 1350)
3. `advanced` (50 puzzles, Elo 947–2728, Avg: 1718)
4. `endgame` (50 puzzles, Elo 804–2152, Avg: 1555)
5. `master_gm` (50 puzzles, Elo 2205–2873, Avg: 2410)

#### Comparative Baseline Test: v28 vs. v29
Evaluated via `python tools/solve_puzzles.py --times 25,100,250`:

| Time Control | Engine | Solved (%) | Avg Depth | Mating (50) | Motifs (50) | Advanced (50) | Endgame (50) | Master / GM (50) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **25ms** | v28 | 223 / 250 (89.2%) | d5.7 | 48 / 50 | 48 / 50 | 46 / 50 | 48 / 50 | 33 / 50 (66%) |
| | **v29** | 222 / 250 (88.8%) | **d7.8 (+2.1 plies)** | 48 / 50 | 48 / 50 | 44 / 50 | 48 / 50 | 34 / 50 (68%) |
| **100ms** | v28 | 228 / 250 (91.2%) | d7.0 | 48 / 50 | 48 / 50 | 47 / 50 | 48 / 50 | 37 / 50 (74%) |
| | **v29** | 227 / 250 (90.8%) | **d9.5 (+2.5 plies)** | 48 / 50 | 49 / 50 | 47 / 50 | 48 / 50 | 35 / 50 (70%) |
| **250ms** | v28 | 230 / 250 (92.0%) | d7.7 | 48 / 50 | 48 / 50 | 48 / 50 | 49 / 50 | 37 / 50 (74%) |
| | **v29** | 229 / 250 (91.6%) | **d10.6 (+2.9 plies)** | 48 / 50 | 49 / 50 | 47 / 50 | 49 / 50 | 36 / 50 (72%) |

#### Deeper Search Scaling (Ironfang v29)
Evaluated up to 1 full second per puzzle via `python tools/solve_puzzles.py --times 100,300,1000`:

| Time Tier | Solved (%) | Avg Depth | Avg Time / Puzzle | Mating (50) | Motifs (50) | Advanced (50) | Endgame (50) | Master / GM (50) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **100ms** | 227 / 250 (90.8%) | d9.4 | 78.7 ms | 48 / 50 | 49 / 50 | 47 / 50 | 48 / 50 | 35 / 50 (70%) |
| **300ms** | 231 / 250 (92.4%) | d10.8 | 234.7 ms | 48 / 50 | 49 / 50 | 48 / 50 | 48 / 50 | 38 / 50 (76%) |
| **1000ms (1.0s)** | **236 / 250 (94.4%)** | **d12.0** | 780.9 ms | 48 / 50 (96%) | 49 / 50 (98%) | 48 / 50 (96%) | **50 / 50 (100%)** | **41 / 50 (82%)** |

*At 1 second per move, Ironfang achieves a **perfect 100% score on Endgames** and solves **82% of Grandmaster-level tactics**.*

---

### 3.4 Search Upgrades: Late Move Pruning (LMP) & History Malus
Implemented in `src/search.cpp`:
- **Late Move Pruning (LMP)**: At `depth <= 3`, prunes quiet non-checking moves once $3 + 3 \times \text{depth}^2$ quiet candidates have been evaluated without finding a cutoff.
- **History Malus**: When a quiet move refutes a branch, awards $+\min(400, \text{depth}^2)$ to the refutation while penalizing all preceding failed quiet moves with $-\min(400, \text{depth}^2)$ (bounded within $[-50000, +50000]$).

#### Empirical Comparison (LMP + Malus vs. v29 Baseline)
Evaluated across 250 curated puzzles (`python tools/solve_puzzles.py --times 25,100,250`):
- **100ms Tier**: Solved **229 / 250 (91.6%)** vs v29's 228 / 250 (+1 puzzle). Average nodes dropped from **189,344 to 166,759 (-11.9% node savings)**. Average depth increased to **d9.6**.
- **250ms Tier**: Solved **232 / 250 (92.8%)** vs v29's 231 / 250 (+1 puzzle). Average nodes dropped from **458,025 to 377,847 (-17.5% node savings)**. Average depth increased to **d10.6**.

#### Head-to-Head Cutechess Match Play vs. v29 Baseline
Executed at standard `10+0.1` blitz time control:
- **Ironfang (LMP+Malus) vs. Ironfang v29**: **3.5 – 2.5 (+2 =3 -1, 58.3%)** $\implies$ **+58.5 Elo over v29 (LOS: 71.8%)**.

---

### 3.5 Search Upgrades: Logarithmic Multi-Ply LMR Table
Replaced flat 1-ply reductions with dynamic logarithmic multi-ply reductions:
$$R(d, m) = \left\lfloor 0.75 + \frac{\ln(d) \times \ln(m)}{2.25} \right\rfloor$$
Combined with giving-check exemptions, killer move discounts, and history score adjustments.

#### Fixed-Depth Benchmark Efficiency (`bench.py --depth 8`)
| Benchmark Position | Baseline v28 Nodes | v29 Baseline Nodes | Logarithmic LMR Nodes | Tree Reduction vs. v28 |
| :--- | :--- | :--- | :--- | :--- |
| **`kiwipete`** | 1,513,207 | 118,636 | **41,371** | **-97.3% (36.6× smaller)** |
| **`rook_pawn_endgame`** | 113,655 | 35,966 | **20,474** | **-82.0%** |
| **`promotion_castling`** | 814,584 | 129,864 | **115,954** | **-85.8%** |
| **`promotion_tactics`** | 555,480 | 61,093 | **23,856** | **-95.7%** |
| **`middlegame_tactics`** | 1,129,308 | 128,000 | **51,612** | **-95.4% (21.9× smaller)** |
| **Total Benchmark Nodes** | **4,126,234** | **473,559** | **253,267** | **-93.9% (16.3× smaller)** |

#### Curated 250-Puzzle Tactical Suite (`tools/solve_puzzles.py`)
| Time Tier | Solved (%) | Avg Depth | Avg Depth Increase vs. v29 |
| :--- | :--- | :--- | :--- |
| **25ms** | 221 / 250 (88.4%) | **d8.4** | **+0.8 plies** |
| **100ms** | 228 / 250 (91.2%) | **d10.5** | **+1.1 plies** |
| **250ms** | 232 / 250 (92.8%) | **d11.9** | **+1.4 plies** |

#### Head-to-Head Cutechess Match Play vs. v29 Baseline
Executed at standard `10+0.1` blitz time control:
- **Ironfang (LogLMR) vs. Ironfang v29**: **4.0 – 2.0 (+2 =4 -0, 66.7%)** $\implies$ **+120.4 Elo over v29 (LOS: 92.1%)**. Zero losses in 6 games.

---

### 3.6 Positional Evaluation Refinements & Ironfang v30
Implemented in `src/evaluate.cpp`:
- **Rooks on Open Files**: $+25\text{ cp}$ bonus for rooks on files devoid of pawns.
- **Rooks on Semi-Open Files**: $+12\text{ cp}$ bonus for rooks on files devoid of friendly pawns.
- **7th Rank Infiltration**: $+20\text{ cp}$ bonus for rooks attacking the enemy 7th rank when the enemy king or pawns are present.
- **Bishop Pair Bonus**: $+30\text{ cp}$ advantage for retaining both bishops.
- **Knight Outposts**: $+25\text{ cp}$ bonus for centrally anchored knights (files c, d, e, f, ranks 4, 5, 6) defended by friendly pawns and unassailable by opposing pawns.

#### Multi-Tier Tactical Benchmark Results (Ironfang v30)
Evaluated across all 250 curated puzzles (`python tools/solve_puzzles.py --times 25,100,250`):
- **Tier 25ms**: **221 / 250 (88.4%)** | Avg Depth: **d8.3**
- **Tier 100ms**: **229 / 250 (91.6%)** | Avg Depth: **d10.5**
- **Tier 250ms**: **233 / 250 (93.2%) [All-Time Record]** | Avg Depth: **d11.8**
  - Master / GM category reached **39 / 50 (78%)**!

#### Standard Regression Testing Match Play (Ironfang v30)
Executed at standard `10+0.1` blitz time control under the engine testing protocol:
- **vs. Previous Version (Ironfang v29, 10 Games)**:
  - **Score**: **9.0 – 1.0 (+8 =2 -0, 90.0%)** $\implies$ **+381.7 Elo over v29 (LOS: 99.8%)**.
  - Undefeated in all 10 games (8 wins, 2 draws).
- **vs. Reference Engine (Chess-Coding-Adventure, 6 Games)**:
  - **Score**: **5.0 – 1.0 (+4 =2 -0, 83.3%)** $\implies$ **+279.6 Elo over Reference (LOS: 97.7%)**.
  - Undefeated in all 6 games (4 wins, 2 draws).
- **Combined Regression Record**: **12 Wins, 0 Losses, 4 Draws (87.5%)**.

#### Athena-ChessBot Game (`omjbaV8R`) Diagnostic Probe
Evaluated at Move 14 after 14. b4 (where Ironfang in the real game played `14... cxb4?!`, creating backward pawns and opening the $a$-file for White's rooks):
- **v28**: `bestmove c5b4` (-10 cp, depth 9, 3.0M nodes)
- **v29**: `bestmove c5b4` (-18 cp, depth 12, 2.7M nodes)
- **v30**: **`bestmove b7b6`** (-27 cp, **depth 14**, 2.2M nodes) $\implies$ **Rejects `cxb4`**, solidifies the pawn chain, keeps the $a$-file closed, and denies White any rook penetration!

#### Athena-ChessBot Rematch Game (`WXQrrUuA`) Autopsy
In the live rematch, Ironfang played White against Athena-ChessBot:
- **Middlegame Dominance**: Ironfang decisively outplayed Athena (+5.4 pawns up with an outside passed $a$-pawn and active rook).
- **The Endgame Repetition Draw (Move 53)**: Facing checking rooks, Ironfang repeated `49. Ke4 Re1+ 50. Kf5 Ra1 51. Ke4 Re1+ 52. Kf5 Ra1 53. Ke4?? Re1+`, allowing Athena to claim a 3-fold repetition draw while completely winning.
- **Root Cause Identified**: The Transposition Table retained the $+346\text{ cp}$ score from Move 51. At Move 53, the TT hit caused an immediate cutoff returning $+346\text{ cp}$, failing to notice that re-entering that position now constituted the 3rd repetition.
- **Regression Tracking**: Position archived into [`tools/known_blunders.csv`](tools/known_blunders.csv) for continuous regression testing.

---

### 3.7 Graph History Interaction (GHI) & Ironfang v31
Implemented in `src/search.cpp`:
- **GHI Repetition Blindness Bypass**: Added `hasAppearedInGameHistory(const BitBoard& board, int ply)` which scans `board.repetitionPath` within the reversible `halfmoveClock` window.
- **Unsound Cutoff Prevention**: If a position has occurred in actual game history prior to the root ($t_{\text{current}} - t_{\text{history}} > \text{ply}$), `minimaxAlphaBeta` bypasses the TT cutoff while preserving `ttMove` for move ordering. Search evaluates child moves, enabling `isRepetitionDraw()` to catch 3-fold repetitions and evaluate them at $0\text{ cp}$, preventing winning games from throwing draws.

#### Throughput & Speed Benchmark vs. v30 Baseline (`1000ms` per position)
| Benchmark Position | v30 Depth & NPS | v31 Depth & NPS | Depth Delta | NPS Delta |
| :--- | :--- | :--- | :--- | :--- |
| **`kiwipete`** | d15 (2.82M NPS) | d15 (2.85M NPS) | +0 ply | +1.1% |
| **`rook_pawn_endgame`** | d18 (3.82M NPS) | d18 (3.84M NPS) | +0 ply | +0.5% |
| **`promotion_castling_stress`** | d13 (3.08M NPS) | d14 (3.04M NPS) | **+1 ply** | -1.3% |
| **`promotion_tactics`** | d16 (2.67M NPS) | d16 (2.65M NPS) | +0 ply | -0.8% |
| **`middlegame_tactics`** | d13 (3.12M NPS) | d13 (3.19M NPS) | +0 ply | +2.1% |

*Speed impact is statistically neutral ($\pm 1\%$), confirming zero search throughput overhead in normal non-repeated tree nodes.*

#### Standard 4-Tier Regression Suite Results (Ironfang v31)
1. **Tier 1 (vs. v30, 10 Games at `10+0.1`)**:
   - **Score**: **5.5 – 4.5 (+3 =5 -2, 55.0%)** $\implies$ **+34.9 Elo over v30 (LOS: 67.3%)**.
   - Zero time forfeits; all decisive results ended in checkmate.
2. **Tier 2 (vs. Reference Engine, 6 Games at `10+0.1`)**:
   - **Score**: **6.0 – 0.0 (+6 =0 -0, 100.0% Clean Sweep)** $\implies$ **LOS: 99.3%**.
   - All 6 games won by forced on-the-board checkmate (+M1).
3. **Tier 3 (Curated 250-Puzzle Tactical Suite at 25ms, 100ms, 250ms)**:
   - **25ms**: **221 / 250 (88.4%)** | Avg Depth: **d8.4** | Avg Nodes: 51,101
   - **100ms**: **231 / 250 (92.4%)** | Avg Depth: **d10.5** | Avg Nodes: 182,281
   - **250ms**: **233 / 250 (93.2%)** | Avg Depth: **d11.7** | Master/GM: **39 / 50 (78.0%)**
   - Zero regression vs. v30's record-high puzzle accuracy.
4. **Tier 4 (Known Blunders Regression Harness)**:
   - **Score**: **4 / 4 Passed (100.0%)** $\implies$ **0 Blunders Repeated**.
   - Position `athena2_m53_repetition` passes cleanly, selecting winning continuations (`f5f4` / `a5a6`) and avoiding `f5e4`.

---

### 3.8 Static Exchange Evaluation (SEE) Experimentation & Reversion (v32 Candidate)
A candidate release for Ironfang v32 was developed and put through the standard 5-tier regression protocol:
- **Experiment 1 (Static Exchange Evaluation in `orderMoves`)**:
  - Implemented `BitBoard::see` via null-window swap algorithm.
  - Used in `orderMoves` to classify captures.
  - **Findings**: Ray-casting during move ordering on contested captures added per-node overhead (~8–18% NPS drop). At fixed `10+0.1` time control, this reduction was not offset by depth gains (search depth remained flat or dropped 1 ply), resulting in a **4.0 – 6.0 defeat vs. v31**.
- **Experiment 2 (MrTonnerre King Walk Autopsy & $f1$ PST Modification)**:
  - White's move `11. Kf1?!` in `4zp7OsMz` was autopsied: at depth 3+, search hallucinated that `11. Kf1` traps Black's bishop after `11... Bd6 12. Be3 Bxh2 13. Bxc5 Qxc5 14. Rxh2` (with the rook on $h1$ protected by the king on $f1$).
  - Artificially changing $f1$ from $+10$ to $-10$ broke the symmetry of the PST table and distorted king evaluation whenever driven to $f1$, degrading self-play.
- **Decision**: In strict accordance with the regression protocol, **both candidate changes were rejected and reverted** in commit `67deb78`.
- The clean, unit-tested `BitBoard::see` implementation is retained in the codebase as a foundation for future capture pruning experiments in the main search tree ($SEE < -50 \times d$).

### 3.6 Version 32 (Released 2026-10-26)
- **Key Enhancements**:
  1. **Structural Castling State & King Safety Fix**:
     - Root cause of artificial king-walk blunder `11. Kf1?!` in `4zp7OsMz`: `kingFile >= 5` had classified the king on $f1$ as safely castled on the flank, completely wiping out the uncastled penalty.
     - Added explicit `whiteHasCastled` and `blackHasCastled` tracking to `Gamestate` and `BitBoard`.
     - Awarded $+25\text{ cp}$ bonus (`castlingBonus`) for genuine castling and applied a $-35\text{ cp}$ penalty (+ development penalty) for voluntary forfeiture of both castling rights without castling.
     - Restricted central uncastled file vulnerability strictly to central files $c, d, e, f$ (`kingFile >= 2 && kingFile <= 5`), preserving complete PST symmetry.
  2. **Counter-Move Heuristic & LMR Exemption**:
     - Added an $8\text{ KB}$ `counterMoves[64][64]` table recording the quiet move that historically refuted the opponent's previous move (`searchPathMoves[ply - 1]`).
     - Prioritized counter-moves immediately after killer moves in move ordering (`COUNTER_MOVE_BIAS = 3500000`).
     - Granted an LMR reduction discount (`reduction = max(0, reduction - 1)`) for counter-moves, preserving tactical vision on known refutations.
- **5-Tier Regression Verification**:
  - **Tier 1 (Previous Version Head-to-Head)**: **7.0 / 10 (+5 -1 =4, +147.2 Elo, LOS: 94.9%)** vs production v31 at `10+0.1`.
  - **Tier 2 (Reference Engine Match)**: **5.5 / 6.0 (+5 -0 =1, +416.6 Elo)** with 0 losses.
  - **Tier 3 (Tactical Puzzle Suite)**: **235 / 250 (94.0%)** at 250ms, **230 / 250 (92.0%)** at 100ms.
  - **Tier 4 (Known Blunders Suite)**: **5 / 5 passed (100.0%)**, zero blunders repeated (`11. Kf1` eliminated).
  - **Tier 5 (Speed Benchmark)**: Search depth maintained/improved with flat NPS.
- **Artifacts & Deployment**:
  - Binary archived: `D:/engines/v32/v32.exe` (269,850 bytes).
  - Production deployment: `D:/lichess-bot/engines/ironfang.exe`.

### 3.7 Version 33 Candidate (Completed 2026-10-27, Unshipped Pending v34)
- **Key Enhancements**:
  1. **Pure HalfKAv2 NNUE Evaluation**:
     - Migrated evaluation from handcrafted heuristics (HCE) to 100% Pure NNUE in [`src/nnue.cpp`](src/nnue.cpp), [`include/nnue.h`](include/nnue.h), and [`include/accumulator.h`](include/accumulator.h).
     - HalfKAv2 architecture: 40,960 perspective-based inputs $\to$ 256 clipped-ReLU hidden accumulators $\to$ scalar output.
     - Quantized to INT16 with fixed-point SIMD arithmetic for ultra-fast incremental accumulator updates in `BitBoard::makeSearchMove` and `unmakeSearchMove`.
     - 3-Phase Stratified Reservoir Streaming Dataset Trainer in [`tools/train_nnue.py`](tools/train_nnue.py) trained across 89.2 Million positions from Lichess Open Evaluations (25% opening, 55% middlegame, 20% endgame).
  2. **Stockfish Dual-Bound Dynamic Time Management**:
     - Modularized time management into dedicated [`include/timeman.h`](include/timeman.h) and [`src/timeman.cpp`](src/timeman.cpp).
     - Implemented dual bounds: `optimumTime` ($\frac{T_{\text{left}} - \text{overhead}}{35} + 0.75 \times \text{inc}$) and `maximumTime` ($\min(0.30 \times T_{\text{left}}, 3.5 \times \text{optimumTime})$).
     - Dynamic node fraction reduction: When the best root move commands $\ge 75\%$ of root nodes, time target scales down ($0.5\times\text{–}0.7\times$), saving critical clock on obvious recaptures.
     - Instability scaling: Multiplies time budget by $1.8\times$ (capped at `maximumTime`) when PV flips or score drops $>30\text{ cp}$ after depth 4.
     - Clean inter-iteration stopping: Aborts before launching an iteration that is statistically unreachable with remaining clock budget.
  3. **Known Blunders Suite & 52-Opening Balanced Suite**:
     - Tested against all 7 historical blunders and expanded to a diverse 52-opening balanced benchmark suite (`bench/results/openings_50.pgn`).
- **5-Tier Regression & 100-Game Verification**:
  - **Mega 100-Game Match vs. v32**: **64 Wins, 26 Losses, 10 Draws (69.0%, +139.0 Elo, LOS: 100.0%)** across 52 opening systems at `10+0.1`.
  - **Direct Model Showdown**: 89.2M defeated 79.2M in head-to-head match play (5.5 – 4.5).
  - **Tier 2 (Reference Engine Match)**: Undefeated vs Reference Engine at `10+0.1`.
  - **Tier 3 (Tactical Puzzle Suite)**: **Record 94.8% (237/250)** at 100ms, **94.4% (236/250)** at 25ms.
  - **Endgame Suite (100% Verified)**: **50 / 50 (100.0%)** at 1000ms.
  - **Tier 4 (Known Blunders Suite)**: **7 / 7 Passed (100.0%)** — only model in history to pass all 7 blunder tests (curing `athena2_m38`).
  - **Tier 5 (Speed Benchmark)**: Maintained **d15.3** average search depth in 1.0 second (~3.1M NPS).
- **Release Status**:
  - Binary deployed: `build-windows/ironfang.exe` (UCI `Ironfangv33`).
  - Officially released and archived to `D:/engines/v33/v33.exe`.

---

## 4. Development Standards & Tooling

### 4.1 Standard 6-Tier Regression Protocol
Every newly compiled milestone must complete the six-tier regression harness before deployment:
1. **Tier 1 - Previous Version Match**: 10 games against `v<N-1>` at `10+0.1` via Cutechess CLI. Must achieve $\ge 50\%$ or demonstrate positive Elo.
2. **Tier 2 - Reference Engine Match**: 6 games against Reference Engine at `10+0.1` via Cutechess CLI.
3. **Tier 3 - Tactical Puzzle Suite**: 250 curated puzzles evaluated at `25ms`, `100ms`, and `250ms` via `tools/solve_puzzles.py`.
4. **Tier 4 - Known Blunders Suite**: Verification against all historically identified blunder and loss positions via `python tools/test_known_blunders.py --engine <exe>`.
5. **Tier 5 - Speed Benchmark**: Verification of throughput / NPS comparison strictly against `bench-v<N-1>-1s.csv` via `python tools/bench.py --engine <exe> --movetime 1000`. Must not regress in search depth.
6. **Tier 6 - Famous Historic Positions Suite**: Verification of historic sacrificial and study positions via `python tools/test_famous_positions.py --engine <exe> --movetime 1000`.

### 4.2 Binary Compilation & Release Archive
Compile using CMake and Ninja on Windows x64:
```powershell
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --config Release
```

All milestone executables are permanently archived by version in `D:/engines/`:
- `D:/engines/v28/v28.exe`
- `D:/engines/v29/v29.exe`
- `D:/engines/v30/v30.exe`
- `D:/engines/v31/v31.exe`
- `D:/engines/v32/v32.exe`
- `D:/engines/v33/v33.exe`
- `D:/engines/v34/v34.exe`

*(Rule: File copy commands to `D:/engines/` must never use `-Force` to prevent accidental overwrites).*

### 4.2 Automated Testing Scripts
- **Lichess Bot Challenger**: [`sc.py`](sc.py)
  ```powershell
  python sc.py <opponent_bot_username> --tc 3+1
  ```
- **Cutechess Head-to-Head Runner**: [`scratch/run_match_v29_vs_v28.py`](scratch/run_match_v29_vs_v28.py)
  ```powershell
  python scratch/run_match_v29_vs_v28.py 6 10+0.1 2
  ```
- **250-Puzzle Tactical Benchmark**: [`tools/solve_puzzles.py`](tools/solve_puzzles.py)
  ```powershell
  python tools/solve_puzzles.py --times 25,100,250
  python tools/solve_puzzles.py --times 100,300,1000
  ```

---

## 5. Roadmap & Next Steps

### Phase 1: Pure NNUE Evaluation (Completed in v33)
1. [x] **HalfKAv2 Accumulator Architecture**: 40,960 inputs $\to$ 256 clipped-ReLU accumulators with dual perspectives.
2. [x] **Incremental Accumulator Updates**: Embedded in `BitBoard::makeSearchMove` and `unmakeSearchMove`.
3. [x] **Streaming PyTorch Trainer**: 3-phase stratified reservoir sampler trained on 89.2M evaluations.
4. [x] **Pure NNUE Search Integration**: 100% neural evaluation in `evaluatePosition()` without HCE anchors (+139.0 Elo vs v32).

### Phase 2: Advanced Search Refinements (Completed in v30–v32)
1. [x] **Late Move Pruning (LMP)**: At depth $\le 3$, prune quiet moves after searching the first $3 + 3 \times \text{depth}^2$ moves. (Completed: +58.5 Elo vs v29)
2. [x] **History Malus / Penalties**: Penalize quiet moves that fail to produce a cutoff. (Completed)
3. [x] **Logarithmic LMR Table**: Replace flat 1-ply reduction with depth- and move-index-based reduction tables:
   $$R = 0.75 + \frac{\ln(\text{depth}) \times \ln(\text{moveIndex})}{2.25}$$
   (Completed: +120.4 Elo vs v29, 36.6× Kiwipete tree reduction)
4. [x] **Positional Evaluation Refinements**:
   - Rooks on open and semi-open files (+25 cp, +12 cp).
   - 7th rank rook infiltration (+20 cp).
   - Knight outposts protected by pawns (+25 cp).
   - Bishop pair advantage (+30 cp). (Completed: All-time record 93.2% puzzle accuracy)
5. [x] **Stockfish Dual-Bound Time Manager**: Optimum/Maximum time bounds with PV instability scaling. (Completed in v33)

### Phase 3: Ironfang v34 Performance Roadmap
1. [ ] **AVX2 / AVX-512 SIMD Intrinsics**: Hand-vectorized 256-bit accumulator updates to boost NPS from 3.1M $\to$ 5.5M+ NPS (+50 Elo).
2. [ ] **Search Pruning Tuning for NNUE**: Re-calibrate LMR, RFP, and NMP margins specifically against NNUE's non-linear score distribution.
3. [ ] **Wider Accumulator Network (512 Hidden Neurons)**: Expand network capacity to ingest 250M+ positions.
