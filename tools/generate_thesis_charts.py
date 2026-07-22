"""Generate dependency-free SVG figures for the PPSP dissertation results."""
import csv, hashlib, html, json, math, os, sys
from collections import defaultdict
from xml.etree import ElementTree

W, H = 1000, 620
COLORS = {"P0":"#4E79A7","P1":"#F28E2B","P2":"#E15759","P3":"#76B7B2",
          "P4":"#59A14F","P5":"#EDC948","P6":"#B07AA1","P7":"#FF9DA7",
          "P8":"#0B8F55","P9":"#9C755F"}

def read(path):
    with open(path, encoding="utf-8-sig", newline="") as f: return list(csv.DictReader(f))
def esc(x): return html.escape(str(x))
def line(x1,y1,x2,y2,stroke="#ccd3db",width=1,dash=""):
    d=f' stroke-dasharray="{dash}"' if dash else ""
    return f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{width}"{d}/>'
def text(x,y,s,size=13,anchor="middle",fill="#25313c",weight="normal",rotate=None):
    tr=f' transform="rotate({rotate} {x} {y})"' if rotate is not None else ""
    return f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" text-anchor="{anchor}" fill="{fill}" font-weight="{weight}"{tr}>{esc(s)}</text>'
def circle(x,y,r,fill,stroke="white",width=1,opacity=1):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" opacity="{opacity}"/>'
def rect(x,y,w,h,fill,stroke="none",width=1,opacity=1):
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" opacity="{opacity}"/>'
def poly(points,stroke, width=2, fill="none", opacity=1):
    p=" ".join(f"{x:.1f},{y:.1f}" for x,y in points)
    return f'<polyline points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" opacity="{opacity}"/>'
def svg(title, subtitle, body, note=""):
    head=f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><rect width="100%" height="100%" fill="white"/>'
    head+=text(50,38,title,22,"start","#16222e","bold")+text(50,62,subtitle,13,"start","#536273")
    if note: body+=text(50,H-18,note,11,"start","#697887")
    return head+body+"</svg>"
def save(out,name,content):
    with open(os.path.join(out,name),"w",encoding="utf-8",newline="") as f:f.write(content)
def scale(v,a,b,p,q): return (p+q)/2 if b==a else p+(v-a)*(q-p)/(b-a)
def axes(x0,y0,x1,y1,xticks,yticks,xfmt=str,yfmt=str):
    b=rect(x0,y0,x1-x0,y1-y0,"#fbfcfd","#d8dee6")
    for v,p in yticks:
        b+=line(x0,p,x1,p,"#e7ebef")+text(x0-10,p+4,yfmt(v),11,"end")
    for v,p in xticks:b+=line(p,y0,p,y1,"#eef1f4")+text(p,y1+22,xfmt(v),11)
    return b

def cumulative(outdir, rows):
    chosen=["P0","P1","P3","P7","P8","P9"]; x0,y0,x1,y1=90,95,950,495
    vals=[float(r["avg_cumulative_profit"]) for r in rows if r["policy_id"] in chosen]; ymin=0;ymax=max(vals)*1.05
    xt=[(m,scale(m,1,60,x0,x1)) for m in (1,12,24,36,48,60)]
    yt=[(v,scale(v,ymin,ymax,y1,y0)) for v in range(0,200001,40000)]
    b=axes(x0,y0,x1,y1,xt,yt,lambda v:str(v),lambda v:f"{v/1000:.0f}k")
    for pid in chosen:
        rr=sorted((r for r in rows if r["policy_id"]==pid),key=lambda r:int(r["month"]));pts=[(scale(int(r["month"]),1,60,x0,x1),scale(float(r["avg_cumulative_profit"]),ymin,ymax,y1,y0)) for r in rr]
        b+=poly(pts,COLORS[pid],3 if pid in ("P0","P8") else 1.7,opacity=1 if pid in ("P0","P8") else .75)
    b+=text((x0+x1)/2,535,"X: Simulation month (month)",12)+text(22,(y0+y1)/2,"Y: Cumulative objective (KRW million)",12,rotate=-90)
    for i,pid in enumerate(chosen): b+=line(110+i*135,568,138+i*135,568,COLORS[pid],3)+text(144+i*135,572,pid,12,"start",COLORS[pid],"bold")
    save(outdir,"01_CUMULATIVE_PROFIT.svg",svg("Cumulative objective trajectory","Probability-weighted mean by policy; P0 and P8 emphasized",b,"Figure 1. Values are discounted and penalty-adjusted under the reference configuration."))

def distributions(outdir, raw):
    series={p:[float(r["objective_value"]) for r in raw if r["policy_id"]==p] for p in ("P0","P8")}; allv=series["P0"]+series["P8"]
    lo=min(allv);hi=max(allv);bins=32;step=(hi-lo)/bins;x0,y0,x1,y1=90,95,950,495
    density={}; ymax=0
    for pid,vs in series.items():
        c=[0]*bins
        for v in vs:c[min(int((v-lo)/step),bins-1)]+=1
        d=[n/(len(vs)*step) for n in c];density[pid]=d;ymax=max(ymax,max(d))
        mu=sum(vs)/len(vs);sd=math.sqrt(sum((v-mu)**2 for v in vs)/len(vs));ymax=max(ymax,1/(sd*math.sqrt(2*math.pi)))
    xt=[(v,scale(v,lo,hi,x0,x1)) for v in range(80000,221000,20000) if lo<=v<=hi];yt=[(v,scale(v,0,ymax*1.1,y1,y0)) for v in [0,ymax*.25,ymax*.5,ymax*.75,ymax]]
    b=axes(x0,y0,x1,y1,xt,yt,lambda v:f"{v/1000:.0f}k",lambda v:f"{v*1000:.3f}")
    bw=(x1-x0)/bins
    for label_index,pid in enumerate(("P0","P8")):
        for i,d in enumerate(density[pid]):b+=rect(x0+i*bw,scale(d,0,ymax*1.1,y1,y0),bw,scale(d,0,ymax*1.1,y1,y0)*-1+y1,COLORS[pid],opacity=.22)
        vs=series[pid];mu=sum(vs)/len(vs);sd=math.sqrt(sum((v-mu)**2 for v in vs)/len(vs));pts=[]
        for i in range(241):
            v=lo+(hi-lo)*i/240;d=math.exp(-.5*((v-mu)/sd)**2)/(sd*math.sqrt(2*math.pi));pts.append((scale(v,lo,hi,x0,x1),scale(d,0,ymax*1.1,y1,y0)))
        label_y=112+label_index*22;b+=poly(pts,COLORS[pid],3)+line(scale(mu,lo,hi,x0,x1),label_y+5,scale(mu,lo,hi,x0,x1),scale(1/(sd*math.sqrt(2*math.pi)),0,ymax*1.1,y1,y0),COLORS[pid],1,"3,3")+text(scale(mu,lo,hi,x0,x1),label_y,f"{pid} mean {mu:,.0f}",11,"middle",COLORS[pid],"bold")
    b+=text((x0+x1)/2,535,"X: Five-year objective (KRW million)",12)+text(22,(y0+y1)/2,"Y: Probability density (per KRW million)",12,rotate=-90)
    save(outdir,"02_OUTCOME_DISTRIBUTION.svg",svg("Observed outcome distributions and normal overlays","14,000-run experiment subset: 1,400 actual outcomes per displayed policy",b,"Figure 2. Transparent bars are empirical densities; curves are descriptive normal fits, not normality claims."))

def scenario_heatmap(outdir, rows):
    groups=[];policies=[f"P{i}" for i in range(10)];lookup={}
    for r in rows:
        if r["scenario_group"] not in groups:groups.append(r["scenario_group"])
        lookup[(r["scenario_group"],r["policy_id"])]=float(r["avg_objective"])
    diffs={(g,p):lookup[(g,p)]-lookup[(g,"P0")] for g in groups for p in policies};mx=max(abs(v) for v in diffs.values());x0,y0=190,105;cw,ch=73,55;b=""
    for j,p in enumerate(policies):b+=text(x0+j*cw+cw/2,92,p,12,weight="bold")
    for i,g in enumerate(groups):
        b+=text(x0-12,y0+i*ch+34,g,11,"end",weight="bold")
        for j,p in enumerate(policies):
            v=diffs[(g,p)];t=min(abs(v)/mx,1);base=(11,143,85) if v>=0 else (210,66,66);rgb=tuple(round(245+(c-245)*t) for c in base);color="#%02x%02x%02x"%rgb
            b+=rect(x0+j*cw,y0+i*ch,cw-2,ch-2,color,"white")+text(x0+j*cw+cw/2,y0+i*ch+32,f"{v/1000:+.1f}k",10,fill="white" if t>.55 else "#25313c")
    b+=text(x0+5*cw,525,"X: Policy (P0-P9)",12)+text(28,y0+3.5*ch,"Y: Scenario group",12,rotate=-90)+text(x0+5*cw,548,"Cell: objective difference vs P0 (thousand KRW million)",11)
    save(outdir,"03_SCENARIO_POLICY_HEATMAP.svg",svg("Scenario-group policy effects relative to P0","Cell value = group mean objective difference from balanced baseline",b,"Figure 3. Green indicates improvement over P0; red indicates deterioration."))

def risk_return(outdir, summary):
    x0,y0,x1,y1=90,95,950,490;xs=[float(r["std_objective"]) for r in summary];ys=[float(r["worst_10pct_objective"]) for r in summary];xmin=min(xs)*.9;xmax=max(xs)*1.05;ymin=min(ys)*.92;ymax=max(ys)*1.05
    xt=[(v,scale(v,xmin,xmax,x0,x1)) for v in range(16000,25001,2000)];yt=[(v,scale(v,ymin,ymax,y1,y0)) for v in range(80000,125001,10000)]
    b=axes(x0,y0,x1,y1,xt,yt,lambda v:f"{v/1000:.0f}k",lambda v:f"{v/1000:.0f}k")
    label_offsets={"P0":(14,-17),"P1":(14,22),"P2":(14,-24),"P3":(14,28),"P4":(14,-12),"P5":(14,25),"P6":(-16,20),"P7":(14,18),"P8":(-18,-19),"P9":(14,18)}
    for r in summary:
        p=r["policy_id"];x=scale(float(r["std_objective"]),xmin,xmax,x0,x1);y=scale(float(r["worst_10pct_objective"]),ymin,ymax,y1,y0);size=8+max(float(r["avg_objective"])-100000,0)/9000
        dx,dy=label_offsets[p];anchor="start" if dx>0 else "end";b+=circle(x,y,size,COLORS[p],opacity=.85)+line(x,y,x+dx,y+dy,COLORS[p],1)+text(x+dx+(2 if dx>0 else -2),y+dy,p,11,anchor,COLORS[p],"bold")
    b+=text((x0+x1)/2,532,"X: Objective standard deviation (KRW million; lower is safer)",12)+text(22,(y0+y1)/2,"Y: Worst 10% mean (KRW million; higher is better)",12,rotate=-90)+text((x0+x1)/2,555,"Bubble area: mean objective",11)
    save(outdir,"04_RISK_RETURN_MAP.svg",svg("Risk-return policy map","Bubble size represents mean objective; upper-left is the desirable direction",b,"Figure 4. No single point dominates every criterion; policy choice depends on the loss function."))

def forest(outdir, rows):
    rr=[r for r in rows if r["policy_id"]!="P0"];x0,y0,x1,y1=170,90,950,525;lo=min(float(r["ci95_lower"]) for r in rr);hi=max(float(r["ci95_upper"]) for r in rr);bound=max(abs(lo),abs(hi));bound=math.ceil(bound/10000)*10000
    ticks=[v for v in range(-bound,bound+1,10000)];b=axes(x0,y0,x1,y1,[(v,scale(v,-bound,bound,x0,x1)) for v in ticks],[],lambda v:f"{v/1000:+.0f}k")+line(scale(0,-bound,bound,x0,x1),y0,scale(0,-bound,bound,x0,x1),y1,"#25313c",1.5)
    for i,r in enumerate(rr):
        p=r["policy_id"];y=y0+25+i*45;l=scale(float(r["ci95_lower"]),-bound,bound,x0,x1);h=scale(float(r["ci95_upper"]),-bound,bound,x0,x1);m=scale(float(r["paired_mean_difference"]),-bound,bound,x0,x1)
        b+=text(x0-20,y+4,p,12,"end",COLORS[p],"bold")+line(l,y,h,y,COLORS[p],3)+line(l,y-6,l,y+6,COLORS[p],2)+line(h,y-6,h,y+6,COLORS[p],2)+circle(m,y,6,COLORS[p])
    b+=text((x0+x1)/2,558,"X: Paired objective difference vs P0 (KRW million)",12)+text(30,(y0+y1)/2,"Y: Policy",12,rotate=-90)
    save(outdir,"05_PAIRED_EFFECT_FOREST.svg",svg("Paired policy effects versus P0","Mean difference and scenario-clustered 95% confidence interval",b,"Figure 5. Intervals crossing zero do not establish a directional difference at the 5% level."))

def sensitivity(outdir, rows):
    p0={(r["annual_discount_rate"],r["internal_ratio_penalty"]):float(r["avg_objective"]) for r in rows if r["policy_id"]=="P0"};p8={(r["annual_discount_rate"],r["internal_ratio_penalty"]):float(r["avg_objective"]) for r in rows if r["policy_id"]=="P8"};rates=sorted({r["annual_discount_rate"] for r in rows},key=float);pens=sorted({r["internal_ratio_penalty"] for r in rows},key=float);vals={(a,b):p8[(a,b)]-p0[(a,b)] for a in rates for b in pens};mx=max(abs(v) for v in vals.values());x0,y0,cw,ch=260,130,160,105;b=""
    for j,p in enumerate(pens):b+=text(x0+j*cw+cw/2,112,f"Penalty {p}",13,weight="bold")
    for i,a in enumerate(rates):
        b+=text(x0-20,y0+i*ch+58,f"Discount {float(a)*100:.0f}%",13,"end",weight="bold")
        for j,p in enumerate(pens):
            v=vals[(a,p)];t=abs(v)/mx;rgb=tuple(round(242+(c-242)*t) for c in (11,143,85));color="#%02x%02x%02x"%rgb
            b+=rect(x0+j*cw,y0+i*ch,cw-5,ch-5,color,"white")+text(x0+j*cw+cw/2,y0+i*ch+45,f"P8 - P0",11,fill="white" if t>.55 else "#25313c")+text(x0+j*cw+cw/2,y0+i*ch+68,f"{v:+,.0f}",16,fill="white" if t>.55 else "#25313c",weight="bold")
    b+=text(x0+1.5*cw,570,"X: Internal-ratio penalty multiplier",12)+text(92,y0+2*ch,"Y: Annual discount rate (%)",12,rotate=-90)+text(x0+1.5*cw,548,"Cell: P8 - P0 objective (KRW million)",11)
    save(outdir,"06_SENSITIVITY_GRID.svg",svg("P8 advantage across evaluation assumptions","Fixed-decision evaluation sensitivity: objective difference P8 minus P0",b,"Figure 6. This grid evaluates unchanged decisions and is not parameter-specific re-optimization."))

def projects(outdir, internal, external):
    items=[]
    for typ,rows in (("Internal",internal),("External",external)):
        for r in rows:items.append((r["project_id"],typ,float(r["resource_burden_score"]),float(r["revenue_total"])*float(r["expected_margin"]),float(r.get("p_estimated") or 1)))
    x0,y0,x1,y1=90,95,950,490;xs=[x[2] for x in items];ys=[x[3] for x in items];xt=[(v,scale(v,0,1,x0,x1)) for v in (0,.2,.4,.6,.8,1)];yt=[(v,scale(v,0,max(ys)*1.08,y1,y0)) for v in range(0,1001,200)]
    b=axes(x0,y0,x1,y1,xt,yt,lambda v:f"{v:.1f}",lambda v:str(v))
    for pid,typ,x,y,p in items:
        px=scale(x,0,1,x0,x1);py=scale(y,0,max(ys)*1.08,y1,y0);color="#4E79A7" if typ=="Internal" else "#F28E2B";b+=circle(px,py,4+9*p,color,opacity=.55)
        if y>max(ys)*.72 or (x<.35 and y>max(ys)*.45):b+=text(px+7,py-7,pid,9,"start",color)
    b+=text((x0+x1)/2,530,"X: Resource burden score (0-1)",12)+text(22,(y0+y1)/2,"Y: Expected margin value (KRW million)",12,rotate=-90)
    b+=circle(690,558,7,"#4E79A7",opacity=.7)+text(703,562,"Internal",11,"start")+circle(800,558,7,"#F28E2B",opacity=.7)+text(813,562,"External",11,"start")+text(90,562,"Bubble size: estimated win probability (external)",10,"start")
    save(outdir,"07_PROJECT_PORTFOLIO_MAP.svg",svg("Project attribute portfolio map","Expected margin value versus resource burden; external bubble size reflects estimated win probability",b,"Figure 7. This is an input-attribute map, not evidence of policy-specific project selection."))

def main(data,out):
    os.makedirs(out,exist_ok=True);summary=read(os.path.join(out,"93_OUTPUT_POLICY_SUMMARY.csv"));raw=read(os.path.join(out,"90_OUTPUT_POLICY_SCENARIO.csv"))
    cumulative(out,read(os.path.join(out,"97_OUTPUT_MONTHLY_POLICY.csv")));distributions(out,raw);scenario_heatmap(out,read(os.path.join(out,"95_OUTPUT_SCENARIO_GROUP.csv")));risk_return(out,summary);forest(out,read(os.path.join(out,"100_POLICY_COMPARISON_VS_P0.csv")));sensitivity(out,read(os.path.join(out,"101_SENSITIVITY_SUMMARY.csv")));projects(out,read(os.path.join(data,"04_INTERNAL_CANDIDATES.csv")),read(os.path.join(data,"05_EXTERNAL_CANDIDATES.csv")))
    audit={"generator":"dependency-free SVG","figures":{}}
    for name in sorted(n for n in os.listdir(out) if n.endswith(".svg")):
        path=os.path.join(out,name);ElementTree.parse(path)
        with open(path,"rb") as f:digest=hashlib.sha256(f.read()).hexdigest()
        audit["figures"][name]={"valid_xml":True,"sha256":digest}
    audit["figure_count"]=len(audit["figures"])
    with open(os.path.join(out,"VISUAL_AUDIT.json"),"w",encoding="utf-8") as f:json.dump(audit,f,ensure_ascii=False,indent=2)
    print("Validated",audit["figure_count"],"SVG dissertation figures in",out)

if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: generate_thesis_charts.py DATA_DIR RESULT_DIR")
    main(sys.argv[1],sys.argv[2])
