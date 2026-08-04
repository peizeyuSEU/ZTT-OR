import csv,sys
from pathlib import Path
R=Path(sys.argv[1]); rs=[]
for d in [0,5,10,15,20]:
 for s in [101,202,303,404,505,606,707,808,909,1010]:
  p=R/f'D{d:02d}'/(f'seed_{s}_attempt_03' if d==10 and s==101 else f'seed_{s}')
  r=next(csv.DictReader((p/'result.csv').open()));r.update(distance_km=d,random_seed=s);rs.append(r)
with (R/'P5_NETWORK_STABILITY_BY_DISTANCE.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['distance_km','open_dc_ids','frequency','share'])
 for d in [0,5,10,15,20]:
  sub=[r for r in rs if int(r['distance_km'])==d]; counts={r['open_dc_ids']:0 for r in sub}
  for r in sub:counts[r['open_dc_ids']]+=1
  for k,v in sorted(counts.items(),key=lambda x:(-x[1],x[0])):w.writerow([d,k,v,v/len(sub)])
with (R/'P5_MARKET_ASSIGNMENT_STABILITY.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['distance_km','served_market_ids','frequency','share'])
 for d in [0,5,10,15,20]:
  sub=[r for r in rs if int(r['distance_km'])==d]; counts={r['served_market_ids']:0 for r in sub}
  for r in sub:counts[r['served_market_ids']]+=1
  for k,v in sorted(counts.items(),key=lambda x:(-x[1],x[0])):w.writerow([d,k,v,v/len(sub)])
with (R/'P5_W_STABILITY_BY_DISTANCE.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['distance_km','mean_w_open_dcs','mean_demand_weighted_w','mean_max_w','mean_min_w'])
 for d in [0,5,10,15,20]:
  sub=[r for r in rs if int(r['distance_km'])==d];w.writerow([d]+[sum(float(r[k]) for r in sub)/len(sub) for k in ['mean_w_open_dcs','demand_weighted_mean_w','max_w','min_w']])
with (R/'P5_CANONICAL_BEST_FOUND_RUNS.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['distance_km','random_seed','reported_total_profit','inner_status','outer_outcome','output_key'])
 for d in [0,5,10,15,20]:
  sub=[r for r in rs if int(r['distance_km'])==d];best=sorted(sub,key=lambda r:(-float(r['reported_total_profit']),int(r['random_seed'])))[0];w.writerow([d,best['random_seed'],best['reported_total_profit'],best['final_inner_bp_status'],best['outer_status'],f'D{d:02d}/seed_{best["random_seed"]}'])
