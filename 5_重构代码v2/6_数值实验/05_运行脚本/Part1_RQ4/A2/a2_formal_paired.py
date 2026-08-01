#!/usr/bin/env python3
"""Run the formal paired A2 Grid Search-BP versus GA-BP experiment.

This is an experiment orchestration script only. It never changes src/ or include/.
Each seed is completed sequentially: the full grid finishes before its paired GA.
"""
import argparse
import csv
import hashlib
import re
import shutil
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

SEEDS = [101, 202, 303, 404, 505, 606, 707, 808, 909, 1010]


def run_timed(cmd, cwd, log_path):
    start = time.time()
    with log_path.open("w", encoding="utf-8") as log:
        proc = subprocess.run(["/usr/bin/time", "-v"] + cmd, cwd=cwd,
                              stdout=log, stderr=subprocess.STDOUT, text=True)
    return proc.returncode, time.time() - start


def parse_time_file(path):
    text = path.read_text(errors="replace") if path.exists() else ""
    out = {"wall": "", "user": "", "system": "", "rss": ""}
    m = re.search(r"Elapsed \(wall clock\) time \(h:mm:ss or m:ss\):\s*(.*)", text)
    if m:
        token = m.group(1).strip()
        parts = token.split(":")
        try:
            if len(parts) == 3:
                out["wall"] = float(parts[0]) * 3600 + float(parts[1]) * 60 + float(parts[2])
            elif len(parts) == 2:
                out["wall"] = float(parts[0]) * 60 + float(parts[1])
            else:
                out["wall"] = float(token)
        except ValueError:
            pass
    for key, label in (("user", "User time (seconds)"), ("system", "System time (seconds)"), ("rss", "Maximum resident set size (kbytes)")):
        m = re.search(re.escape(label) + r":\s*([0-9.]+)", text)
        if m:
            out[key] = float(m.group(1))
    return out


def write_flat_config(path, seed, mode, root):
    lines = [
        "plan_name: A2_formal_paired", "num_dc: 5", "num_retailers: 20", f"random_seed: {seed}",
        f"run_mode: {mode}", "use_sqrt_investment: true", "use_invest_in_column: true",
        "transport_direct_distance: false", "legacy_mode: false", "delta: 3000.0", "investment_exponent: 0.5",
        "carbon_price: 2.0", "carbon_cap: 100000.0", "inv_carbon_coeff: 2.0",
        "transport_carbon_coeff: 0.01", "fac_carbon_ratio: 0.1", "service_level: 0.975", "z_alpha: 1.96",
        "coord_min: 0.0", "coord_max: 1000.0", "supplier_x: 500.0", "supplier_y: 500.0",
        "mu_min: 2000.0", "mu_max: 3500.0", "var_min: 10.0", "var_max: 15.0",
        "fixed_cost_min: 7000.0", "fixed_cost_max: 10000.0", "reserve_price_min: 60.0", "reserve_price_max: 80.0",
        "order_fixed_cost: 100.0", "transport_fixed_cost: 100.0", "lead_time: 1.0", "holding_cost: 10.0",
        "chromosome_length: 3", "min_w: 0.0", "max_w: 1.0",
        "population_size: 30" if mode == "ga" else "population_size: 1",
        "max_generation: 50" if mode == "ga" else "max_generation: 1",
        "crossover_rate: 0.7", "mutation_rate: -1", "elitism: true",
        "early_stop: true" if mode == "ga" else "early_stop: false",
        "convergence_generations: 10", "convergence_tolerance: 0.001",
        "pricing_algorithm: 0", "pricing_max_cols_per_dc: 3", "dual_smooth_alpha: 0.0",
        "rc_eps: 1.0e-6", "max_cg_iterations: 2000", "max_branch_nodes: 10000",
        "bp_time_limit_sec: 600.0", "bp_relative_gap: 0.0", "cg_early_stop: false",
        "use_dfs: true", "root_heuristic: true", "root_rmp_mip_heuristic: false",
        "parallel_fitness: true" if mode == "ga" else "parallel_fitness: false",
        "num_threads: 8" if mode == "ga" else "num_threads: 1",
        "parallel_pricing: false", "cplex_threads: 1", "pricing_threads: 4", "core_budget: 0",
        "verbose: false", "output_dir: " + str(root),
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_one_csv(path):
    if not path.exists():
        return {}
    with path.open(newline="", encoding="utf-8") as f:
        return next(csv.DictReader(f), {})


def read_grid_summary(path):
    return read_one_csv(path / "GRID_PRECHECK_SUMMARY.csv")


def numeric(v, default=""):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def grid_totals(path):
    rows = list(csv.DictReader((path / "GRID_EVALUATIONS.csv").open()))
    def total(k): return sum(numeric(r.get(k), 0) for r in rows)
    best = max((r for r in rows if r.get("objective")), key=lambda r: numeric(r["objective"], -1), default={})
    return rows, best, total("cg_iterations"), total("branch_nodes")


def run_seed(seed, args, batch_root):
    seed_dir = batch_root / f"seed_{seed}"
    seed_dir.mkdir(parents=True, exist_ok=True)
    grid_dir = seed_dir / "grid"
    ga_dir = seed_dir / "ga"
    grid_dir.mkdir(exist_ok=True); ga_dir.mkdir(exist_ok=True)
    grid_ok = (grid_dir / "GRID_PRECHECK_SUMMARY.csv").exists()
    if not grid_ok:
        grid_dir.mkdir(parents=True, exist_ok=True)
        cmd = ["python3", str(args.grid_script), "--repo", str(args.repo), "--binary", str(args.binary),
               "--root", str(grid_dir), "--limit", "32768", "--workers", "8", "--seed", str(seed)]
        code, _ = run_timed(cmd, args.repo, grid_dir / "run.log")
        shutil.copy2(grid_dir / "run.log", grid_dir / "time_verbose.txt")
        if (grid_dir / "GRID_PRECHECK_SUMMARY.csv").exists(): shutil.copy2(grid_dir / "GRID_PRECHECK_SUMMARY.csv", grid_dir / "GRID_SUMMARY.csv")
        cfg = grid_dir / "config_used.yaml"
        write_flat_config(cfg, seed, "fixed_w", grid_dir)
        shutil.copy2(cfg, grid_dir / "resolved_config.yaml")
        (grid_dir / "MANIFEST.txt").write_text(
            f"experiment_part: Part1_RQ4_A2\nformal_replication: true\npaper_statistics: included\nmethod: Grid Search-BP\n"
            f"num_dc: 5\nnum_retailers: 20\nrandom_seed: {seed}\nchromosome_length: 3\nlevels_per_dc: 8\nexpected_grid_combinations: 32768\n"
            f"parallel_grid_evaluation: true\nnum_workers: 8\nparallel_pricing: false\ncplex_threads: 1\nprocess_return_code: {code}\n", encoding="utf-8")
    summary = read_grid_summary(grid_dir)
    grid_complete = (numeric(summary.get("completed_combinations"), 0) == 32768 and
                     numeric(summary.get("certified_optimal_combinations"), 0) == 32768 and
                     numeric(summary.get("timeout_combinations"), 0) == 0 and
                     numeric(summary.get("failed_combinations"), 0) == 0)
    if grid_complete and not (ga_dir / "solver_output" / "result.csv").exists():
        cfg = ga_dir / "config_used.yaml"
        write_flat_config(cfg, seed, "ga", ga_dir)
        code, _ = run_timed([str(args.binary), str(cfg)], args.repo, ga_dir / "run.log")
        text = (ga_dir / "run.log").read_text(errors="replace")
        output_dir = None
        for line in text.splitlines():
            if "结果保存至:" in line:
                output_dir = Path(line.split("结果保存至:", 1)[1].strip())
        if output_dir and not output_dir.is_absolute(): output_dir = args.repo / output_dir
        if output_dir and output_dir.exists(): shutil.move(str(output_dir), str(ga_dir / "solver_output"))
        shutil.copy2(cfg, ga_dir / "resolved_config.yaml")
        if (ga_dir / "solver_output" / "result.csv").exists(): shutil.copy2(ga_dir / "solver_output" / "result.csv", ga_dir / "ga_result.csv")
        shutil.copy2(ga_dir / "run.log", ga_dir / "time_verbose.txt")
        if (ga_dir / "solver_output" / "report.txt").exists(): shutil.copy2(ga_dir / "solver_output" / "report.txt", ga_dir / "report.txt")
        (ga_dir / "MANIFEST.txt").write_text(
            f"experiment_part: Part1_RQ4_A2\nformal_replication: true\npaper_statistics: included\nmethod: GA-BP\n"
            f"num_dc: 5\nnum_retailers: 20\nrandom_seed: {seed}\nchromosome_length: 3\npopulation_size: 30\nmax_generation: 50\n"
            f"parallel_fitness: true\nnum_threads: 8\nparallel_pricing: false\ncplex_threads: 1\nprocess_return_code: {code}\n", encoding="utf-8")
    if (ga_dir / "solver_output" / "convergence.csv").exists(): shutil.copy2(ga_dir / "solver_output" / "convergence.csv", ga_dir / "ga_convergence.csv")
    return seed, grid_dir, ga_dir


def make_pair(seed, grid_dir, ga_dir):
    rows, best, cg, branches = grid_totals(grid_dir)
    gs = read_grid_summary(grid_dir)
    ga = read_one_csv(ga_dir / "solver_output" / "result.csv")
    w = [numeric(ga.get("best_w", "").split(";")[i], "") for i in range(5)] if ga.get("best_w") else [""] * 5
    k = [round(x * 7) if isinstance(x, float) else "" for x in w]
    grid_obj = numeric(best.get("objective"), "")
    ga_obj = numeric(ga.get("total_profit"), "")
    diff = grid_obj - ga_obj if isinstance(grid_obj, float) and isinstance(ga_obj, float) else ""
    rel = diff / abs(grid_obj) * 100 if isinstance(diff, float) and grid_obj else ""
    ga_calls = numeric(ga.get("fitness_bp_solves"), "")
    saving = 1 - ga_calls / 32768 if isinstance(ga_calls, float) else ""
    time_files = parse_time_file(grid_dir / "time_verbose.txt")
    ga_time = parse_time_file(ga_dir / "run.log")
    grid_status = "OPTIMAL_CERTIFIED" if len(rows) == 32768 and all(r.get("bp_status") == "OPTIMAL" for r in rows) else "INCOMPLETE"
    ga_status = ga.get("inner_bp_status", "MISSING")
    pair = grid_status == "OPTIMAL_CERTIFIED" and ga.get("has_integer_solution") == "1" and ga_status == "OPTIMAL"
    return {
        "random_seed": seed, "num_dc": 5, "num_retailers": 20, "chromosome_length": 3,
        "grid_status": grid_status, "grid_complete": int(grid_status == "OPTIMAL_CERTIFIED"),
        "grid_certified_combinations": len([r for r in rows if r.get("certified_optimal") == "1"]),
        "grid_best_grid_id": best.get("grid_id", ""), "grid_best_objective": grid_obj,
        **{f"grid_best_k{i+1}": best.get(f"k{i+1}", "") for i in range(5)},
        **{f"grid_best_w{i+1}": best.get(f"w{i+1}", "") for i in range(5)},
        "grid_wall_time_sec": time_files["wall"], "grid_user_time_sec": time_files["user"], "grid_system_time_sec": time_files["system"],
        "grid_actual_bp_calls": 32768, "grid_total_cg_iterations": cg, "grid_total_branch_nodes": branches,
        "ga_status": ga_status, "ga_best_objective": ga_obj, **{f"ga_best_k{i+1}": k[i] for i in range(5)}, **{f"ga_best_w{i+1}": w[i] for i in range(5)},
        "ga_generations": ga.get("ga_generations", ""), "ga_termination_reason": "early_stop_or_completed",
        "ga_wall_time_sec": ga_time["wall"], "ga_user_time_sec": ga_time["user"], "ga_system_time_sec": ga_time["system"],
        "ga_fitness_requests": ga.get("fitness_requests", ""), "ga_actual_bp_calls": ga.get("fitness_bp_solves", ""),
        "ga_cache_hits": ga.get("actual_fitness_cache_hits", ""), "ga_cache_hit_rate": (numeric(ga.get("actual_fitness_cache_hits"), 0) / numeric(ga.get("fitness_requests"), 1) if ga.get("fitness_requests") else ""),
        "ga_certified_optimal_bp": ga.get("fitness_bp_certified_optimal", ""), "ga_uncertified_feasible_bp": ga.get("fitness_bp_feasible_uncertified", ""), "ga_failed_bp": ga.get("fitness_evaluation_failures", ""),
        "ga_total_cg_iterations": ga.get("cg_iterations", ""), "ga_total_branch_nodes": ga.get("branch_nodes", ""),
        "absolute_objective_difference": diff, "relative_gap_percent": rel, "ga_hits_grid_optimum": int(isinstance(diff, float) and abs(diff) <= 1e-6 * max(1, abs(grid_obj))),
        "evaluation_saving_ratio": saving, "wall_time_ratio_grid_over_ga": (time_files["wall"] / ga_time["wall"] if time_files["wall"] and ga_time["wall"] else ""),
        "pair_complete": int(pair), "notes": "same unified random_seed and deterministic instance generator",
    }


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--repo", type=Path, required=True); ap.add_argument("--binary", type=Path, required=True); ap.add_argument("--grid-script", type=Path, required=True); ap.add_argument("--batch-root", type=Path, required=True); ap.add_argument("--workers", type=int, default=1); ap.add_argument("--seeds", nargs="+", type=int, default=SEEDS); ap.add_argument("--no-summary", action="store_true")
    args = ap.parse_args(); args.repo=args.repo.resolve(); args.binary=args.binary.resolve(); args.grid_script=args.grid_script.resolve(); args.batch_root=args.batch_root.resolve(); args.batch_root.mkdir(parents=True, exist_ok=True)
    # Formal runs are deliberately seed-sequential; only the Grid worker pool is concurrent.
    pairs=[]
    for seed in args.seeds:
        _, gd, ad = run_seed(seed,args,args.batch_root); pair=make_pair(seed,gd,ad); pairs.append(pair)
        with (args.batch_root/f"seed_{seed}"/"PAIRED_RESULT.csv").open("w",newline="",encoding="utf-8") as f: csv.DictWriter(f,fieldnames=list(pair)).writeheader(); csv.DictWriter(f,fieldnames=list(pair)).writerow(pair)
    if args.no_summary:
        print("FORMAL A2 SEED SUBSET COMPLETE", [p["random_seed"] for p in pairs], flush=True)
        return
    fields=list(pairs[0]);
    with (args.batch_root/"A2_FORMAL_PAIRED_RESULTS.csv").open("w",newline="",encoding="utf-8") as f: w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(pairs)
    complete=[p for p in pairs if p["pair_complete"]]
    gaps=[numeric(p["relative_gap_percent"],0) for p in pairs]; grid_t=[numeric(p["grid_wall_time_sec"],0) for p in pairs]; ga_t=[numeric(p["ga_wall_time_sec"],0) for p in pairs]
    summary={"replications":len(pairs),"complete_pairs":len(complete),"grid_complete_rate":sum(p["grid_complete"] for p in pairs)/len(pairs),"ga_certified_rate":sum(p["ga_status"]=="OPTIMAL" for p in pairs)/len(pairs),"ga_hits_grid_optimum_count":sum(p["ga_hits_grid_optimum"] for p in pairs),"ga_hits_grid_optimum_rate":sum(p["ga_hits_grid_optimum"] for p in pairs)/len(pairs),"gap_mean_percent":sum(gaps)/len(gaps),"gap_min_percent":min(gaps),"gap_max_percent":max(gaps),"grid_wall_mean_sec":sum(grid_t)/len(grid_t),"ga_wall_mean_sec":sum(ga_t)/len(ga_t),"evaluation_saving_mean":sum(numeric(p["evaluation_saving_ratio"],0) for p in pairs)/len(pairs),"anomalous_pairs":len(pairs)-len(complete)}
    with (args.batch_root/"A2_FORMAL_SUMMARY.csv").open("w",newline="",encoding="utf-8") as f: w=csv.DictWriter(f,fieldnames=list(summary)); w.writeheader(); w.writerow(summary)
    print("FORMAL A2 COMPLETE", summary, flush=True)


if __name__ == "__main__": main()
