#!/usr/bin/env python3
import csv, re, sys
from pathlib import Path

REQUIRED=["config_used.yaml","resolved_config.yaml","result.csv","convergence.csv","report.txt","run.log","time_verbose.txt"]
def validate(run_dir, nd=None, nr=None, seed=None):
    p=Path(run_dir); errors=[]
    for name in REQUIRED:
        if not (p/name).is_file() or (p/name).stat().st_size==0: errors.append(f"missing_or_empty:{name}")
    cfg=(p/"config_used.yaml").read_text(errors="ignore") if (p/"config_used.yaml").exists() else ""
    checks={"chromosome_length":"10","population_size":"30","max_generation":"50","num_threads":"8","cplex_threads":"1","parallel_pricing":"false","pricing_max_cols_per_dc":"3","dual_smooth_alpha":"0(?:\\.0+)?"}
    for k,v in checks.items():
        value_pattern=v if k=='dual_smooth_alpha' else re.escape(v)
        if not re.search(rf"^\s*{re.escape(k)}\s*:\s*{value_pattern}\s*$",cfg,re.M): errors.append(f"config_mismatch:{k}={v}")
    for k,v in (("num_dc",nd),("num_retailers",nr),("random_seed",seed)):
        if v is not None and not re.search(rf"^\s*{k}\s*:\s*{v}\s*$",cfg,re.M): errors.append(f"config_mismatch:{k}={v}")
    text="".join((p/x).read_text(errors="ignore") for x in ["report.txt","result.csv"] if (p/x).exists())
    if not re.search(r"hasIntegerSolution\s*[:=]\s*(true|1)|integer solution\s*(true|found)|整数可行",text,re.I): errors.append("integer_solution_not_confirmed")
    bad=re.findall(r"SOLVER_ERROR|CPLEX license failure|fork failure|thread failure|file collision|configuration mismatch",text,re.I)
    if bad: errors.append("prohibited_error:"+",".join(sorted(set(bad))))
    # If the frozen launcher exposes best_w, require a complete candidate-DC vector.
    if (p/'result.csv').exists() and nd is not None:
        try:
            rr=list(csv.DictReader((p/'result.csv').open(newline='')))[-1]
            if rr.get('best_w','').strip():
                vals=[float(x) for x in rr['best_w'].split(';') if x.strip()]
                if len(vals)!=int(nd): errors.append(f"w_vector_length:{len(vals)} != num_dc:{nd}")
                if any(not (0.0-1e-9 <= x <= 1.0+1e-9) for x in vals): errors.append('w_vector_out_of_range')
        except Exception as e: errors.append('w_vector_parse_error:'+str(e))
    return (not errors), errors
def main():
    if len(sys.argv)<2: raise SystemExit("usage: a3_validate_run.py RUN_DIR [nd nr seed]")
    ok,errors=validate(sys.argv[1],*(int(x) for x in sys.argv[2:]))
    print("VALID" if ok else "INVALID"); [print(e) for e in errors]; raise SystemExit(0 if ok else 1)
if __name__=="__main__": main()
