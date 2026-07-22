"""Failure decomposition, practical effect, and held-out policy validation."""
import csv, hashlib, html, json, math, os, sys
from collections import defaultdict

def rows(path):
    with open(path,encoding="utf-8-sig",newline="") as f:yield from csv.DictReader(f)
def write(path,fields,data):
    with open(path,"w",encoding="utf-8-sig",newline="") as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(data)
def esc(x):return html.escape(str(x))
def svg(title,subtitle,body,note=""):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="620" viewBox="0 0 1000 620"><rect width="100%" height="100%" fill="white"/><text x="45" y="38" font-family="sans-serif" font-size="22" font-weight="bold" fill="#16222e">{esc(title)}</text><text x="45" y="62" font-family="sans-serif" font-size="13" fill="#536273">{esc(subtitle)}</text>{body}<text x="45" y="602" font-family="sans-serif" font-size="11" fill="#667585">{esc(note)}</text></svg>'
def txt(x,y,s,size=12,anchor="middle",color="#25313c",bold=False,rotate=None):
    transform=f' transform="rotate({rotate} {x} {y})"' if rotate is not None else ""
    return f'<text x="{x:.1f}" y="{y:.1f}" font-family="sans-serif" font-size="{size}" text-anchor="{anchor}" fill="{color}" font-weight="{"bold" if bold else "normal"}"{transform}>{esc(s)}</text>'
def rect(x,y,w,h,c,stroke="none"):return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{c}" stroke="{stroke}"/>'
def line(x1,y1,x2,y2,c="#dce2e8",w=1):return f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{c}" stroke-width="{w}"/>'

def main(data_dir,out_dir):
    scenarios=list(rows(os.path.join(data_dir,"02_SCENARIOS.csv")));total=sum(float(r["probability"]) for r in scenarios);prob={r["scenario_id"]:float(r["probability"])/total for r in scenarios};group={r["scenario_id"]:r["scenario_group"] for r in scenarios}
    constraints={r["constraint_name"]:float(r["value"]) for r in rows(os.path.join(data_dir,"13_CONSTRAINTS.csv"))};short_limit=constraints["fail_short_rate_limit"];consecutive_limit=int(constraints["fail_consecutive_short_months"]);min_profit=constraints["min_profit_for_success"]
    run_rows=list(rows(os.path.join(out_dir,"90_OUTPUT_POLICY_SCENARIO.csv")));meta={r["run_id"]:r for r in run_rows};failure={};current=None;demand=shortage=0.0;streak=defaultdict(int);max_streak=0
    def finish():
        if current is not None:failure[current]=(shortage/demand if demand else 0.0,max_streak)
    for r in rows(os.path.join(out_dir,"91_OUTPUT_MONTHLY_RESOURCE.csv")):
        rid=r["run_id"]
        if rid!=current:
            finish();current=rid;demand=shortage=0.0;streak=defaultdict(int);max_streak=0
        d=float(r["demand_fte"]);s=float(r["short_fte"]);g=r["grade_id"];demand+=d;shortage+=s;streak[g]=streak[g]+1 if s>1e-9 else 0;max_streak=max(max_streak,streak[g])
    finish()
    agg=defaultdict(lambda:defaultdict(float));mismatch=0
    for rid,r in meta.items():
        pid=r["policy_id"];w=prob[r["scenario_id"]]/20;sr,ms=failure[rid];p=float(r["objective_value"])<min_profit-1e-9;s=sr>short_limit+1e-12;c=ms>=consecutive_limit;key=("P" if p else "")+("S" if s else "")+("C" if c else "") or "NONE";a=agg[pid];a["weight"]+=w;a["any"]+=w*(p or s or c);a["profit"]+=w*p;a["short_rate"]+=w*s;a["consecutive"]+=w*c;a[key]+=w;a["mean_shortage_rate"]+=w*sr
        if int(r["fail_flag"])!=int(p or s or c):mismatch+=1
    failure_rows=[]
    for pid in sorted(agg):
        a=agg[pid];w=a["weight"];failure_rows.append({"policy_id":pid,"reported_fail_rate":f'{a["any"]/w:.9f}',"profit_failure_rate":f'{a["profit"]/w:.9f}',"cumulative_shortage_failure_rate":f'{a["short_rate"]/w:.9f}',"consecutive_shortage_failure_rate":f'{a["consecutive"]/w:.9f}',"consecutive_only_rate":f'{a["C"]/w:.9f}',"shortage_only_rate":f'{a["S"]/w:.9f}',"profit_only_rate":f'{a["P"]/w:.9f}',"multiple_condition_rate":f'{sum(v for k,v in a.items() if len(k)>1 and set(k)<=set("PSC"))/w:.9f}',"mean_shortage_rate":f'{a["mean_shortage_rate"]/w:.9f}'})
    write(os.path.join(out_dir,"105_FAILURE_DECOMPOSITION.csv"),list(failure_rows[0]),failure_rows)
    threshold_rows=[]
    for threshold in range(2,7):
        for pid in sorted(agg):
            selected=[(rid,r) for rid,r in meta.items() if r["policy_id"]==pid];den=sum(prob[r["scenario_id"]]/20 for _,r in selected);rate=sum(prob[r["scenario_id"]]/20*(failure[rid][1]>=threshold) for rid,r in selected)/den
            threshold_rows.append({"consecutive_month_threshold":threshold,"policy_id":pid,"classified_failure_rate":f"{rate:.9f}"})
    write(os.path.join(out_dir,"109_FAILURE_THRESHOLD_SENSITIVITY.csv"),list(threshold_rows[0]),threshold_rows)

    comparison={r["policy_id"]:r for r in rows(os.path.join(out_dir,"100_POLICY_COMPARISON_VS_P0.csv"))};project=list(rows(os.path.join(out_dir,"102_PROJECT_POLICY_SUMMARY.csv")))
    def project_metric(pid,typ,field):return sum(float(r[field]) for r in project if r["policy_id"]==pid and r["project_type"]==typ)
    effect=float(comparison["P8"]["paired_mean_difference"]);relative=float(comparison["P8"]["relative_effect_percent"]);external_start=project_metric("P8","EXTERNAL","start_rate")-project_metric("P0","EXTERNAL","start_rate");internal_start=project_metric("P8","INTERNAL","start_rate")-project_metric("P0","INTERNAL","start_rate");gross=sum(float(r["avg_discounted_gross_revenue"]) for r in project if r["policy_id"]=="P8")-sum(float(r["avg_discounted_gross_revenue"]) for r in project if r["policy_id"]=="P0");residual=gross-effect;annuity=effect*.05/(1-(1.05)**-5)
    practical=[
        {"metric":"five_year_incremental_objective_pv","value":f"{effect:.6f}","unit":"KRW million","interpretation":"Maximum additional unmodelled implementation cost before P8 loses its P0 advantage"},
        {"metric":"relative_effect","value":f"{relative:.6f}","unit":"percent","interpretation":"P8 objective improvement relative to P0"},
        {"metric":"simple_annual_average","value":f"{effect/5:.6f}","unit":"KRW million/year","interpretation":"Five-year PV divided by five; not an annuity"},
        {"metric":"five_percent_equivalent_annual_value","value":f"{annuity:.6f}","unit":"KRW million/year","interpretation":"Five-year equivalent annual value at 5 percent"},
        {"metric":"additional_external_starts","value":f"{external_start:.6f}","unit":"projects/portfolio","interpretation":"Probability-weighted expected external-project starts"},
        {"metric":"change_internal_starts","value":f"{internal_start:.6f}","unit":"projects/portfolio","interpretation":"Probability-weighted expected internal-project starts"},
        {"metric":"incremental_discounted_gross_revenue","value":f"{gross:.6f}","unit":"KRW million","interpretation":"Candidate-project gross revenue without shared-cost allocation"},
        {"metric":"incremental_modelled_cost_and_penalty_burden","value":f"{residual:.6f}","unit":"KRW million","interpretation":"Gross-revenue difference less objective difference"},
        {"metric":"objective_gain_per_additional_external_start","value":f"{effect/external_start:.6f}","unit":"KRW million/project","interpretation":"Portfolio-level ratio; not a causal project unit profit"},
        {"metric":"paired_run_win_fraction","value":comparison["P8"]["run_level_win_fraction"],"unit":"ratio","interpretation":"Fraction of paired runs where P8 exceeds P0"}]
    write(os.path.join(out_dir,"106_PRACTICAL_SIGNIFICANCE.csv"),list(practical[0]),practical)

    values=defaultdict(lambda:defaultdict(list))
    for r in run_rows:values[r["policy_id"]][r["scenario_id"]].append(float(r["objective_value"]))
    def mean_for(pid,sids):
        denom=sum(prob[s] for s in sids);return sum(prob[s]*sum(values[pid][s])/len(values[pid][s]) for s in sids)/denom
    policies=sorted(values);all_sids=[r["scenario_id"] for r in scenarios];groups=sorted(set(group.values()));oos=[]
    for hold in groups:
        test=[s for s in all_sids if group[s]==hold];train=[s for s in all_sids if group[s]!=hold];train_means={p:mean_for(p,train) for p in policies};chosen=max(policies,key=lambda p:(train_means[p],p));test_means={p:mean_for(p,test) for p in policies};oracle=max(policies,key=lambda p:(test_means[p],p));rank=1+sum(v>test_means[chosen]+1e-9 for v in test_means.values());oos.append({"validation_type":"leave_one_scenario_group_out","holdout":hold,"training_selected_policy":chosen,"holdout_selected_policy_mean":f'{test_means[chosen]:.6f}',"holdout_rank":rank,"holdout_oracle_policy":oracle,"oracle_regret":f'{test_means[oracle]-test_means[chosen]:.6f}',"difference_vs_p0":f'{test_means[chosen]-test_means["P0"]:.6f}'})
    write(os.path.join(out_dir,"107_OOS_GROUP_VALIDATION.csv"),list(oos[0]),oos)
    positions=defaultdict(list)
    for g in groups:
        for i,s in enumerate(sorted(s for s in all_sids if group[s]==g)):positions[i].append(s)
    folds=[]
    for i,test in sorted(positions.items()):
        train=[s for s in all_sids if s not in test];tm={p:mean_for(p,train) for p in policies};chosen=max(policies,key=lambda p:(tm[p],p));hm={p:mean_for(p,test) for p in policies};oracle=max(policies,key=lambda p:(hm[p],p));rank=1+sum(v>hm[chosen]+1e-9 for v in hm.values());folds.append({"validation_type":"stratified_10_fold_scenario_holdout","fold":i+1,"holdout_scenario_count":len(test),"training_selected_policy":chosen,"holdout_selected_policy_mean":f'{hm[chosen]:.6f}',"holdout_rank":rank,"holdout_oracle_policy":oracle,"oracle_regret":f'{hm[oracle]-hm[chosen]:.6f}',"difference_vs_p0":f'{hm[chosen]-hm["P0"]:.6f}'})
    write(os.path.join(out_dir,"108_OOS_STRATIFIED_FOLDS.csv"),list(folds[0]),folds)

    b="";cats=[("Consecutive", "consecutive_shortage_failure_rate","#D24A4A"),("Cumulative shortage","cumulative_shortage_failure_rate","#F28E2B"),("Profit","profit_failure_rate","#4E79A7")]
    for i,r in enumerate(failure_rows):
        x=95+i*86;b+=txt(x,555,r["policy_id"],12,bold=True)
        for j,(label,key,c) in enumerate(cats):
            v=float(r[key]);bw=18;xx=x-28+j*22;b+=rect(xx,520-v*410,bw,v*410,c)
    for j,(label,key,c) in enumerate(cats):b+=rect(570+j*130,82,14,14,c)+txt(590+j*130,94,label,10,"start")
    for v in (0,.25,.5,.75,1):b+=line(65,520-v*410,955,520-v*410)+txt(57,524-v*410,f"{v:.2f}",10,"end")
    b+=txt(510,580,"X: Policy (P0-P9)",12,bold=True)+txt(25,315,"Y: Inclusive failure-component rate (0-1)",11,rotate=-90)
    with open(os.path.join(out_dir,"11_FAILURE_DECOMPOSITION.svg"),"w",encoding="utf-8") as f:f.write(svg("Why runs are classified as failures","X = policy; Y = inclusive component rate (ratio); components may overlap",b,"Failure is triggered by profit, cumulative shortage, or three consecutive shortage months."))

    items=[("Gross revenue",gross,"#4E79A7"),("Cost/penalty burden",-residual,"#D24A4A"),("Net objective",effect,"#0B8F55")];mx=max(abs(v) for _,v,_ in items);b=line(120,480,900,480,"#7c8996",2)
    for i,(label,v,c) in enumerate(items):
        x=190+i*300;h=330*abs(v)/mx;y=480-h if v>=0 else 480;b+=rect(x-70,y,140,h,c)+txt(x,520,label,12,bold=True)+txt(x,y-10 if v>=0 else y+h+18,f"{v:+,.1f}",13,color=c,bold=True)
    b+=txt(490,565,"X: Effect component",12,bold=True)+txt(28,300,"Y: P8 - P0 five-year discounted difference (KRW million)",11,rotate=-90)
    with open(os.path.join(out_dir,"12_PRACTICAL_EFFECT_WATERFALL.svg"),"w",encoding="utf-8") as f:f.write(svg("Practical meaning of the P8 advantage","X = effect component; Y = discounted five-year difference versus P0 (KRW million)",b,"Gross revenue less modelled cost and penalty burden equals the incremental objective."))

    b="";combined=oos+folds
    for i,r in enumerate(combined):
        y=95+i*27;v=float(r["difference_vs_p0"]);w=360*abs(v)/max(abs(float(x["difference_vs_p0"])) for x in combined);c="#0B8F55" if v>=0 else "#D24A4A";label=r["holdout"] if "holdout" in r else f'Fold {r["fold"]}';b+=txt(205,y+4,label,9,"end")+rect(500 if v>=0 else 500-w,y-7,w,14,c)+txt(510+w if v>=0 else 490-w,y+4,f"{v:+,.0f}",9,"start" if v>=0 else "end",c)
    b+=line(500,78,500,545,"#65717d",2)+txt(500,565,"X: Held-out selected-policy objective difference vs P0 (KRW million)",11,bold=True)+txt(35,310,"Y: Held-out group / stratified fold",11,rotate=-90)
    with open(os.path.join(out_dir,"13_HELD_OUT_VALIDATION.svg"),"w",encoding="utf-8") as f:f.write(svg("Held-out policy-selection validation","X = selected-policy difference vs P0 (KRW million); Y = unseen holdout",b,"Top 7 rows: held-out groups; bottom 10 rows: stratified scenario folds."))

    b="";colors={"P0":"#4E79A7","P7":"#F28E2B","P8":"#0B8F55"}
    for v in (0,.25,.5,.75,1):b+=line(90,520-v*400,930,520-v*400)+txt(78,524-v*400,f"{v:.2f}",10,"end")
    for pid in ("P0","P7","P8"):
        rr=[r for r in threshold_rows if r["policy_id"]==pid];pts=[]
        for r in rr:
            x=150+(int(r["consecutive_month_threshold"])-2)*180;y=520-float(r["classified_failure_rate"])*400;pts.append((x,y));b+=f'<circle cx="{x}" cy="{y:.1f}" r="6" fill="{colors[pid]}"/>'+txt(x,y-11,f'{float(r["classified_failure_rate"]):.2f}',10,color=colors[pid])
        b+=f'<polyline points="{" ".join(f"{x},{y:.1f}" for x,y in pts)}" fill="none" stroke="{colors[pid]}" stroke-width="3"/>'
    for threshold in range(2,7):b+=txt(150+(threshold-2)*180,548,str(threshold),12,bold=True)
    for i,pid in enumerate(("P0","P7","P8")):b+=rect(650+i*80,82,14,14,colors[pid])+txt(670+i*80,94,pid,11,"start",colors[pid],True)
    b+=txt(510,575,"X: Consecutive-shortage threshold (months)",12,bold=True)+txt(28,315,"Y: Classified failure rate (0-1)",11,rotate=-90)
    with open(os.path.join(out_dir,"14_FAILURE_THRESHOLD_SENSITIVITY.svg"),"w",encoding="utf-8") as f:f.write(svg("Sensitivity of failure classification","X = consecutive-shortage threshold (months); Y = classified failure rate (ratio)",b,"This changes the diagnostic classification only; simulated decisions and objective values are unchanged."))

    outputs=["105_FAILURE_DECOMPOSITION.csv","106_PRACTICAL_SIGNIFICANCE.csv","107_OOS_GROUP_VALIDATION.csv","108_OOS_STRATIFIED_FOLDS.csv","109_FAILURE_THRESHOLD_SENSITIVITY.csv","11_FAILURE_DECOMPOSITION.svg","12_PRACTICAL_EFFECT_WATERFALL.svg","13_HELD_OUT_VALIDATION.svg","14_FAILURE_THRESHOLD_SENSITIVITY.svg"]
    audit={"source_runs":len(run_rows),"reconstructed_fail_mismatches":mismatch,"valid_failure_reconstruction":mismatch==0,"group_holdouts":len(oos),"stratified_folds":len(folds),"outputs":{}}
    for name in outputs:
        with open(os.path.join(out_dir,name),"rb") as f:audit["outputs"][name]=hashlib.sha256(f.read()).hexdigest()
    with open(os.path.join(out_dir,"EXTENDED_ANALYSIS_AUDIT.json"),"w",encoding="utf-8") as f:json.dump(audit,f,indent=2)
    print(json.dumps(audit,indent=2))

if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: analyze_failure_practical_oos.py DATA_DIR RESULT_DIR")
    main(sys.argv[1],sys.argv[2])
