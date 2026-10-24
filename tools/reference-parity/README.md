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
`bench` adds elapsed search time. `randomfens <seed> <count> [max-plies]`
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
