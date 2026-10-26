#!/usr/bin/env python3
"""
Test Ironfang against known game blunder positions to prevent regressions.
Usage:
    python tools/test_known_blunders.py --engine build-windows/ironfang.exe
    python tools/test_known_blunders.py --engine D:/engines/v30/v30.exe --movetime 1000
"""

import argparse
import csv
import pathlib
import subprocess
import sys

def query_engine(engine_path, fen, movetime=1000):
    p = subprocess.Popen(
        [str(engine_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    p.stdin.write("uci\n")
    p.stdin.write("isready\n")
    p.stdin.write(f"position fen {fen}\n")
    p.stdin.write(f"go movetime {movetime}\n")
    p.stdin.flush()
    
    bestmove = None
    depth = None
    score = None
    pv = None
    
    for line in p.stdout:
        line = line.strip()
        if "depth" in line and "score" in line:
            parts = line.split()
            if "depth" in parts:
                depth = parts[parts.index("depth") + 1]
            if "cp" in parts:
                score = parts[parts.index("cp") + 1]
            elif "mate" in parts:
                score = f"M{parts[parts.index('mate') + 1]}"
            if "pv" in parts:
                pv = " ".join(parts[parts.index("pv") + 1 : parts.index("pv") + 5])
        if line.startswith("bestmove"):
            parts = line.split()
            if len(parts) > 1:
                bestmove = parts[1]
            break
            
    p.stdin.write("quit\n")
    p.stdin.flush()
    p.terminate()
    return bestmove, depth, score, pv

def main():
    parser = argparse.ArgumentParser(description="Test engine against known blunders.")
    parser.add_argument("--engine", type=pathlib.Path, required=True, help="Path to engine executable")
    parser.add_argument("--movetime", type=int, default=1000, help="Movetime per position in ms")
    parser.add_argument("--blunders", type=pathlib.Path, default=pathlib.Path(__file__).parent / "known_blunders.csv")
    args = parser.parse_args()

    engine_path = args.engine.resolve()
    if not engine_path.is_file():
        print(f"Error: Engine executable not found at {engine_path}", file=sys.stderr)
        sys.exit(1)

    with open(args.blunders, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        positions = list(reader)

    print("=" * 85)
    print(f"KNOWN BLUNDERS REGRESSION SUITE: {engine_path.name} ({len(positions)} positions, {args.movetime}ms)")
    print("=" * 85)

    passed_count = 0
    failed_count = 0

    for pos in positions:
        pid = pos["id"]
        fen = pos["fen"]
        blunder = pos["blunder_move"].strip()
        good_moves = [m.strip() for m in pos["good_moves"].split(";")]
        desc = pos["description"]

        bestmove, depth, score, pv = query_engine(engine_path, fen, args.movetime)

        if bestmove == blunder:
            status = "BLUNDER REPEATED [FAIL]"
            failed_count += 1
        elif bestmove in good_moves:
            status = "GOOD MOVE PLAYED [PASS]"
            passed_count += 1
        else:
            status = f"ACCEPTABLE ALTERNATIVE [PASS]"
            passed_count += 1

        print(f"\n[{pid}] {desc}")
        print(f"  FEN: {fen}")
        print(f"  Engine Move: {bestmove:6s} (d{depth}, {score} cp, PV: {pv})")
        print(f"  Blunder to avoid: {blunder} | Expected: {good_moves}")
        print(f"  Result: {status}")

    print("\n" + "#" * 85)
    print(f"SUMMARY: {passed_count}/{len(positions)} Passed ({100.0 * passed_count / len(positions):.1f}%) | {failed_count} Blunders Repeated")
    print("#" * 85)

    if failed_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
