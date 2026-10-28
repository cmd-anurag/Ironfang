#!/usr/bin/env python3
"""Run a comprehensive tactical puzzle benchmark across multiple time controls and categories."""

import argparse
import csv
import pathlib
import subprocess
import sys
import time


def parse_args():
    parser = argparse.ArgumentParser(description="Evaluate engine tactical solving across time controls and categories.")
    parser.add_argument("--engine", type=str, default="build-windows/Ironfang.exe", help="Path to UCI engine executable")
    parser.add_argument("--baseline", type=str, default=None, help="Optional baseline engine executable for side-by-side comparison")
    parser.add_argument("--puzzles", type=str, default="tools/puzzles.csv", help="Path to puzzle CSV file")
    parser.add_argument("--count", type=int, default=250, help="Number of puzzles to evaluate (default: 250)")
    parser.add_argument("--depth", type=int, default=None, help="Fixed search depth per puzzle")
    parser.add_argument("--movetime", type=int, default=None, help="Single fixed movetime in ms")
    parser.add_argument("--times", type=str, default=None, help="Comma-separated movetimes in ms (e.g. '20,100,500')")
    parser.add_argument("--verbose", action="store_true", help="Print details for every puzzle")
    return parser.parse_args()


class EngineProcess:
    def __init__(self, engine_path):
        self.proc = subprocess.Popen(
            [str(engine_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        self._send("uci")
        self._wait_for("uciok")
        self._send("setoption name UseOpeningBook value false")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token):
        for line in self.proc.stdout:
            if token in line:
                return line.strip()
        return ""

    def solve(self, fen, setup_move, depth=None, movetime=None):
        self._send("isready")
        self._wait_for("readyok")

        if setup_move:
            self._send(f"position fen {fen} moves {setup_move}")
        else:
            self._send(f"position fen {fen}")

        if movetime is not None:
            go_cmd = f"go movetime {movetime}"
        elif depth is not None:
            go_cmd = f"go depth {depth}"
        else:
            go_cmd = "go depth 8"

        t0 = time.perf_counter()
        self._send(go_cmd)

        best_move = None
        nodes = 0
        depth_reached = 0

        for line in self.proc.stdout:
            line = line.strip()
            if line.startswith("info"):
                tokens = line.split()
                if "nodes" in tokens:
                    try:
                        nodes = int(tokens[tokens.index("nodes") + 1])
                    except (ValueError, IndexError):
                        pass
                if "depth" in tokens:
                    try:
                        depth_reached = int(tokens[tokens.index("depth") + 1])
                    except (ValueError, IndexError):
                        pass
            elif line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    best_move = parts[1]
                break

        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        return best_move, elapsed_ms, nodes, depth_reached

    def close(self):
        try:
            self._send("quit")
            self.proc.wait(timeout=2)
        except Exception:
            self.proc.kill()


def evaluate_tier(engine, puzzles, depth=None, movetime=None, verbose=False):
    passed = 0
    total_time_ms = 0
    total_nodes = 0
    depths = []

    categories = {
        "mating": {"passed": 0, "total": 0},
        "motifs": {"passed": 0, "total": 0},
        "advanced": {"passed": 0, "total": 0},
        "endgame": {"passed": 0, "total": 0},
        "master_gm": {"passed": 0, "total": 0},
    }

    brackets = {
        "<1500": {"passed": 0, "total": 0},
        "1500-1799": {"passed": 0, "total": 0},
        "1800-2099": {"passed": 0, "total": 0},
        "2100+": {"passed": 0, "total": 0},
    }

    failed_puzzles = []

    for idx, p in enumerate(puzzles, 1):
        pid = p.get("id", str(idx))
        fen = p["fen"]
        setup = p.get("setup_move", "")
        expected = p["expected_move"]
        rating = int(p.get("rating", 0))
        cat = p.get("category", "motifs")

        if cat in categories:
            categories[cat]["total"] += 1

        if rating < 1500:
            bracket_key = "<1500"
        elif rating < 1800:
            bracket_key = "1500-1799"
        elif rating < 2100:
            bracket_key = "1800-2099"
        else:
            bracket_key = "2100+"

        brackets[bracket_key]["total"] += 1

        best_move, elapsed_ms, nodes, d = engine.solve(
            fen, setup, depth=depth, movetime=movetime
        )

        is_correct = (best_move == expected)
        total_time_ms += elapsed_ms
        total_nodes += nodes
        depths.append(d)

        if is_correct:
            passed += 1
            if cat in categories:
                categories[cat]["passed"] += 1
            brackets[bracket_key]["passed"] += 1
            status = "PASS"
        else:
            status = "FAIL"
            failed_puzzles.append({
                "id": pid,
                "rating": rating,
                "category": cat,
                "expected": expected,
                "actual": best_move,
                "themes": p.get("themes", "")
            })

        if verbose or (not is_correct and idx % 20 == 0) or idx == len(puzzles):
            print(f"[{idx:3d}/{len(puzzles)}] {pid:6s} ({cat:8s} {rating:4d}): {status:4s} "
                  f"[got: {best_move:5s} exp: {expected:5s}] (d{d:2d}, {elapsed_ms:5.1f}ms, {nodes:6d} nodes)")

    pct = 100.0 * passed / len(puzzles) if puzzles else 0.0
    avg_d = sum(depths) / len(depths) if depths else 0.0
    avg_ms = total_time_ms / len(puzzles) if puzzles else 0.0
    avg_nodes = total_nodes / len(puzzles) if puzzles else 0.0

    return {
        "passed": passed,
        "total": len(puzzles),
        "pct": pct,
        "avg_depth": avg_d,
        "avg_ms": avg_ms,
        "avg_nodes": avg_nodes,
        "total_nodes": total_nodes,
        "total_time_ms": total_time_ms,
        "categories": categories,
        "brackets": brackets,
        "failed": failed_puzzles
    }


def main():
    args = parse_args()
    engine_path = pathlib.Path(args.engine).resolve()
    puzzles_path = pathlib.Path(args.puzzles).resolve()

    if not engine_path.is_file():
        print(f"Error: Engine executable not found at {engine_path}", file=sys.stderr)
        sys.exit(1)
    if not puzzles_path.is_file():
        print(f"Error: Puzzles CSV not found at {puzzles_path}", file=sys.stderr)
        sys.exit(1)

    puzzles = []
    with open(puzzles_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            puzzles.append(row)
            if len(puzzles) >= args.count:
                break

    print(f"Loaded {len(puzzles)} curated puzzles from {puzzles_path.name}")

    if args.times:
        time_tiers = [int(t.strip()) for t in args.times.split(",")]
    elif args.movetime is not None:
        time_tiers = [args.movetime]
    elif args.depth is not None:
        time_tiers = None
    else:
        # Default 3-tier benchmark: Fast (25ms), Blitz (100ms), Rapid (300ms)
        time_tiers = [25, 100, 300]

    engine = EngineProcess(engine_path)
    tier_results = {}

    try:
        if time_tiers:
            for ms in time_tiers:
                print(f"\n" + "=" * 75)
                print(f"Running Time Tier: {ms}ms per puzzle on {engine_path.name}")
                print("=" * 75)
                res = evaluate_tier(engine, puzzles, movetime=ms, verbose=args.verbose)
                tier_results[f"{ms}ms"] = res
                print(f"--> Tier {ms}ms: Solved {res['passed']}/{res['total']} ({res['pct']:.1f}%) | Avg Depth: {res['avg_depth']:.1f} | Avg Nodes: {int(res['avg_nodes']):,}")
        else:
            print(f"\n" + "=" * 75)
            print(f"Running Fixed Depth: Depth {args.depth} on {engine_path.name}")
            print("=" * 75)
            res = evaluate_tier(engine, puzzles, depth=args.depth, verbose=args.verbose)
            tier_results[f"d{args.depth}"] = res

    finally:
        engine.close()

    # Summary table across all tiers
    print("\n" + "#" * 75)
    print(f"MULTI-TIER TACTICAL BENCHMARK SUMMARY: {engine_path.name}")
    print("#" * 75)
    print(f"{'Tier':<10} {'Solved':<16} {'Avg Depth':<12} {'Avg Time':<12} {'Avg Nodes':<14} {'Mating':<10} {'Motifs':<10} {'Advanced':<10} {'Endgame':<10} {'Master/GM':<10}")
    print("-" * 105)
    for t_name, r in tier_results.items():
        cat_str = [f"{r['categories'][c]['passed']}/{r['categories'][c]['total']}" for c in ["mating", "motifs", "advanced", "endgame", "master_gm"]]
        print(f"{t_name:<10} {r['passed']}/{r['total']} ({r['pct']:4.1f}%)  d{r['avg_depth']:<10.1f} {r['avg_ms']:<8.1f}ms  {int(r['avg_nodes']):<13,} {cat_str[0]:<10} {cat_str[1]:<10} {cat_str[2]:<10} {cat_str[3]:<10} {cat_str[4]:<10}")
    print("#" * 75)


if __name__ == "__main__":
    main()
