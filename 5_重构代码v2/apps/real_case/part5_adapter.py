#!/usr/bin/env python3
"""Read-only Part 5 real-case adapter and validator.

This program deliberately stops at input validation.  It does not import or
invoke the GA, branch-and-price, CPLEX, or the synthetic DataGenerator.
"""
import argparse, csv, hashlib, json, math, platform, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path

MODEL_COMMIT = "0dd725354732f9b8011e9e7e8540e0e64a3ff223"
MODEL_TAG = "paper-exp-baseline-20260801"
DATA_COMMIT = "b9fc758dbe11762882bf66cbbc7672da0918adb7"
DATA_BRANCH = "codex/v02-intracity-carbon-finalization"
DATA_VERSION = "v0.2.0-candidate"
DISTANCES = [0, 5, 10, 15, 20]

def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))

def f(row, key):
    return float(row[key])

def sha256(path):
    h=hashlib.sha256()
    with path.open("rb") as g:
        for b in iter(lambda:g.read(1<<20), b""): h.update(b)
    return h.hexdigest()

def git_head(path):
    try: return subprocess.check_output(["git","-C",str(path),"rev-parse","HEAD"],text=True).strip()
    except Exception: return "UNKNOWN"

def write_csv(path, fields, rows):
    with path.open("w", encoding="utf-8", newline="") as g:
        w=csv.DictWriter(g,fieldnames=fields); w.writeheader(); w.writerows(rows)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--intracity-distance-km", type=float, choices=DISTANCES, required=True)
    ap.add_argument("--output-dir", required=True)
    ap.add_argument("--validate-only", action="store_true")
    a=ap.parse_args()
    data=Path(a.data_dir); out=Path(a.output_dir); out.mkdir(parents=True,exist_ok=True)
    files={n:data/n for n in ["case_config.json","nodes.csv","supplier_dc.csv","dc_market.csv","market_params.csv","dc_params.csv","inventory_params.csv","dc_emission_params.csv"]}
    missing=[n for n,p in files.items() if not p.exists()]
    if missing: raise SystemExit("missing data files: "+", ".join(missing))
    cfg=json.loads(files["case_config.json"].read_text(encoding="utf-8"))
    nodes=read_csv(files["nodes.csv"]); sdc=read_csv(files["supplier_dc.csv"])
    dcm=read_csv(files["dc_market.csv"]); markets=read_csv(files["market_params.csv"])
    dcs=read_csv(files["dc_params.csv"]); inv=read_csv(files["inventory_params.csv"])
    em=read_csv(files["dc_emission_params.csv"])
    dc_ids=[r["dc_id"] for r in dcs]; market_ids=[r["market_id"] for r in markets]
    intracity=[r for r in dcm if abs(f(r,"road_distance_km")-10.0)<1e-9 and r["dc_id"]==r["market_id"]]
    checks=[]
    def check(name, observed, expected, ok, note=""):
        checks.append({"check":name,"observed":str(observed),"expected":str(expected),"status":"PASS" if ok else "FAIL","notes":note})
    check("data_version",cfg.get("version"),DATA_VERSION,cfg.get("version")==DATA_VERSION)
    check("supplier_count",sum(r["supplier_flag"]=="1" for r in nodes),1,sum(r["supplier_flag"]=="1" for r in nodes)==1)
    check("candidate_dc_count",len(dcs),8,len(dcs)==8)
    check("market_count",len(markets),15,len(markets)==15)
    check("supplier_dc_arc_count",len(sdc),8,len(sdc)==8)
    check("dc_market_arc_count",len(dcm),120,len(dcm)==120)
    check("intracity_arc_count",len(intracity),8,len(intracity)==8,"same-city arcs are dc_id=market_id and baseline distance 10 km")
    check("dc_keys_unique",len(set(dc_ids)),8,len(set(dc_ids))==8)
    check("market_keys_unique",len(set(market_ids)),15,len(set(market_ids))==15)
    check("carbon_quota_status",cfg.get("carbon_quota"),"not_yet_generated",cfg.get("carbon_quota")=="not_yet_generated")
    # numeric sanity checks
    for name,rows,keys in [("dc_market",dcm,["road_distance_km","transport_cost_baseline"]),("supplier_dc",sdc,["road_distance_km","transport_cost_baseline"]),("market",markets,["mu_annual_tonnes","mu_daily_tonnes","sigma2_daily","v_i_baseline"]),("dc",dcs,["f_j_cny_per_year"]),("inventory",inv,["F_j_cny_per_order","g_j_cny_per_order","h_cny_per_tonne_year","L_j_days","alpha","z_alpha"]),("emission",em, ["hat_f_j_tco2e_per_year","hat_h_tco2e_per_tonne_year"])]:
        ok=True
        for r in rows:
            for k in keys:
                try: ok=ok and math.isfinite(float(r[k]))
                except Exception: ok=False
        check(name+"_numeric_finite",ok,True,ok)
    write_csv(out/"real_case_input_validation.csv",["check","observed","expected","status","notes"],checks)
    # Scenario input: only the eight intracity DC-market rows change distance.
    base={ (r["dc_id"],r["market_id"]):f(r,"road_distance_km") for r in dcm }
    scenario_rows=[]
    for k in DISTANCES:
        for r in dcm:
            key=(r["dc_id"],r["market_id"]); is_in=key in {(x["dc_id"],x["market_id"]) for x in intracity}
            dist=float(k) if is_in else base[key]
            scenario_rows.append({"scenario_distance_km":k,"dc_id":key[0],"market_id":key[1],"is_intracity_arc":str(is_in).lower(),"source_distance_km":base[key],"adapted_distance_km":dist,"distance_changed":str(abs(dist-base[key])>1e-9).lower(),"transport_rate_cny_per_tonne_km":cfg["transport_rate_cny_per_tonne_km"],"transport_emission_factor_tco2e_per_tonne_km":cfg["transport_emission_factor_tco2e_per_tonne_km"]})
    write_csv(out/"scenario_input_all.csv",list(scenario_rows[0]),scenario_rows)
    rows0=[r for r in scenario_rows if r["scenario_distance_km"]==0]; rows10=[r for r in scenario_rows if r["scenario_distance_km"]==10]
    diff=[]
    for x,y in zip(rows0,rows10):
        if x["distance_changed"]=="true" or y["distance_changed"]=="true":
            diff.append({"dc_id":x["dc_id"],"market_id":x["market_id"],"source_distance_km":x["source_distance_km"],"distance_at_0_km":x["adapted_distance_km"],"distance_at_10_km":y["adapted_distance_km"],"changed_only_intracity":str(x["is_intracity_arc"]=="true" and y["is_intracity_arc"]=="true").lower()})
    write_csv(out/"intracity_scenario_input_diff_0_vs_10.csv",["dc_id","market_id","source_distance_km","distance_at_0_km","distance_at_10_km","changed_only_intracity"],diff)
    for k in DISTANCES:
        sr=[r for r in scenario_rows if r["scenario_distance_km"]==k]
        summary={"scenario_distance_km":k,"num_suppliers":1,"num_candidate_dcs":len(dcs),"num_markets":len(markets),"num_supplier_dc_arcs":len(sdc),"num_dc_market_arcs":len(dcm),"num_intracity_arcs":len(intracity),"changed_arc_count":sum(r["distance_changed"]=="true" for r in sr),"carbon_quota":"not_yet_generated","solver_invoked":False,"ga_invoked":False,"bp_invoked":False,"cplex_invoked":False,"data_commit":DATA_COMMIT,"model_commit":MODEL_COMMIT}
        (out/f"scenario_{k:02d}km_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    (out/"adapter_metadata.json").write_text(json.dumps({"created_utc":datetime.now(timezone.utc).isoformat(),"data_dir":str(data),"data_commit":DATA_COMMIT,"model_commit":MODEL_COMMIT,"validate_only":a.validate_only,"solver_invoked":False,"config_sha256":{n:sha256(p) for n,p in files.items()}},indent=2)+"\n")
    print(json.dumps({"status":"PASS" if all(x["status"]=="PASS" for x in checks) and len(diff)==8 else "FAIL","checks":len(checks),"intracity_arcs":len(intracity),"diff_0_vs_10":len(diff),"output":str(out)},indent=2))
if __name__=="__main__": main()
