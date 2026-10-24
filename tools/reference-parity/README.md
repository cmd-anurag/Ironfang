# Reference parity harness

This .NET harness runs the reference engine's own evaluation, move generator,
and fixed-depth iterative search without changing the reference checkout. It
links the source tree selected by `ReferenceRoot`.

```powershell
dotnet run --project tools/reference-parity/ReferenceParity.csproj -- eval `
  'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
```

Commands `eval`, `moves`, and `hash` take a FEN. `search <FEN> <depth>` returns
the reference's best move and score at that fixed depth. `randomfens <seed>
<count> [max-plies]` generates repeatable legal positions. `randomaudit` emits
each position with its Zobrist key, evaluation, and ordered legal-move list.
Override the reference path with
`-p:ReferenceRoot='D:\path\to\Chess-Coding-Adventure'`.

Ironfang exposes matching diagnostic-only UCI commands: after `position`, send
`eval`, `hash`, or `legalmoves`. They do not run during ordinary search.

## Current differential audit

On 2026-10-24, seed `400191` generated 1,000 legal positions up to 120 plies.
Ironfang matched the reference's Zobrist key, static evaluation, and legal
moves in the same order on all 1,000 positions. A separate 70-position
depth-4 sample had 67 searchable positions and 3 terminal positions; all
searchable positions matched best move and score exactly. A further
20-position depth-5 sample also matched best move and score on every position.

These checks establish parity for the sampled positions, not a proof for every
possible chess state. Opening-book move selection remains nondeterministic: the
book data and weighting calculation match, but the C++ and .NET random number
generators are different. Wall-clock search can also complete different
iterations when runtime or machine timing differs; fixed-depth search avoids
that source of variation.
