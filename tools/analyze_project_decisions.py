"""Stream project decision traces into thesis tables, SVGs, and an audit."""
import csv, hashlib, html, json, os, sys
from collections import defaultdict

def rows(path):
    with open(path,encoding="utf-8-sig",newline="") as f:yield from csv.DictReader(f)
def write(path, fields, data):
    with open(path,"w",encoding="utf-8-sig",newline="") as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(data)
def e(x):return html.escape(str(x))
def svg(title,subtitle,body):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="620" viewBox="0 0 1000 620"><rect width="100%" height="100%" fill="white"/><text x="45" y="38" font-family="sans-serif" font-size="22" font-weight="bold" fill="#16222e">{e(title)}</text><text x="45" y="62" font-family="sans-serif" font-size="13" fill="#536273">{e(subtitle)}</text>{body}</svg>'
def txt(x,y,s,size=12,anchor="middle",color="#25313c",bold=False):return f'<text x="{x:.1f}" y="{y:.1f}" font-family="sans-serif" font-size="{size}" text-anchor="{anchor}" fill="{color}" font-weight="{"bold" if bold else "normal"}">{e(s)}</text>'
def rect(x,y,w,h,c,stroke="none"):return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{c}" stroke="{stroke}"/>'
def line(x1,y1,x2,y2,c="#dce2e8",w=1):return f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{c}" stroke-width="{w}"/>'

def main(data_dir,out_dir):
    prob={r["scenario_id"]:float(r["probability"]) for r in rows(os.path.join(data_dir,"02_SCENARIOS.csv"))};total=sum(prob.values());prob={k:v/total for k,v in prob.items()}
    project=defaultdict(lambda:defaultdict(float));group=defaultdict(lambda:defaultdict(float));policy_type=defaultdict(lambda:defaultdict(float));project_types={};seen_potential={};viol=defaultdict(int);count=0
    source=os.path.join(out_dir,"99_OUTPUT_PROJECT_DECISION.csv")
    for r in rows(source):
        count+=1;w=prob[r["scenario_id"]]/20;pid=r["policy_id"];prj=r["project_id"];typ=r["project_type"];project_types[prj]=typ;grp=r["scenario_group"];proposal=int(r["proposal_month"]);start=int(r["start_month"]);observed=int(r["observed_month"]);win=int(r["potential_win_result"]);gross=float(r["discounted_gross_revenue"]);resource=float(r["resource_demand_fte_months"])
        if typ=="INTERNAL" and proposal:viol["internal_proposal"]+=1
        if start and start<observed:viol["start_before_observation"]+=1
        if typ=="EXTERNAL" and start and (not proposal or win!=1):viol["external_start_without_proposal_win"]+=1
        if not start and (abs(gross)>1e-9 or abs(resource)>1e-9):viol["contribution_without_start"]+=1
        potential_key=(r["scenario_id"],r["replication_id"],prj)
        if typ=="EXTERNAL":
            if potential_key in seen_potential and seen_potential[potential_key]!=win:viol["policy_dependent_potential_win"]+=1
            seen_potential[potential_key]=win
        for key,target in (((pid,prj),project),((grp,pid,typ),group),((pid,typ),policy_type)):
            a=target[key];a["weight"]+=w;a["proposed"]+=w*(proposal>0);a["potential_win"]+=w*(win==1);a["started"]+=w*(start>0);a["start_month"]+=w*start*(start>0);a["gross"]+=w*gross;a["resource"]+=w*resource
    summary=[]
    for (pid,prj),a in sorted(project.items()):
        typ=project_types[prj]
        summary.append({"policy_id":pid,"project_id":prj,"project_type":typ,"proposal_rate":f'{a["proposed"]/a["weight"]:.9f}',"potential_win_rate":f'{a["potential_win"]/a["weight"]:.9f}' if typ=="EXTERNAL" else "","start_rate":f'{a["started"]/a["weight"]:.9f}',"avg_start_month_if_started":f'{a["start_month"]/a["started"]:.6f}' if a["started"] else "","avg_discounted_gross_revenue":f'{a["gross"]/a["weight"]:.6f}',"avg_resource_demand_fte_months":f'{a["resource"]/a["weight"]:.6f}'})
    fields=list(summary[0]);write(os.path.join(out_dir,"102_PROJECT_POLICY_SUMMARY.csv"),fields,summary)
    gs=[]
    for (grp,pid,typ),a in sorted(group.items()):gs.append({"scenario_group":grp,"policy_id":pid,"project_type":typ,"proposal_rate":f'{a["proposed"]/a["weight"]:.9f}',"start_rate":f'{a["started"]/a["weight"]:.9f}',"avg_start_month_if_started":f'{a["start_month"]/a["started"]:.6f}' if a["started"] else "","avg_discounted_gross_revenue":f'{a["gross"]/a["weight"]:.6f}',"avg_resource_demand_fte_months":f'{a["resource"]/a["weight"]:.6f}'})
    write(os.path.join(out_dir,"103_PROJECT_SCENARIO_SUMMARY.csv"),list(gs[0]),gs)
    by={(r["policy_id"],r["project_id"]):r for r in summary};cmp=[]
    for prj in sorted({r["project_id"] for r in summary}):
        a=by[("P0",prj)];b=by[("P8",prj)];cmp.append({"project_id":prj,"project_type":a["project_type"],"p0_start_rate":a["start_rate"],"p8_start_rate":b["start_rate"],"start_rate_difference":f'{float(b["start_rate"])-float(a["start_rate"]):.9f}',"p0_discounted_gross_revenue":a["avg_discounted_gross_revenue"],"p8_discounted_gross_revenue":b["avg_discounted_gross_revenue"],"discounted_gross_difference":f'{float(b["avg_discounted_gross_revenue"])-float(a["avg_discounted_gross_revenue"]):.6f}'})
    write(os.path.join(out_dir,"104_PROJECT_P8_VS_P0.csv"),list(cmp[0]),cmp)

    top=sorted(cmp,key=lambda r:abs(float(r["start_rate_difference"])),reverse=True)[:15];mx=max(abs(float(r["start_rate_difference"])) for r in top) or 1;b="";zero=500
    for i,r in enumerate(top):
        y=100+i*31;v=float(r["start_rate_difference"]);width=360*abs(v)/mx;color="#0B8F55" if v>=0 else "#D24A4A";x=zero if v>=0 else zero-width;b+=txt(125,y+5,r["project_id"],11,"start",color,True)+line(zero,y-11,zero,y+12,"#8b98a5")+rect(x,y-8,width,17,color)+txt(zero+(width+8)*(1 if v>=0 else -1),y+5,f"{v:+.3f}",10,"start" if v>=0 else "end",color)
    with open(os.path.join(out_dir,"08_PROJECT_START_DIFFERENCE.svg"),"w",encoding="utf-8") as f:f.write(svg("Largest project start-rate differences: P8 versus P0","Positive bars indicate projects started more often under preemptive hiring",b))

    b="";stages=[("Observed",1.0),("Proposed",None),("Started",None)]
    for col,pid in enumerate(("P0","P8")):
        a=policy_type[(pid,"EXTERNAL")];vals=[1,a["proposed"]/a["weight"],a["started"]/a["weight"]];cx=300+col*400;b+=txt(cx,105,pid,18,color="#4E79A7" if pid=="P0" else "#0B8F55",bold=True)
        for i,(label,_) in enumerate(stages):
            width=260*vals[i];x=cx-width/2;y=140+i*125;color="#4E79A7" if pid=="P0" else "#0B8F55";b+=rect(x,y,width,65,color)+txt(cx,y+29,label,13,color="white",bold=True)+txt(cx,y+50,f"{vals[i]*100:.1f}%",12,color="white")
            if i<2:b+=line(cx,y+65,cx,y+120,"#9aa6b2",2)
    with open(os.path.join(out_dir,"09_EXTERNAL_PROJECT_FUNNEL.svg"),"w",encoding="utf-8") as f:f.write(svg("External-project decision funnel","Probability-weighted rates across all external candidates and scenarios",b))

    groups=sorted({r["scenario_group"] for r in gs});types=("INTERNAL","EXTERNAL");lookup={(r["scenario_group"],r["policy_id"],r["project_type"]):float(r["start_rate"]) for r in gs};vals={(g,t):lookup[(g,"P8",t)]-lookup[(g,"P0",t)] for g in groups for t in types};mx=max(abs(v) for v in vals.values()) or 1;b="";x0,y0,cw,ch=410,115,210,58
    for j,t in enumerate(types):b+=txt(x0+j*cw+cw/2,98,t,13,bold=True)
    for i,g in enumerate(groups):
        b+=txt(x0-18,y0+i*ch+35,g,11,"end",bold=True)
        for j,t in enumerate(types):
            v=vals[(g,t)];q=min(abs(v)/mx,1);base=(11,143,85) if v>=0 else (210,66,66);rgb=tuple(round(245+(c-245)*q) for c in base);c="#%02x%02x%02x"%rgb;b+=rect(x0+j*cw,y0+i*ch,cw-4,ch-4,c,"white")+txt(x0+j*cw+cw/2,y0+i*ch+34,f"{v:+.3f}",12,color="white" if q>.5 else "#25313c",bold=True)
    with open(os.path.join(out_dir,"10_SCENARIO_PROJECT_ACTIVATION.svg"),"w",encoding="utf-8") as f:f.write(svg("Scenario-specific project activation: P8 minus P0","Difference in probability-weighted start rate by project type",b))

    outputs=["102_PROJECT_POLICY_SUMMARY.csv","103_PROJECT_SCENARIO_SUMMARY.csv","104_PROJECT_P8_VS_P0.csv","08_PROJECT_START_DIFFERENCE.svg","09_EXTERNAL_PROJECT_FUNNEL.svg","10_SCENARIO_PROJECT_ACTIVATION.svg"]
    audit={"source_rows":count,"expected_rows":2660000,"valid_row_count":count==2660000,"violations":dict(viol),"valid_logic":not any(viol.values()),"outputs":{}}
    for name in outputs:
        with open(os.path.join(out_dir,name),"rb") as f:audit["outputs"][name]=hashlib.sha256(f.read()).hexdigest()
    with open(os.path.join(out_dir,"PROJECT_DECISION_AUDIT.json"),"w",encoding="utf-8") as f:json.dump(audit,f,indent=2)
    print(json.dumps(audit,indent=2))

if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: analyze_project_decisions.py DATA_DIR RESULT_DIR")
    main(sys.argv[1],sys.argv[2])
