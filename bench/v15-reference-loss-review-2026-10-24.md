# v15 vs reference: UCI game review (2026-10-24)

## What this file contains

The attached Cute Chess transcript contains one game, not the full ten-game match: one `ucinewgame` per engine and 87 `go`/`bestmove` pairs per engine. v15 was White and Chess-Coding-Adventure was Black. The final moves are `87.Kf1 Ra1#`; v15 already reported `mate -1` before its final move. The loss was therefore decided well before the mating move.

The match result supplied separately was reference 7 wins, v15 0 wins, and 3 draws. This is a serious signal, but this one transcript cannot explain all ten results. The move history below was reconstructed from the UCI `position` and `bestmove` commands.

## Search and TT measurements from this game

- v15 completed 87 searches, totaling 179,117,114 nodes and 139,414 ms of reported search time: about 1.285 million nodes/second across this game.
- Median search time was 1,471 ms. Median reported depth was 9; the ordinary search depths were 8–12. The final losing searches reported depths 96/99 after detecting forced mate, so those late depth values are not useful as ordinary strength comparisons.
- TT counters: 94,329,440 probes, 28,293,149 full-key matches (30.0% of probes), and 15,399,818 usable bound cutoffs (16.3% of probes; 54.4% of key matches). These describe this one engine/game only; the reference transcript does not expose comparable counters.
- Every v15 `info` line reported `hashfull 999`. This transcript cannot show how quickly the TT filled: the engine keeps its global TT across `ucinewgame`, and the captured game may have started with entries from earlier games. Record/reset starting occupancy per game before using `hashfull` as a fill-rate comparison.
- The first command gave v15 61,000 ms remaining plus a 1,000 ms increment. The current allocator divides by the default 30 moves-to-go, adds the increment, and applies a 95% search hard limit. That predicts 2,881 ms on move one, exactly matching the transcript. This is the allocator's expected budget, not evidence that the engine arbitrarily stopped early.

## Concrete move findings

Stockfish was used only as a diagnostic baseline at fixed positions (depth 20). Its score is from White's point of view. Search scores can vary slightly by build and hardware; the large move-choice gaps below are the useful signal.

| White move | v15 played / v15 score | Stockfish depth-20 choice | Restricted search of v15 move | Assessment |
|---|---|---|---|---|
| 15 | `c3` / `+0.50` at depth 8 | `Nxc3`, `+0.16` | `c3` scored `-0.74` | v15's score after its chosen move was about 1.24 pawns more optimistic than this baseline. This is the first clear divergence in the reviewed sequence. |
| 17 | `c4` / `+0.44` at depth 8 | `Ng4`, `-0.59` | `c4` scored `-2.58` | The chosen move gave Black a much stronger line; the depth-20 PV starts `...Nd4`. This is the clearest individual error in this game. |
| 18 | `Nxc8` / `-0.30` at depth 8 | `O-O`, `-2.57` | Not separately measured | The positions were already bad, and v15 chose a different continuation from Stockfish. Treat this as a candidate for follow-up, not a quantified blunder. |

The move-15 and move-17 FENs and moves are also in `reference-v15-loss-positions.csv` for fixed-position regression checks. v15 saw move 17 as favorable while Stockfish found a losing continuation for that exact choice. That points to a search/evaluation disagreement that we can reproduce; it does not, by itself, prove whether search depth, move ordering, or evaluation is the root cause.

## What this does and does not establish

- This particular game shows v15 reaching a forced loss as White. Its final mate reporting was not the cause; v15 recognized mate was unavoidable before the last move.
- The opening evaluation is optimistic compared with Stockfish in some positions, but one game is not enough to decide whether the evaluation is systematically mis-scaled.
- The TT cutoffs are being used, but there is no reference-side TT log here, so we cannot claim Ironfang's TT is better or worse from this transcript.
- Before changing evaluation weights, use the saved FENs to reproduce v15's root scores and candidate rankings, then identify which code path makes `c4` look safe. Add the remaining nine PGNs before generalizing from the match.
