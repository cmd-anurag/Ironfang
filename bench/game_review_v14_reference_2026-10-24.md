# Ironfang v14 vs Chess-Coding-Adventure baseline

Source: the supplied Lichess PGN and UCI log for the game with Ironfang v14 as White. Scores are centipawns from White's perspective. The UCI values are Ironfang's completed iterative-deepening result; Stockfish local values are the same-position depth-22 analysis noted during review. Lichess PGN annotations are listed separately because they can come from a different Stockfish version/configuration and depth.

| Position | FEN | Ironfang result | Ironfang total search | Local Stockfish d22 | Lichess PGN eval |
|---|---|---:|---:|---:|---:|
| After `2...b6` | `rnbqkbnr/p1pp1ppp/1p2p3/8/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 0 3` | `d2d4`, +1.02, depth 10 | 12,998,656 nodes; 11,856 ms; 7,444,444 qnodes; 5,554,218 TT probes; 604,642 key matches; 309,558 TT cutoffs | +0.75 | +0.77 |
| After `12...Re8` | `r2qr1k1/pbpp1p1p/1pn3p1/3pP3/3P4/3B3Q/PPP1NPPP/R4RK1 w - - 2 13` | `d3b5`, +0.87, depth 9 | 10,162,176 nodes; 9,419 ms; 6,009,081 qnodes; 4,153,099 TT probes; 533,645 key matches; 263,762 TT cutoffs; 251,463 LMR reductions / 138 re-searches | +3.06 | +3.87; annotation recommends `f2f4` |
| After `24...Qa2` | `3rr3/1bp2pk1/p1n3pp/1p2P3/3p1P2/P2B2N1/qPPQ2PP/4RRK1 w - - 2 25` | `c2c3`, +0.58, depth 9 | 7,299,072 nodes; 7,362 ms; 4,602,816 qnodes; 2,696,124 TT probes; 320,062 key matches; 174,336 TT cutoffs; 149,963 LMR reductions / 207 re-searches | +7.76 | +5.93; annotation recommends `d2f2` |

## What the data says

- The opening score after `2...b6` is only 0.27 pawns above the local Stockfish result and 0.25 above the supplied PGN annotation. This is a small calibration difference, not evidence of a general +1 pawn bias.
- The material failures are later. After `12...Re8`, Ironfang chose `13.Bb5` instead of the attacking `13.f4`; after `24...Qa2`, it chose `25.c3`, which the PGN annotation rates as a drop from +5.93 to +1.63. The latter move then allowed the game to return close to equality after `27.cxd4`.
- At those two critical searches, qsearch accounted for about 59% and 63% of all nodes. TT cutoffs occurred on about 6.4% of probes in each position. These are useful baselines, but a TT hit/cutoff rate alone does not establish playing strength or correctness.
- Root-diagnostic A/B after `12...Re8`: at depth 8, normal search and root-diagnostic search both selected `c2c4` at +0.89. Normal search took 1.32 s; root diagnostics (which use full windows for each root move) took 9.20 s. At depth 7, root diagnostics selected `f2f4` at +0.92. This does not isolate root LMR as the cause of the depth-9 miss; disabling it has not earned a permanent change.
- The current C++ evaluation is already a close port of the reference's hand-written evaluation terms and weights. This match therefore does not support blindly copying the reference evaluation again. A king-attack term may help, but it needs a broader position set and an A/B test before being enabled by default.

## First reference-alignment change: legal-move LMR indexing

The reference generates legal moves before applying late move reductions. Ironfang generates pseudo-legal moves and rejects illegal moves during `makeMove`, but was counting the original pseudo-legal index for LMR. The root and interior search now count successfully made moves instead, so pinned/illegal moves cannot trigger reductions on earlier legal candidates. Reduction depth and thresholds are unchanged.

Fixed-depth-8 fresh-process comparison before/after the change:

| Position | Best move / score before | Nodes / ms before | Best move / score after | Nodes / ms after |
|---|---:|---:|---:|---:|
| After `2...b6` | `d2d4`, +1.14 | 828,048 / 789 | `d2d4`, +1.10 | 686,465 / 644 |
| After `12...Re8` | `c2c4`, +0.89 | 1,339,688 / 1,327 | `c2c4`, +0.89 | 1,352,323 / 1,336 |
| After `24...Qa2` | `c2c3`, +0.63 | 1,639,348 / 1,680 | `c2c3`, +0.63 | 1,584,277 / 1,592 |

At a fixed 8-second move budget, both tactical positions completed depth 9 with the same choices as their depth-8 results (`c2c4` after `...Re8`, `c2c3` after `...Qa2`). This change fixes an implementation mismatch without claiming to solve the missed tactics; the move decisions remain a target for the next isolated experiment.

## Next regression checks

Use these three FENs as fixed positions for every search/evaluation change. Compare best move, score, completed depth, nodes, elapsed time, and PV under the same build and time limit. Preserve the baseline rows above; append new version results instead of overwriting them. A game result against the reference remains a separate strength check and should use multiple color-swapped games.

The supplied files contain the v14-White win only. The v14-Black draw PGN/UCI trace is not part of this baseline yet.

## Current search cleanup and regression positions

Repetition detection previously compared the current key against every recorded
game/search position at every main-search and quiescence node. Since pawn moves
and captures reset `halfmoveClock`, and the Zobrist key includes side to move,
the search now checks only same-side entries within that reversible window.
This preserves repetition identity while avoiding scans of positions that
cannot possibly repeat.

The current depth-8 results on the three saved game positions are:

| Position | Best move | Score | Nodes | Repetition entries scanned |
|---|---:|---:|---:|---:|
| After `2...b6` | `d2d4` | +1.10 | 686,465 | 865,584 |
| After `12...Re8` | `c2c4` | +0.89 | 1,352,323 | 1,419,161 |
| After `24...Qa2` | `c2c3` | +0.63 | 1,584,277 | 1,688,994 |

All three retain the same move, score, and node count recorded after the legal-
move LMR-index correction. The six-position generic depth-8 suite also retained
all moves, scores, and node counts; median NPS was 3.1% higher in this single
run, which is too small to treat as a confirmed speedup. The CSVs preserve both
runs for later comparison.
