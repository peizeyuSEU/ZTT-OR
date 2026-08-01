#!/usr/bin/env python3
import csv,datetime,subprocess,sys
from pathlib import Path
def main():
    if len(sys.argv)!=2: raise SystemExit('usage: a3_snapshot.py BATCH_DIR')
    b=Path(sys.argv[1]); rows=list(csv.DictReader((b/'A3_RUN_QUEUE.csv').open())); now=datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    done=[r for r in rows if r['status']=='COMPLETED']; bad=[r for r in rows if r['status'] in ('FAILED','INTERRUPTED')]
    nxt=next((r for r in rows if r['status']=='PENDING'),None)
    md=b/'progress'/f'A3_PROGRESS_SNAPSHOT_{now}.md'; cp=b/'progress'/f'A3_PROGRESS_SNAPSHOT_{now}.csv'
    text=f"# A3 progress snapshot\n\n- snapshot_time: {datetime.datetime.now().astimezone().isoformat()}\n- completed: {len(done)}/100\n- failed_or_interrupted: {len(bad)}\n- next_task: {nxt['size_id']} seed {nxt['random_seed']}\n" if nxt else f"# A3 progress snapshot\n\n- completed: {len(done)}/100\n- failed_or_interrupted: {len(bad)}\n- next_task: none\n"
    md.write_text(text)
    with cp.open('w',newline='') as f:
        fields=['order_id','size_id','random_seed','status','started_at','finished_at','output_directory']
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows({k:r.get(k,'') for k in fields} for r in rows)
    print(md); print(cp)
if __name__=='__main__': main()
