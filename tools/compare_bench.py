#!/usr/bin/env python3
"""Compare two CSV files created by tools/bench.py."""

import argparse
import csv
import statistics


def number(row, field):
    value = row.get(field, "")
    return float(value) if value else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=argparse.FileType("r", encoding="utf-8"))
    parser.add_argument("candidate", type=argparse.FileType("r", encoding="utf-8"))
    args = parser.parse_args()

    old_rows = {row["position_id"]: row for row in csv.DictReader(args.baseline)}
    new_rows = {row["position_id"]: row for row in csv.DictReader(args.candidate)}
    common = sorted(old_rows.keys() & new_rows.keys())
    if not common:
        parser.error("the two files have no position IDs in common")

    ratios = []
    changed_moves = 0
    print(
        "position,old_nodes,new_nodes,node_ratio,old_nps,new_nps,nps_ratio,"
        "old_move,new_move,score_delta_cp,old_qnode_pct,new_qnode_pct,"
        "old_tt_keyhit_pct,new_tt_keyhit_pct,old_tt_cutoff_pct,new_tt_cutoff_pct,"
        "old_first_move_cutoff_pct,new_first_move_cutoff_pct"
    )
    for position_id in common:
        old = old_rows[position_id]
        new = new_rows[position_id]
        old_nodes = number(old, "nodes")
        new_nodes = number(new, "nodes")
        old_nps = number(old, "nps")
        new_nps = number(new, "nps")
        old_score = number(old, "score")
        new_score = number(new, "score")
        score_delta = ""
        if old.get("score_type") == new.get("score_type") == "cp" and old_score is not None and new_score is not None:
            score_delta = int(new_score - old_score)

        node_ratio = new_nodes / old_nodes if old_nodes else None
        nps_ratio = new_nps / old_nps if old_nps else None
        if nps_ratio is not None:
            ratios.append(nps_ratio)
        if old.get("bestmove") != new.get("bestmove"):
            changed_moves += 1

        values = [
            position_id,
            old.get("nodes", ""),
            new.get("nodes", ""),
            f"{node_ratio:.3f}" if node_ratio is not None else "",
            old.get("nps", ""),
            new.get("nps", ""),
            f"{nps_ratio:.3f}" if nps_ratio is not None else "",
            old.get("bestmove", ""),
            new.get("bestmove", ""),
            score_delta,
            old.get("qnode_pct", ""),
            new.get("qnode_pct", ""),
            old.get("tt_keyhit_pct", ""),
            new.get("tt_keyhit_pct", ""),
            old.get("tt_cutoff_pct", ""),
            new.get("tt_cutoff_pct", ""),
            old.get("first_move_cutoff_pct", ""),
            new.get("first_move_cutoff_pct", ""),
        ]
        print(",".join(str(value) for value in values))

    if ratios:
        print(
            f"\nMedian NPS ratio candidate/baseline: {statistics.median(ratios):.3f}"
        )
    print(f"Best move changed on {changed_moves}/{len(common)} positions.")
    print("Score deltas are for diagnosis only; changing evaluation changes their scale.")


if __name__ == "__main__":
    main()
