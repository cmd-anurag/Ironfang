#!/usr/bin/env python3
"""Run a deterministic, fixed-depth UCI benchmark and save one CSV row per FEN."""

import argparse
import csv
import pathlib
import queue
import subprocess
import sys
import threading
import time


FIELDS = [
    "label",
    "position_id",
    "category",
    "fen",
    "depth",
    "seldepth",
    "time_ms",
    "nodes",
    "nps",
    "hashfull",
    "score_type",
    "score",
    "bestmove",
    "pv",
    "qnodes",
    "qnode_pct",
    "tt_probes",
    "tt_keyhits",
    "tt_keyhit_pct",
    "tt_cutoffs",
    "tt_cutoff_pct",
    "tt_store_attempts",
    "tt_writes",
    "tt_overwrites",
    "ab_cutoffs",
    "first_move_cutoffs",
    "first_move_cutoff_pct",
]


def int_value(tokens, name):
    try:
        return int(tokens[tokens.index(name) + 1])
    except (ValueError, IndexError):
        return ""


def percent(numerator, denominator):
    try:
        return f"{100.0 * int(numerator) / int(denominator):.2f}"
    except (TypeError, ValueError, ZeroDivisionError):
        return ""


def run_position(engine, fen, depth, timeout):
    process = subprocess.Popen(
        [str(engine)],
        text=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        cwd=str(engine.parent),
    )
    lines = queue.Queue()

    def read_output():
        try:
            for output_line in process.stdout:
                lines.put(output_line.rstrip("\r\n"))
        finally:
            lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    output = []

    def send(command):
        process.stdin.write(command + "\n")
        process.stdin.flush()

    def read_until(predicate, deadline):
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timed out waiting for engine response to {fen}")
            try:
                line = lines.get(timeout=remaining)
            except queue.Empty as error:
                raise TimeoutError(f"timed out waiting for engine response to {fen}") from error
            if line is None:
                raise RuntimeError("engine closed stdout before completing the UCI search")
            output.append(line)
            if predicate(line):
                return line

    try:
        deadline = time.monotonic() + timeout
        send("uci")
        read_until(lambda line: line.strip() == "uciok", deadline)
        send("isready")
        read_until(lambda line: line.strip() == "readyok", deadline)
        send("ucinewgame")
        send(f"position fen {fen}")
        send(f"go depth {depth}")
        read_until(lambda line: line.startswith("bestmove "), deadline)
        send("quit")
        process.stdin.close()
        process.wait(timeout=max(1.0, deadline - time.monotonic()))
        if process.returncode != 0:
            raise RuntimeError(f"engine exited with code {process.returncode}")
    except Exception:
        process.kill()
        process.wait()
        raise
    finally:
        reader.join(timeout=1)

    row = {}
    for line in output:
        if "info string invalid FEN position" in line:
            raise RuntimeError(f"engine rejected benchmark FEN: {fen}")
        tokens = line.split()
        if tokens[:3] == ["info", "string", "stats"]:
            for token in tokens[3:]:
                if "=" in token:
                    key, value = token.split("=", 1)
                    row[key] = value
            continue

        if tokens[:2] == ["info", "depth"]:
            row["depth"] = int_value(tokens, "depth")
            row["seldepth"] = int_value(tokens, "seldepth")
            row["time_ms"] = int_value(tokens, "time")
            row["nodes"] = int_value(tokens, "nodes")
            row["nps"] = int_value(tokens, "nps")
            row["hashfull"] = int_value(tokens, "hashfull")
            try:
                score_index = tokens.index("score")
                row["score_type"] = tokens[score_index + 1]
                row["score"] = tokens[score_index + 2]
            except (ValueError, IndexError):
                pass
            try:
                pv_index = tokens.index("pv")
                row["pv"] = " ".join(tokens[pv_index + 1 :])
            except ValueError:
                row["pv"] = ""
        elif tokens and tokens[0] == "bestmove" and len(tokens) > 1:
            row["bestmove"] = tokens[1]

    if "bestmove" not in row:
        raise RuntimeError(
            "engine did not return bestmove; output was:\n" + result.stdout
        )
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=pathlib.Path)
    parser.add_argument("--label", default="engine")
    parser.add_argument("--depth", type=int, default=8)
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument(
        "--positions",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parent.parent
        / "bench"
        / "positions.csv",
    )
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("bench-results.csv"))
    args = parser.parse_args()

    engine = args.engine.resolve()
    if not engine.is_file():
        parser.error(f"engine executable does not exist: {engine}")
    if args.depth < 1:
        parser.error("depth must be at least 1")

    with args.positions.open(newline="", encoding="utf-8") as source:
        positions = list(csv.DictReader(source))
    if not positions:
        parser.error(f"position suite is empty: {args.positions}")

    results = []
    for position in positions:
        print(f"{position['id']}: searching depth {args.depth}...", flush=True)
        row = run_position(engine, position["fen"], args.depth, args.timeout)
        row.update(
            {
                "label": args.label,
                "position_id": position["id"],
                "category": position["category"],
                "fen": position["fen"],
            }
        )
        row["qnode_pct"] = percent(row.get("qnodes"), row.get("nodes"))
        row["tt_keyhit_pct"] = percent(
            row.get("tt_keyhits"), row.get("tt_probes")
        )
        row["tt_cutoff_pct"] = percent(
            row.get("tt_cutoffs"), row.get("tt_probes")
        )
        row["first_move_cutoff_pct"] = percent(
            row.get("first_move_cutoffs"), row.get("ab_cutoffs")
        )
        results.append(row)
        print(
            f"  bestmove {row.get('bestmove', '?')}, "
            f"depth {row.get('depth', '?')}, "
            f"nodes {row.get('nodes', '?')}, "
            f"nps {row.get('nps', '?')}"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=FIELDS, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(results)
    print(f"Saved {len(results)} positions to {args.output.resolve()}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.TimeoutExpired, RuntimeError) as error:
        print(f"benchmark failed: {error}", file=sys.stderr)
        sys.exit(1)
