#!/usr/bin/env python3
"""Compare two CSV files created by tools/bench.py."""

import argparse
import csv
import statistics


def number(row, field):
    value = row.get(field, "")
    if not value:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


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
    time_ratios = []
    depth_deltas = []
    changed_moves = 0
    old_sf_matches = 0
    new_sf_matches = 0
    reference_count = 0
    print(
        "position,old_depth,new_depth,depth_delta,"
        "old_nodes,new_nodes,node_ratio,old_time_ms,new_time_ms,time_ratio,"
        "old_nps,new_nps,nps_ratio,"
        "old_move,new_move,sf_reference_move,old_sf_match,new_sf_match,"
        "score_delta_cp,old_qnode_pct,new_qnode_pct,"
        "old_tt_keyhit_pct,new_tt_keyhit_pct,old_tt_cutoff_pct,new_tt_cutoff_pct,"
        "old_first_move_cutoff_pct,new_first_move_cutoff_pct,"
        "old_lmr_reductions,new_lmr_reductions,"
        "old_lmr_researches,new_lmr_researches,"
        "old_lmr_check_exemptions,new_lmr_check_exemptions,"
        "old_root_pvs_researches,new_root_pvs_researches"
    )
    for position_id in common:
        old = old_rows[position_id]
        new = new_rows[position_id]
        old_depth = number(old, "depth")
        new_depth = number(new, "depth")
        old_nodes = number(old, "nodes")
        new_nodes = number(new, "nodes")
        old_time = number(old, "time_ms")
        new_time = number(new, "time_ms")
        old_nps = number(old, "nps")
        new_nps = number(new, "nps")
        old_score = number(old, "score")
        new_score = number(new, "score")
        score_delta = ""
        if old.get("score_type") == new.get("score_type") == "cp" and old_score is not None and new_score is not None:
            score_delta = int(new_score - old_score)

        node_ratio = (
            new_nodes / old_nodes
            if old_nodes not in (None, 0) and new_nodes is not None
            else None
        )
        time_ratio = (
            new_time / old_time
            if old_time not in (None, 0) and new_time is not None
            else None
        )
        depth_delta = new_depth - old_depth if old_depth is not None and new_depth is not None else None
        nps_ratio = (
            new_nps / old_nps
            if old_nps not in (None, 0) and new_nps is not None
            else None
        )
        if depth_delta is not None:
            depth_deltas.append(depth_delta)
        if nps_ratio is not None:
            ratios.append(nps_ratio)
        if time_ratio is not None:
            time_ratios.append(time_ratio)
        if old.get("bestmove") != new.get("bestmove"):
            changed_moves += 1
        if old.get("sf_reference_move") or new.get("sf_reference_move"):
            reference_count += 1
            old_sf_matches += old.get("matches_sf_reference") == "true"
            new_sf_matches += new.get("matches_sf_reference") == "true"

        values = [
            position_id,
            old.get("depth", ""),
            new.get("depth", ""),
            f"{depth_delta:.0f}" if depth_delta is not None else "",
            old.get("nodes", ""),
            new.get("nodes", ""),
            f"{node_ratio:.3f}" if node_ratio is not None else "",
            old.get("time_ms", ""),
            new.get("time_ms", ""),
            f"{time_ratio:.3f}" if time_ratio is not None else "",
            old.get("nps", ""),
            new.get("nps", ""),
            f"{nps_ratio:.3f}" if nps_ratio is not None else "",
            old.get("bestmove", ""),
            new.get("bestmove", ""),
            new.get("sf_reference_move") or old.get("sf_reference_move", ""),
            old.get("matches_sf_reference", ""),
            new.get("matches_sf_reference", ""),
            score_delta,
            old.get("qnode_pct", ""),
            new.get("qnode_pct", ""),
            old.get("tt_keyhit_pct", ""),
            new.get("tt_keyhit_pct", ""),
            old.get("tt_cutoff_pct", ""),
            new.get("tt_cutoff_pct", ""),
            old.get("first_move_cutoff_pct", ""),
            new.get("first_move_cutoff_pct", ""),
            old.get("lmr_reductions", ""),
            new.get("lmr_reductions", ""),
            old.get("lmr_researches", ""),
            new.get("lmr_researches", ""),
            old.get("lmr_check_exemptions", ""),
            new.get("lmr_check_exemptions", ""),
            old.get("root_pvs_researches", ""),
            new.get("root_pvs_researches", ""),
        ]
        print(",".join(str(value) for value in values))

    if ratios:
        print(
            f"\nMedian NPS ratio candidate/baseline: {statistics.median(ratios):.3f}"
        )
    if time_ratios:
        print(
            f"Median time ratio candidate/baseline: {statistics.median(time_ratios):.3f}"
        )
    if depth_deltas:
        print(
            f"Median depth change candidate/baseline: {statistics.median(depth_deltas):+.1f} plies"
        )
    print(f"Best move changed on {changed_moves}/{len(common)} positions.")
    if reference_count:
        print(
            f"Matches to supplied Stockfish references: "
            f"{old_sf_matches}/{reference_count} -> {new_sf_matches}/{reference_count}."
        )
    print("Score deltas are for diagnosis only; changing evaluation changes their scale.")


if __name__ == "__main__":
    main()
