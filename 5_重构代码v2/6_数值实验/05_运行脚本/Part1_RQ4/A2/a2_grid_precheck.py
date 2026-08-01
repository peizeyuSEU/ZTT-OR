#!/usr/bin/env python3
import argparse
import csv
import itertools
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path


def yaml_scalar(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def make_config(path, w, root):
    lines = [
        "plan_name: a2_grid_precheck_seed101",
        "plan_desc: A2 feasibility precheck; fixed w BP only",
        "num_dc: 5", "num_retailers: 20", "random_seed: 101",
        "run_mode: fixed_w", "fixed_w: [" + ", ".join(f"{x:.15g}" for x in w) + "]",
        "use_sqrt_investment: true", "use_invest_in_column: true",
        "transport_direct_distance: false", "legacy_mode: false",
        "delta: 3000.0", "investment_exponent: 0.5",
        "carbon_price: 2.0", "carbon_cap: 100000.0",
        "inv_carbon_coeff: 2.0", "transport_carbon_coeff: 0.01", "fac_carbon_ratio: 0.1",
        "service_level: 0.975", "z_alpha: 1.96",
        "coord_min: 0.0", "coord_max: 1000.0", "supplier_x: 500.0", "supplier_y: 500.0",
        "mu_min: 2000.0", "mu_max: 3500.0", "var_min: 10.0", "var_max: 15.0",
        "fixed_cost_min: 7000.0", "fixed_cost_max: 10000.0",
        "reserve_price_min: 60.0", "reserve_price_max: 80.0",
        "order_fixed_cost: 100.0", "transport_fixed_cost: 100.0", "lead_time: 1.0", "holding_cost: 10.0",
        "chromosome_length: 3", "min_w: 0.0", "max_w: 1.0",
        "population_size: 1", "max_generation: 1", "crossover_rate: 0.7", "mutation_rate: -1", "elitism: true",
        "early_stop: false", "convergence_generations: 10", "convergence_tolerance: 0.001",
        "pricing_algorithm: 0", "pricing_max_cols_per_dc: 3", "dual_smooth_alpha: 0.0",
        "rc_eps: 1.0e-6", "max_cg_iterations: 2000", "max_branch_nodes: 10000",
        "bp_time_limit_sec: 600.0", "bp_relative_gap: 0.0", "cg_early_stop: false",
        "use_dfs: true", "root_heuristic: true", "root_rmp_mip_heuristic: false",
        "parallel_fitness: false", "num_threads: 1", "parallel_pricing: false",
        "cplex_threads: 1", "pricing_threads: 4", "core_budget: 0", "verbose: false",
        "output_dir: " + str(root / "runs"),
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_result(path):
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    return rows[0] if rows else {}


def run_one(item, args):
    gid, digits, w, root = item
    cfg = root / "configs" / f"grid_{gid:05d}.yaml"
    make_config(cfg, w, root)
    start = time.time()
    log = root / "checkpoint" / f"grid_{gid:05d}.launcher.log"
    cmd = [str(args.binary), str(cfg)]
    proc = subprocess.run(cmd, cwd=args.repo, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
    elapsed = time.time() - start
    log.write_text(proc.stdout, encoding="utf-8", errors="replace")
    marker = "结果保存至:"
    output_dir = None
    for line in proc.stdout.splitlines():
        if marker in line:
            output_dir = Path(line.split(marker, 1)[1].strip())
    dest = root / "runs" / f"grid_{gid:05d}"
    dest.mkdir(parents=True, exist_ok=True)
    if output_dir and output_dir.exists():
        shutil.move(str(output_dir), str(dest / "solver_output"))
    shutil.copy2(cfg, dest / "config_used.yaml")
    shutil.copy2(cfg, dest / "resolved_config.yaml")
    result = {}
    result_csv = dest / "solver_output" / "result.csv"
    if result_csv.exists():
        result = read_result(result_csv)
    status = result.get("inner_bp_status", "PROCESS_ERROR" if proc.returncode else "MISSING_RESULT")
    objective = result.get("total_profit", "")
    row = {
        "grid_id": gid, "k1": digits[0], "k2": digits[1], "k3": digits[2],
        "k4": digits[3], "k5": digits[4],
        "w1": w[0], "w2": w[1], "w3": w[2], "w4": w[3], "w5": w[4],
        "objective": objective, "bp_status": status,
        "integer_solution": result.get("has_integer_solution", ""),
        "certified_optimal": "1" if status == "OPTIMAL" and result.get("has_integer_solution") == "1" else "0",
        "bp_gap": result.get("inner_bp_relative_gap", ""), "wall_time_sec": result.get("bp_time", elapsed),
        "cg_iterations": result.get("cg_iterations", ""), "branch_nodes": result.get("branch_nodes", ""),
        "processed_nodes": result.get("processed_nodes", ""), "pruned_nodes": result.get("pruned_nodes", ""),
        "timeout": "1" if status == "TIME_LIMIT" else "0",
        "error_message": "" if proc.returncode == 0 else f"returncode={proc.returncode}",
    }
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", type=Path, required=True)
    ap.add_argument("--binary", type=Path, required=True)
    ap.add_argument("--root", type=Path, required=True)
    ap.add_argument("--limit", type=int, default=64)
    ap.add_argument("--workers", type=int, default=8)
    args = ap.parse_args()
    args.repo = args.repo.resolve(); args.binary = args.binary.resolve(); args.root = args.root.resolve()
    args.root.mkdir(parents=True, exist_ok=True)
    for d in ("configs", "runs", "checkpoint"):
        (args.root / d).mkdir(exist_ok=True)
    levels = range(8)
    all_items = []
    for gid, digits in enumerate(itertools.product(levels, repeat=5)):
        if gid >= args.limit: break
        w = [k / 7.0 for k in digits]
        all_items.append((gid, digits, w, args.root))
    rows = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(run_one, item, args) for item in all_items]
        for fut in as_completed(futures):
            row = fut.result(); rows.append(row)
            print(f"completed grid_id={row['grid_id']} status={row['bp_status']}", flush=True)
    rows.sort(key=lambda r: int(r["grid_id"]))
    fields = ["grid_id","k1","k2","k3","k4","k5","w1","w2","w3","w4","w5","objective","bp_status","integer_solution","certified_optimal","bp_gap","wall_time_sec","cg_iterations","branch_nodes","processed_nodes","pruned_nodes","timeout","error_message"]
    with (args.root / "GRID_EVALUATIONS.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(rows)
    completed = len(rows)
    certified = sum(r["certified_optimal"] == "1" for r in rows)
    timeouts = sum(r["timeout"] == "1" for r in rows)
    failures = sum(r["bp_status"] in ("PROCESS_ERROR", "MISSING_RESULT", "SOLVER_ERROR") for r in rows)
    times = [float(r["wall_time_sec"]) for r in rows if r["wall_time_sec"] not in ("", None)]
    best = max((r for r in rows if r["objective"] not in ("", None)), key=lambda r: float(r["objective"]), default={})
    summary = {
        "num_dc": 5, "num_retailers": 20, "random_seed": 101, "chromosome_length": 3,
        "levels_per_dc": 8, "expected_combinations": 32768, "completed_combinations": completed,
        "certified_optimal_combinations": certified, "uncertified_combinations": completed - certified - failures,
        "failed_combinations": failures, "timeout_combinations": timeouts,
        "total_wall_time_sec": sum(times), "total_user_time_sec": "UNKNOWN", "total_system_time_sec": "UNKNOWN",
        "max_parent_rss_kb": "UNKNOWN", "best_grid_id": best.get("grid_id", ""), "best_objective": best.get("objective", ""),
        "best_w": ";".join(str(best.get(f"w{i}", "")) for i in range(1,6)),
        "average_bp_time_sec": (sum(times)/len(times) if times else ""), "median_bp_time_sec": "UNKNOWN",
        "maximum_bp_time_sec": max(times) if times else "", "total_cg_iterations": "UNKNOWN", "average_cg_iterations": "UNKNOWN", "total_branch_nodes": "UNKNOWN",
    }
    with (args.root / "GRID_PRECHECK_SUMMARY.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary)); writer.writeheader(); writer.writerow(summary)
    print(f"SUMMARY completed={completed} certified={certified} timeouts={timeouts} failures={failures}", flush=True)


if __name__ == "__main__":
    main()
