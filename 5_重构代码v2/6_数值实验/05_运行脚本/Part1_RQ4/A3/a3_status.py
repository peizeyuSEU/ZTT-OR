#!/usr/bin/env python3
import csv,json,sys,subprocess,datetime
from pathlib import Path
def main():
    if len(sys.argv)!=2: raise SystemExit("usage: a3_status.py BATCH_DIR")
    b=Path(sys.argv[1]); rows=list(csv.DictReader((b/"A3_RUN_QUEUE.csv").open()))
    counts={s:sum(r["status"]==s for r in rows) for s in ["PENDING","RUNNING","COMPLETED","FAILED","INTERRUPTED"]}
    print(f"batch_directory: {b}"); print(f"batch_status: {json.loads((b/'A3_BATCH_STATE.json').read_text()).get('batch_status','UNKNOWN')}")
    print(f"controller_running: {(b/'RUNNING.lock').exists()}")
    for k,v in counts.items(): print(f"{k.lower()}_count: {v}")
    print(f"estimated_progress_percent: {counts['COMPLETED']/100*100:.1f}")
    active=next((r for r in rows if r["status"]=="RUNNING"),None); nxt=next((r for r in rows if r["status"]=="PENDING"),None)
    if active: print(f"current_size: {active['size_id']}\ncurrent_seed: {active['random_seed']}\ncurrent_started_at: {active['started_at']}")
    if nxt: print(f"next_pending_size: {nxt['size_id']}\nnext_pending_seed: {nxt['random_seed']}")
    by={}
    for r in rows: by.setdefault(r['size_id'],[0,10]); by[r['size_id']][0]+=r['status']=='COMPLETED'
    print("completed_by_size: "+", ".join(f"{k}={v[0]}/10" for k,v in by.items()))
if __name__=="__main__": main()
