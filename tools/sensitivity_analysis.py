import csv,hashlib,json,math,os,sys
from collections import defaultdict

RATES=(0.00,0.03,0.05,0.07);PENALTIES=(0.5,1.0,2.0)
def rows(path):
    with open(path,encoding="utf-8-sig",newline="") as f:yield from csv.DictReader(f)
def qtile(values,q):
    values=sorted(values);p=q*(len(values)-1);lo=int(p);hi=min(lo+1,len(values)-1);return values[lo]*(hi-p)+values[hi]*(p-lo)

def main(data_dir,out_dir):
    scenarios=list(rows(os.path.join(data_dir,"02_SCENARIOS.csv")));total=sum(float(x["probability"]) for x in scenarios)
    prob={x["scenario_id"]:float(x["probability"])/total for x in scenarios};limit=float(next(x["value"] for x in rows(os.path.join(data_dir,"13_CONSTRAINTS.csv")) if x["constraint_name"]=="internal_ratio_limit"))
    runs={};meta={}
    for r in rows(os.path.join(out_dir,"91_OUTPUT_MONTHLY_RESOURCE.csv")):
        if r["grade_id"]!="G1":continue
        rid=r["run_id"];month=int(r["month"]);entry=runs.setdefault(rid,{"profit":[0.0]*60,"internal":[0.0]*60,"total":[0.0]*60})
        entry["profit"][month-1]=float(r["monthly_profit"]);entry["internal"][month-1]=float(r["internal_revenue"]);entry["total"][month-1]=float(r["internal_revenue"])+float(r["external_revenue"]);meta[rid]=(r["policy_id"],r["scenario_id"])
    output=[]
    for rate in RATES:
        beta=[(1+rate)**(-m/12) for m in range(60)]
        for penalty in PENALTIES:
            by_policy=defaultdict(list)
            for rid,x in runs.items():
                value=sum(beta[m]*x["profit"][m] for m in range(60))
                for year in range(5):
                    a=year*12;b=a+12;excess=max(sum(x["internal"][a:b])-limit*sum(x["total"][a:b]),0.0);value-=beta[b-1]*penalty*excess
                pid,sid=meta[rid];by_policy[pid].append((value,prob[sid]/20))
            means={p:sum(v*w for v,w in vals) for p,vals in by_policy.items()};ranking={p:i+1 for i,p in enumerate(sorted(means,key=lambda x:(-means[x],x)))}
            for pid,vals in sorted(by_policy.items()):
                mean=means[pid];std=math.sqrt(sum(w*(v-mean)**2 for v,w in vals));raw=[v for v,_ in vals];p10=qtile(raw,.1);tail=[v for v in raw if v<=p10]
                output.append(dict(annual_discount_rate=f"{rate:.2f}",internal_ratio_penalty=f"{penalty:.1f}",policy_id=pid,avg_objective=f"{mean:.6f}",std_objective=f"{std:.6f}",p10_objective=f"{p10:.6f}",cvar_10pct=f"{sum(tail)/len(tail):.6f}",rank=ranking[pid],analysis_type="fixed-decision evaluation sensitivity"))
    path=os.path.join(out_dir,"101_SENSITIVITY_SUMMARY.csv")
    with open(path,"w",encoding="utf-8-sig",newline="") as f:w=csv.DictWriter(f,fieldnames=output[0]);w.writeheader();w.writerows(output)
    winners=[]
    for rate in RATES:
        for penalty in PENALTIES:
            subset=[r for r in output if float(r["annual_discount_rate"])==rate and float(r["internal_ratio_penalty"])==penalty];winner=min(subset,key=lambda r:int(r["rank"]));winners.append((rate,penalty,winner["policy_id"],float(winner["avg_objective"])))
    with open(os.path.join(out_dir,"SENSITIVITY_SUMMARY.md"),"w",encoding="utf-8") as f:
        f.write("# Discount and internal-ratio penalty sensitivity\n\nThis is a fixed-decision evaluation sensitivity analysis. It holds each policy's CRN decisions constant and changes only discounted evaluation and the year-end internal-ratio excess penalty.\n\n|Annual rate|Penalty|Top policy|Mean objective|\n|---:|---:|---|---:|\n")
        for rate,penalty,pid,value in winners:f.write(f"|{rate:.0%}|{penalty:.1f}|{pid}|{value:,.1f}|\n")
    digest=hashlib.sha256()
    with open(path,"rb") as f:
        for chunk in iter(lambda:f.read(1<<20),b""):digest.update(chunk)
    with open(os.path.join(out_dir,"SENSITIVITY_AUDIT.json"),"w",encoding="utf-8") as f:
        json.dump({"analysis_type":"fixed-decision evaluation sensitivity","discount_rates":RATES,"penalties":PENALTIES,"rows":len(output),"sha256":digest.hexdigest()},f,indent=2)

if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: sensitivity_analysis.py DATA_DIR OUTPUT_DIR")
    main(sys.argv[1],sys.argv[2])
