# Reference parity harness

This .NET harness runs the reference engine's own evaluation, move generator,
and fixed-depth iterative search without changing the reference checkout. It
links the source tree selected by `ReferenceRoot`.

```powershell
dotnet run --project tools/reference-parity/ReferenceParity.csproj -- eval `
  'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
```

Commands `eval`, `moves`, and `hash` take a FEN. `search <FEN> <depth>` returns
the reference's best move and score after fixed-depth iterative deepening;
`bench` also reports searched nodes and elapsed search time, so it can compare
work and throughput separately. `randomfens <seed> <count> [max-plies]`
generates repeatable legal positions. `randomaudit` emits each position with
its Zobrist key, evaluation, and ordered legal-move list. Override the
reference path with
`-p:ReferenceRoot='D:\path\to\Chess-Coding-Adventure'`.

Ironfang exposes matching diagnostic-only UCI commands: after `position`, send
`eval`, `hash`, or `legalmoves`. They do not run during ordinary search.

## Differential parity audit (2026-10-24)

The earlier state matched hash, static evaluation, and ordered legal moves on
1,000 random legal positions. The search port then had several concrete
differences which are now aligned:

- Root ordering uses the previous completed iteration's best move; the first
  iteration starts without a root hint. Internal nodes continue to use the
  indexed TT move, including on a key collision, as the reference does.
- Killer and hash move matching now use the reference's packed move identity,
  including the pawn-double-push flag. The TT decoder preserves that flag.
- The reference's `-1` TT lookup-failure sentinel also masks a legitimate
  score of `-1`; Ironfang now reproduces that lookup behavior for parity.
- Root search uses the same alpha-beta routine as interior nodes. The hard
  search deadline is signaled asynchronously, as in the reference.

After those changes, fixed-depth searches matched the reference on repeatable
random positions: 80/80 best moves at depth 5 (79/79 raw scores compared; one
result had no root-score line to compare),
50/50 best moves at depth 6, and 20/20 at depth 7. The test resets the TT
between positions and compares the same FEN and iterative depth sequence. A
separate 8-position depth-8 sample also matched all 8 best moves.

This is strong evidence that the sampled board, evaluation, move-ordering, TT,
and fixed-depth search behavior now agree. It does not prove parity for every
position or guarantee equal strength under a clock. Opening-book random-number
streams differ, and the C++ and .NET builds can complete different work before
the same deadline. The small exploratory depth-7 timing sample varied by
position and is not a reliable throughput estimate; the reference project
targets .NET 6 while this harness targets .NET 10.

For controlled UCI comparisons, disable Ironfang's opening book before sending
the position and go commands:

```text
setoption name UseOpeningBook value false
```

The reference UCI build also reports a comparable diagnostic line after each
non-book search:

```text
info string stats total_nodes=... elapsed_ms=... nps=... depth=... partial=0
```

This can be compared with Ironfang's `info string stats` output at the same
FEN and time budget. The reference's `total_nodes` includes main-search and
quiescence calls.

The option defaults to `true` for normal play. Disabling it prevents random
opening choices from contaminating fixed-depth, timed-throughput, or
color-balanced search comparisons.

## v21 hot-path audit (2026-10-25)

`bench` reports the reference's node count as well as elapsed time. On five
fixed depth-8 positions, the v21 Release build and its v20 parent returned the
same best move, score, and node count after the packed-move fixes. A separate
10-position random depth-6 comparison matched the reference's best move and
centipawn score on all 10 positions.

The packed-move conversion initially had two parity errors: it gave the quiet
promotion ordering bonus to a knight promotion instead of a queen promotion,
and treated en passant as a capture for late-move reduction, killer, and
history handling. Both are corrected, and the packed flags now have shared
named constants. These checks cover samples only; they do not guarantee match
results.

For the expensive black position
`r3r1k1/1ppbqppp/2n1pn2/2Pp4/p2P4/P1BBPN2/1PQ2PPP/R4RK1 b - - 0 15`, the
current v21 Release executable completed depth 9 within an 800 ms move budget
in repeated runs. In three runs, v20 averaged about 4.24M NPS and v21 about
4.85M NPS on this FEN. The reference source compiled by this harness on .NET 6
took about 0.8 s to complete depth 8. This did not reproduce the earlier claim
that the optimized v21 binary was slower than the reference on this position.
Check the executable path and Release build configuration when comparing UCI
logs; the engine's displayed `id name` is still a manually maintained version
string. Timing outside this repeated position was noisy, so these measurements
do not establish a universal speedup.

### Recheck against the actual reference UCI build (2026-10-25)

The earlier depth-7/1.12M-NPS result was from before these v21 optimizations,
so it is not evidence about the current build. I reran the current Release
executable (`build-windows/ironfang.exe`) and the downloaded reference Release
UCI executable on the same machine with `go movetime 800`. Both runs used the
same five non-book benchmark FENs and returned the same best move in every
position. NPS below is the UCI-reported value; the reference executable did not
expose a total-node counter in a form the benchmark parser could read, so node
counts are not compared here.

| Position | Ironfang depth | Reference depth | Ironfang NPS | Reference NPS | Best move |
| --- | ---: | ---: | ---: | ---: | --- |
| Kiwipete | 8 | 8 | 5.06M | 3.11M | d5d6 |
| Rook-pawn endgame | 11 | 11 | 4.81M | 3.41M | b4f4 |
| Promotion/castling stress | 9 | 8 | 4.36M | 2.35M | c4c5 |
| Promotion tactics | 9 | 9 | 4.04M | 2.60M | d7c8q |
| Middlegame tactics | 9 | 8 | 4.75M | 2.33M | c3d5 |

Ironfang's reported NPS was higher on all five positions (median ratio 1.63x),
with equal depth on three and one extra completed ply on two. On the previously
highlighted expensive black position, three repeated 800 ms runs also completed
depth 9 with `...Rb8`; the reference completed depth 8 with the same move. These
results contradict the claim that this current build is slower on the tested
positions. They are throughput checks, not proof of overall playing-strength
parity: NPS definitions, search trees, and results can vary by position, and a
five-position timing sample is not a match-strength test.

The raw per-run observations are saved locally in the ignored file
`../../bench/results/v21-reference-throughput-2026-10-25.csv`; the summary table
above is the versioned record.
