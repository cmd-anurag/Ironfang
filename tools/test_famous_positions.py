#!/usr/bin/env python3
"""Run Tier 6 regression: Famous Historic Chess Positions Benchmark."""

import argparse
import csv
import subprocess
import sys
import time

def run_famous_positions(engine_path, csv_path="tools/famous_positions.csv", movetime_ms=1000, depth_limit=None):
    positions = []
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            positions.append(row)

    print("=" * 85)
    print(f"TIER 6: FAMOUS HISTORIC CHESS POSITIONS BENCHMARK: {engine_path}")
    print(f"Testing {len(positions)} iconic positions (Movetime: {movetime_ms}ms" + (f", Depth: {depth_limit}" if depth_limit else "") + ")")
    print("=" * 85)

    passed = 0
    total = len(positions)

    for i, pos in enumerate(positions, 1):
        pos_id = pos["id"]
        name = pos["name"]
        fen = pos["fen"]
        best_moves = [m.strip().lower() for m in pos["best_moves"].split(";") if m.strip()]
        desc = pos["description"]

        p = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        p.stdin.write("uci\nisready\n")
        p.stdin.flush()
        while True:
            line = p.stdout.readline()
            if "readyok" in line:
                break

        go_cmd = f"go movetime {movetime_ms}"
        if depth_limit:
            go_cmd = f"go depth {depth_limit}"

        p.stdin.write(f"position fen {fen}\n{go_cmd}\n")
        p.stdin.flush()

        bestmove = ""
        info_line = ""
        while True:
            line = p.stdout.readline()
            if not line:
                break
            line = line.strip()
            if line.startswith("info depth") and "score" in line:
                info_line = line
            if line.startswith("bestmove"):
                bestmove = line.split()[1].strip().lower()
                break

        p.stdin.write("quit\n")
        p.stdin.flush()

        # Parse info
        depth = "?"
        score = "?"
        pv = ""
        for token in info_line.split():
            if token == "depth":
                idx = info_line.split().index(token)
                depth = info_line.split()[idx+1]
            if token == "cp":
                idx = info_line.split().index(token)
                score = info_line.split()[idx+1] + " cp"
        if "pv " in info_line:
            pv = info_line.split("pv ", 1)[1]

        is_pass = bestmove in best_moves
        if is_pass:
            passed += 1
            result_str = "[PASS] FOUND HISTORIC MOVE"
        else:
            result_str = "[FAIL] MISSED HISTORIC MOVE"

        print(f"\n[{i:2d}/{total:2d}] {name} ({pos_id})")
        print(f"  FEN: {fen}")
        print(f"  Engine Move: {bestmove:6s} (d{depth}, {score}, PV: {pv[:40]}...)")
        print(f"  Historic Target(s): {best_moves}")
        print(f"  Context: {desc}")
        print(f"  Result: {result_str}")

    print("\n" + "#" * 85)
    rate = 100.0 * passed / total
    print(f"SUMMARY: {passed}/{total} Historic Positions Solved ({rate:.1f}%)")
    print("#" * 85)
    return passed, total

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="build-windows/Ironfang.exe")
    parser.add_argument("--csv", default="tools/famous_positions.csv")
    parser.add_argument("--movetime", type=int, default=1000)
    parser.add_argument("--depth", type=int, default=None)
    args = parser.parse_args()

    passed, total = run_famous_positions(args.engine, args.csv, args.movetime, args.depth)
    if passed == 0:
        sys.exit(1)
