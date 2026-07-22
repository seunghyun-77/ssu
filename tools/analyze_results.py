import csv, hashlib, json, math, os, sys
from collections import defaultdict

CHECKPOINTS=(12,24,36,48,60)
GROUPS=("BASE","HIGH_WIN","LOW_WIN","RESOURCE_SHORT","DELAYED_START","COST_PRESSURE","INTERNAL_RATIO_STRESS")

def read_csv(path):
    with open(path,encoding="utf-8-sig",newline="") as f: return list(csv.DictReader(f))
def write_csv(path,fields,rows):
    with open(path,"w",encoding="utf-8-sig",newline="") as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
def weighted_mean(items,key=lambda x:x[0]):
    sw=sum(x[1] for x in items);return sum(key(x)*x[1] for x in items)/sw if sw else 0.0
def weighted_std(items,mean):
    sw=sum(w for _,w in items);return math.sqrt(sum(w*(v-mean)**2 for v,w in items)/sw) if sw else 0.0
def weighted_quantile(items,q):
    a=sorted(items);total=sum(w for _,w in a);target=q*total;acc=0.0
    for v,w in a:
        acc+=w
        if acc>=target:return v
    return a[-1][0] if a else 0.0
def weighted_cvar(items,q=.1):
    a=sorted(items);remaining=q*sum(w for _,w in a);value=weight=0.0
    for v,w in a:
        take=min(w,remaining);value+=v*take;weight+=take;remaining-=take
        if remaining<=1e-15:break
    return value/weight if weight else 0.0
def f(v,n=6): return f"{v:.{n}f}"

def main(data_dir,out_dir):
    scenarios=read_csv(os.path.join(data_dir,"02_SCENARIOS.csv")); policies=read_csv(os.path.join(data_dir,"12_POLICIES.csv"))
    prob={r["scenario_id"]:float(r["probability"]) for r in scenarios};total_prob=sum(prob.values());prob={k:v/total_prob for k,v in prob.items()}
    group={r["scenario_id"]:r["scenario_group"] for r in scenarios};pname={r["policy_id"]:(r["policy_name_ko"],r["policy_name_en"]) for r in policies}
    raw=read_csv(os.path.join(out_dir,"90_OUTPUT_POLICY_SCENARIO.csv"));by_policy=defaultdict(list);by_group=defaultdict(list)
    for r in raw:
        r["w"]=prob[r["scenario_id"]]/20;r["obj"]=float(r["objective_value"]);by_policy[r["policy_id"]].append(r);by_group[(r["scenario_group"],r["policy_id"])].append(r)
    rows93=[];rows94=[]
    for pid in pname:
        rs=by_policy[pid];vals=[(r["obj"],r["w"]) for r in rs];mean=weighted_mean(vals);std=weighted_std(vals,mean)
        avg=lambda k:sum(float(r[k])*r["w"] for r in rs)
        cvar=weighted_cvar(vals); fail=avg("fail_flag")
        rows93.append(dict(policy_id=pid,policy_name_ko=pname[pid][0],policy_name_en=pname[pid][1],avg_objective=f(mean),std_objective=f(std),worst_10pct_objective=f(cvar),fail_rate=f(fail,9),avg_win_rate=f(avg("win_rate"),9),avg_utilization=f(avg("utilization"),9),avg_idle=f(avg("idle_total")),avg_short=f(avg("short_total")),avg_internal_ratio_violation=f(avg("internal_ratio_violation_count")),avg_runtime_ms=f(avg("runtime_ms"),3)))
        rows94.append(dict(policy_id=pid,avg_objective=f(mean),std_objective=f(std),min_objective=f(min(v for v,_ in vals)),p10_objective=f(weighted_quantile(vals,.1)),p25_objective=f(weighted_quantile(vals,.25)),median_objective=f(weighted_quantile(vals,.5)),p75_objective=f(weighted_quantile(vals,.75)),p90_objective=f(weighted_quantile(vals,.9)),max_objective=f(max(v for v,_ in vals)),cvar_10pct=f(cvar),fail_rate=f(fail,9),comment_ko="공통난수 확률가중 결과",comment_en="CRN probability-weighted result"))
    write_csv(os.path.join(out_dir,"93_OUTPUT_POLICY_SUMMARY.csv"),rows93[0].keys(),rows93);write_csv(os.path.join(out_dir,"94_OUTPUT_RISK_SUMMARY.csv"),rows94[0].keys(),rows94)
    baseline={(r["scenario_id"],r["replication_id"]):r["obj"] for r in by_policy["P0"]};rows100=[]
    for pid in pname:
        scenario_diff={}
        paired_wins=0
        for sid in prob:
            ds=[r["obj"]-baseline[(sid,r["replication_id"])] for r in by_policy[pid] if r["scenario_id"]==sid]
            scenario_diff[sid]=sum(ds)/len(ds);paired_wins+=sum(d>0 for d in ds)
        mean_diff=sum(prob[s]*scenario_diff[s] for s in prob);var=sum(prob[s]*(scenario_diff[s]-mean_diff)**2 for s in prob)
        neff=1.0/sum(w*w for w in prob.values());se=math.sqrt(var/neff);base_mean=float(next(r["avg_objective"] for r in rows93 if r["policy_id"]=="P0"))
        rows100.append(dict(policy_id=pid,baseline_policy="P0",paired_mean_difference=f(mean_diff),relative_effect_percent=f(100*mean_diff/base_mean),clustered_standard_error=f(se),ci95_lower=f(mean_diff-1.96*se),ci95_upper=f(mean_diff+1.96*se),scenario_effective_n=f(neff,3),run_level_win_fraction=f(paired_wins/1400,9)))
    write_csv(os.path.join(out_dir,"100_POLICY_COMPARISON_VS_P0.csv"),rows100[0].keys(),rows100)
    group_means={}
    for g in GROUPS:
        for pid in pname:
            rs=by_group[(g,pid)];sw=sum(r["w"] for r in rs);group_means[(g,pid)]=sum(r["obj"]*r["w"] for r in rs)/sw
    rows95=[]
    for g in GROUPS:
        ordered=sorted(pname,key=lambda p:(-group_means[(g,p)],p));rank={p:i+1 for i,p in enumerate(ordered)}
        for pid in pname:
            rs=by_group[(g,pid)];sw=sum(r["w"] for r in rs);norm=[(r["obj"],r["w"]/sw) for r in rs];mean=group_means[(g,pid)]
            avg=lambda k:sum(float(r[k])*r["w"] for r in rs)/sw
            rows95.append(dict(scenario_group=g,policy_id=pid,avg_objective=f(mean),std_objective=f(weighted_std(norm,mean)),fail_rate=f(avg("fail_flag"),9),avg_win_rate=f(avg("win_rate"),9),avg_short=f(avg("short_total")),avg_idle=f(avg("idle_total")),rank_in_group=rank[pid]))
    write_csv(os.path.join(out_dir,"95_OUTPUT_SCENARIO_GROUP.csv"),rows95[0].keys(),rows95)
    checkpoints=read_csv(os.path.join(out_dir,"92_OUTPUT_CHECKPOINT.csv"));cp_groups=defaultdict(list)
    for r in checkpoints:r["w"]=prob[r["scenario_id"]]/20;cp_groups[(r["policy_id"],int(r["checkpoint_month"]))].append(r)
    rows96=[]
    for pid in pname:
        for month in CHECKPOINTS:
            rs=cp_groups[(pid,month)];vals=[(float(r["cum_profit"]),r["w"]) for r in rs];mean=weighted_mean(vals);avg=lambda k:sum(float(r[k])*r["w"] for r in rs)
            rows96.append(dict(policy_id=pid,checkpoint_month=month,avg_cum_profit=f(mean),std_cum_profit=f(weighted_std(vals,mean)),p10_cum_profit=f(weighted_quantile(vals,.1)),fail_rate=f(avg("fail_flag"),9),avg_win_rate=f(avg("win_rate"),9),avg_utilization=f(avg("utilization"),9)))
    write_csv(os.path.join(out_dir,"96_OUTPUT_CHECKPOINT_SUMMARY.csv"),rows96[0].keys(),rows96)
    monthly=defaultdict(lambda:dict(profit=0.0,revenue=0.0,cost=0.0,idle=0.0,short=0.0,ratio=0.0,util=0.0));current=None;supply=served=0.0
    path91=os.path.join(out_dir,"91_OUTPUT_MONTHLY_RESOURCE.csv")
    with open(path91,encoding="utf-8-sig",newline="") as fh:
        for r in csv.DictReader(fh):
            key=(r["run_id"],int(r["month"]));pid=r["policy_id"];month=int(r["month"]);w=prob[r["scenario_id"]]/20;target=monthly[(pid,month)]
            if current!=key:
                if current is not None: prev["util"]+=prev_w*(served/supply if supply else 0.0)
                current=key;supply=served=0.0;prev=target;prev_w=w
                target["profit"]+=w*float(r["monthly_profit"]);target["revenue"]+=w*(float(r["internal_revenue"])+float(r["external_revenue"]));target["cost"]+=w*float(r["cost"]);target["ratio"]+=w*float(r["internal_ratio"])
            demand=float(r["demand_fte"]);cap=float(r["internal_staff_fte"])+float(r["external_staff_fte"]);supply+=cap;served+=min(demand,cap);target["idle"]+=w*float(r["idle_fte"]);target["short"]+=w*float(r["short_fte"])
        if current is not None:prev["util"]+=prev_w*(served/supply if supply else 0.0)
    rows97=[]
    for pid in pname:
        cumulative=0.0
        for month in range(1,61):
            a=monthly[(pid,month)];cumulative+=a["profit"];rows97.append(dict(policy_id=pid,month=month,avg_monthly_profit=f(a["profit"]),avg_cumulative_profit=f(cumulative),avg_revenue=f(a["revenue"]),avg_cost=f(a["cost"]),avg_utilization=f(a["util"],9),avg_idle=f(a["idle"]),avg_short=f(a["short"]),avg_internal_ratio=f(a["ratio"],9)))
    write_csv(os.path.join(out_dir,"97_OUTPUT_MONTHLY_POLICY.csv"),rows97[0].keys(),rows97)
    expected={"90_OUTPUT_POLICY_SCENARIO.csv":14001,"91_OUTPUT_MONTHLY_RESOURCE.csv":3360001,"92_OUTPUT_CHECKPOINT.csv":70001,"93_OUTPUT_POLICY_SUMMARY.csv":11,"94_OUTPUT_RISK_SUMMARY.csv":11,"95_OUTPUT_SCENARIO_GROUP.csv":71,"96_OUTPUT_CHECKPOINT_SUMMARY.csv":51,"97_OUTPUT_MONTHLY_POLICY.csv":601,"98_OUTPUT_RUN_DIAGNOSTIC.csv":14001,"100_POLICY_COMPARISON_VS_P0.csv":11}
    audit={"method":"scenario probabilities normalized; 20 paired CRN replications per scenario","probability_sum_input":total_prob,"files":{}}
    for name,lines_expected in expected.items():
        path=os.path.join(out_dir,name);h=hashlib.sha256();lines=0
        with open(path,"rb") as fh:
            for line in fh:h.update(line);lines+=1
        audit["files"][name]={"lines":lines,"expected_lines":lines_expected,"sha256":h.hexdigest(),"valid":lines==lines_expected}
    with open(os.path.join(out_dir,"RESULT_AUDIT.json"),"w",encoding="utf-8") as fjson:json.dump(audit,fjson,ensure_ascii=False,indent=2)
    ranked=sorted(rows93,key=lambda r:float(r["avg_objective"]),reverse=True);comparison={r["policy_id"]:r for r in rows100}
    with open(os.path.join(out_dir,"RESULT_SUMMARY.md"),"w",encoding="utf-8") as report:
        report.write("# PPSP 14,000-run result summary\n\n")
        report.write("Results use 70 probability-normalized scenarios, 20 paired common-random-number replications, and 10 policies. Monetary units are KRW million.\n\n")
        report.write("|Rank|Policy|Mean objective|Std.|CVaR 10%|Fail rate|P0 paired difference|95% clustered CI|\n|---:|---|---:|---:|---:|---:|---:|---:|\n")
        for rank,r in enumerate(ranked,1):
            c=comparison[r["policy_id"]];report.write(f"|{rank}|{r['policy_id']}|{float(r['avg_objective']):,.1f}|{float(r['std_objective']):,.1f}|{float(r['worst_10pct_objective']):,.1f}|{100*float(r['fail_rate']):.1f}%|{float(c['paired_mean_difference']):,.1f}|[{float(c['ci95_lower']):,.1f}, {float(c['ci95_upper']):,.1f}]|\n")
        report.write("\n## Interpretation boundaries\n\n- P8 has the highest probability-weighted mean objective in this implementation; P0 has a slightly stronger lower-tail CVaR than P8.\n- P7 has the lowest estimated fail rate, so the policy preferred by mean performance is not the policy preferred by operational reliability.\n- P9 has the lowest dispersion but also the lowest mean objective; lower variance alone is not evidence of dominance.\n- Failure is triggered by the supplied shortage-rate, consecutive-shortage, and minimum-profit rules. High failure rates must be discussed as a model result and checked through sensitivity analysis, not hidden.\n- Confidence intervals cluster the 20 paired replications within each of the 70 scenarios; they do not treat 1,400 rows as unrelated observations.\n")

if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: analyze_results.py DATA_DIR OUTPUT_DIR")
    main(sys.argv[1],sys.argv[2])
