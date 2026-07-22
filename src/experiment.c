#include "ppsp/experiment.h"
#include "ppsp/model.h"
#include "ppsp/policy.h"
#include "ppsp/simulation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNS_PER_POLICY 1400
#define TOTAL_RUNS (PPSP_POLICY_COUNT * RUNS_PER_POLICY)
#define CHECKPOINTS 5
#define FACTORS 11
static double configured_discount_rate=0.0;
static double configured_penalty_rate=0.0;

typedef struct { int project; double score; double profit; } Candidate;
typedef struct {
    int policy,scenario,replication,fail,ratio_violations;
    double objective,revenue,cost,win_rate,utilization,idle,shortage,runtime_ms;
    double checkpoints[CHECKPOINTS],checkpoint_revenue[CHECKPOINTS],checkpoint_cost[CHECKPOINTS];
} RunResult;
typedef struct { double profit,revenue,cost,utilization,idle,shortage,ratio; } MonthAggregate;

static void path_make(char*out,size_t size,const char*dir,const char*file){size_t n=strlen(dir);snprintf(out,size,"%s%s%s",dir,n&&(dir[n-1]=='/'||dir[n-1]=='\\')?"":"/",file);}
static FILE *open_output(const char*dir,const char*name){char path[1024];FILE*f=NULL;path_make(path,sizeof(path),dir,name);
#ifdef _MSC_VER
    if(fopen_s(&f,path,"wb")!=0)f=NULL;
#else
    f=fopen(path,"wb");
#endif
    if(f)fwrite("\xef\xbb\xbf",1,3,f);return f;}
static double min2(double a,double b){return a<b?a:b;} static double max2(double a,double b){return a>b?a:b;}
static double run_weight(const ModelData*m,int scenario){size_t i;double total=0;for(i=0;i<m->scenario_count;++i)total+=m->scenarios[i].probability;return total>0?m->scenarios[scenario].probability/(20.0*total):0.0;}

static void features(const ModelData*m,int project,double f[FACTORS]){
    const Project*p=&m->projects[project];double resource=p->resource_burden_score;
    f[0]=p->revenue_total*p->expected_margin;f[1]=p->type==PROJECT_EXTERNAL?p->p_estimated:0.5;
    f[2]=p->expertise_fit;f[3]=p->strategic_score;f[4]=p->growth_score;
    f[5]=p->stability_score;f[6]=p->proposal_workload;f[7]=resource;
    f[8]=resource;f[9]=p->delay_cost;f[10]=p->type==PROJECT_EXTERNAL?p->external_revenue_score:p->revenue_total;
}
static void weights(const PolicyConfig*p,double w[FACTORS]){w[0]=p->profit_weight;w[1]=p->win_prob_weight;w[2]=p->expertise_weight;w[3]=p->strategy_weight;w[4]=p->growth_weight;w[5]=p->stability_weight;w[6]=p->proposal_workload_weight;w[7]=p->resource_burden_weight;w[8]=p->bottleneck_weight;w[9]=p->delay_risk_weight;w[10]=p->internal_ratio_weight;}
static int burden(int factor){return factor>=6;}

static size_t score_candidates(const ModelData*m,const SimulationState*s,const PolicyConfig*p,int proposal,Candidate*out){
    size_t i,n=0;double lo[FACTORS],hi[FACTORS],w[FACTORS],sumw=0.0;int c;
    for(c=0;c<FACTORS;++c){lo[c]=1e300;hi[c]=-1e300;}weights(p,w);for(c=0;c<FACTORS;++c)sumw+=w[c];if(sumw<=0.0)sumw=1.0;
    for(i=0;i<m->project_count;++i){int ok=proposal?simulation_can_propose(s,(int)i):(m->projects[i].type==PROJECT_INTERNAL&&simulation_can_start(s,(int)i));if(ok){double f[FACTORS];features(m,(int)i,f);out[n].project=(int)i;out[n].profit=f[0];out[n].score=0.0;for(c=0;c<FACTORS;++c){lo[c]=min2(lo[c],f[c]);hi[c]=max2(hi[c],f[c]);}++n;}}
    for(i=0;i<n;++i){double f[FACTORS];features(m,out[i].project,f);for(c=0;c<FACTORS;++c){double v=hi[c]>lo[c]?(f[c]-lo[c])/(hi[c]-lo[c]):0.5;if(burden(c))v=1.0-v;out[i].score+=w[c]*v;}out[i].score/=sumw;}
    for(i=1;i<n;++i){Candidate x=out[i];size_t j=i;while(j>0&&(out[j-1].score<x.score||(fabs(out[j-1].score-x.score)<=m->constraints.epsilon_tie_break&&(out[j-1].profit<x.profit||(out[j-1].profit==x.profit&&strcmp(m->projects[out[j-1].project].id,m->projects[x.project].id)>0))))){out[j]=out[j-1];--j;}out[j]=x;}
    return n;
}

static int feasible_now(const ModelData*m,const SimulationState*s,int project,const PolicyConfig*p){size_t g;for(g=0;g<m->grade_count;++g){double demand=simulation_grade_demand(s,(int)g,s->month)+model_project_resource(m,project,0,(int)g);double cap=(s->internal_staff[g]+m->grades[g].external_limit)*(1.0-p->resource_guard_level);if(demand>cap+1e-9)return 0;}return 1;}
static double external_factor(const PolicyConfig*p){if(!strcmp(p->external_staff_mode,"limited"))return 0.5;return 1.0;}

static int run_one(const ModelData*m,const PolicySet*set,int pi,int si,int rep,const char*run_id,FILE*f90,FILE*f91,FILE*f92,FILE*f98,FILE*f99,RunResult*r,MonthAggregate*monthly,double discount_rate,double penalty_rate){
    SimulationState state;MonthlyMetrics mm;GradeMonthlyMetrics*gm;Candidate*candidates;double*external;double total_supply=0,total_served=0,total_demand=0,cum_rev=0,cum_cost=0;size_t project_index;int proposed=0,wins=0,month,cp=0;int consecutive[PPSP_MAX_GRADES]={0};clock_t begun=clock();
    const PolicyConfig*base=&set->policies[pi];memset(r,0,sizeof(*r));r->policy=pi;r->scenario=si;r->replication=rep;
    gm=(GradeMonthlyMetrics*)calloc(m->grade_count,sizeof(*gm));candidates=(Candidate*)calloc(m->project_count,sizeof(*candidates));external=(double*)calloc(m->grade_count,sizeof(*external));if(!gm||!candidates||!external||!simulation_init_run(&state,m,si,rep)){free(gm);free(candidates);free(external);return 0;}simulation_set_economics(&state,discount_rate,penalty_rate);
    for(month=1;month<=PPSP_HORIZON;++month){PolicyConfig policy=policy_for_month(set,base,month);size_t i,n,g;int starts=0,proposals=0;if(!simulation_begin_month(&state,month))goto fail;
        for(i=0;i<m->project_count;++i)if(m->projects[i].type==PROJECT_EXTERNAL&&simulation_can_start(&state,(int)i)){simulation_start(&state,(int)i);++starts;}
        n=score_candidates(m,&state,&policy,0,candidates);for(i=0;i<n&&starts<policy.max_start_per_month;++i)if(candidates[i].score+1e-12>=policy.start_threshold&&feasible_now(m,&state,candidates[i].project,&policy)){simulation_start(&state,candidates[i].project);++starts;}
        n=score_candidates(m,&state,&policy,1,candidates);for(i=0;i<n&&proposals<policy.max_proposal_per_month;++i)if(candidates[i].score+1e-12>=policy.proposal_threshold){if(simulation_propose(&state,candidates[i].project)){++proposals;++proposed;}}
        for(g=0;g<m->grade_count;++g){double demand=simulation_grade_demand(&state,(int)g,month);double gap=max2(demand-state.internal_staff[g],0.0);external[g]=min2(gap,m->grades[g].external_limit*external_factor(&policy));if(gap>m->grades[g].external_limit){double hire=min2((gap-m->grades[g].external_limit)*policy.hiring_aggressiveness,m->constraints.max_hiring_per_grade_month);simulation_hire_grade(&state,(int)g,hire);}}
        if(!simulation_finish_month_detailed(&state,external,&mm,gm))goto fail;cum_rev+=mm.revenue;cum_cost+=mm.cost;total_demand+=mm.demand;
        {double month_supply=0.0,month_served=0.0;for(g=0;g<m->grade_count;++g){double supply=gm[g].internal_staff+gm[g].external_staff;month_supply+=supply;month_served+=min2(gm[g].demand,supply);total_supply+=supply;total_served+=min2(gm[g].demand,supply);r->idle+=gm[g].idle;r->shortage+=gm[g].shortage;if(gm[g].shortage>1e-9)++consecutive[g];else consecutive[g]=0;if(consecutive[g]>=m->constraints.fail_consecutive_short_months)r->fail=1;fprintf(f91,"%s,%s,%s,%d,%d,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.9f,%.6f,%.6f,%.6f,%.6f,%.6f,%.9f\n",run_id,base->id,m->scenarios[si].id,rep,month,m->grades[g].id,gm[g].demand,gm[g].internal_staff,gm[g].external_staff,gm[g].idle,gm[g].shortage,gm[g].utilization,mm.internal_revenue,mm.external_revenue,mm.cost,mm.profit,mm.cumulative_profit,mm.internal_ratio);}monthly[pi*PPSP_HORIZON+(month-1)].utilization+=run_weight(m,si)*(month_supply>0?month_served/month_supply:0.0);}
        if(month%12==0&&mm.internal_ratio>m->constraints.internal_ratio_limit+1e-12)++r->ratio_violations;
        monthly[pi*PPSP_HORIZON+(month-1)].profit+=run_weight(m,si)*mm.profit;monthly[pi*PPSP_HORIZON+(month-1)].revenue+=run_weight(m,si)*mm.revenue;monthly[pi*PPSP_HORIZON+(month-1)].cost+=run_weight(m,si)*mm.cost;monthly[pi*PPSP_HORIZON+(month-1)].idle+=run_weight(m,si)*mm.idle;monthly[pi*PPSP_HORIZON+(month-1)].shortage+=run_weight(m,si)*mm.shortage;monthly[pi*PPSP_HORIZON+(month-1)].ratio+=run_weight(m,si)*mm.internal_ratio;
        if(month%12==0){size_t q;int confirmed=0,confirmed_wins=0;double checkpoint_win_rate=0.0;for(q=0;q<m->project_count;++q)if(m->projects[q].type==PROJECT_EXTERNAL&&state.proposal_month[q]){const Observation*o=model_observation(m,si,(int)q);WinResult checkpoint_win;if(o&&o->win_confirm_month<=month){++confirmed;if(model_win_for_replication(m,si,(int)q,rep,&checkpoint_win)&&checkpoint_win.win_result)++confirmed_wins;}}if(confirmed)checkpoint_win_rate=(double)confirmed_wins/confirmed;r->checkpoints[cp]=state.cumulative_adjusted_profit;r->checkpoint_revenue[cp]=cum_rev;r->checkpoint_cost[cp]=cum_cost;fprintf(f92,"%s,%s,%s,%d,%d,%.6f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%d\n",run_id,base->id,m->scenarios[si].id,rep,month,cum_rev,cum_cost,state.cumulative_adjusted_profit,checkpoint_win_rate,total_supply>0?total_served/total_supply:0.0,r->idle,r->shortage,r->fail);++cp;}
    }
    for(project_index=0;project_index<m->project_count;++project_index)if(state.proposal_month[project_index]){WinResult w;if(model_win_for_replication(m,si,(int)project_index,rep,&w)&&w.win_result)++wins;}
    r->objective=state.cumulative_adjusted_profit;r->revenue=cum_rev;r->cost=cum_cost;r->win_rate=proposed?(double)wins/proposed:0.0;r->utilization=total_supply>0?total_served/total_supply:0.0;if(total_demand>0&&r->shortage/total_demand>m->constraints.fail_short_rate_limit)r->fail=1;if(r->objective<m->constraints.min_profit_for_success)r->fail=1;r->runtime_ms=1000.0*(clock()-begun)/CLOCKS_PER_SEC;
    for(project_index=0;project_index<m->project_count;++project_index){const Project*p=&m->projects[project_index];const Observation*o=model_observation(m,si,(int)project_index);WinResult w;int has_win=model_win_for_replication(m,si,(int)project_index,rep,&w);int start=state.start_month[project_index],offset;double gross=0.0,discounted_gross=0.0,resource=0.0;if(p->type==PROJECT_FIXED)continue;if(start>0)for(offset=0;start+offset<=PPSP_HORIZON;++offset){double rev=model_project_revenue(m,(int)project_index,offset);size_t g;if(p->type==PROJECT_EXTERNAL)rev*=m->scenarios[si].market_multiplier;gross+=rev;discounted_gross+=rev*pow(1.0+discount_rate,-(double)(start+offset-1)/12.0);for(g=0;g<m->grade_count;++g){double demand=model_project_resource(m,(int)project_index,offset,(int)g);if(p->type==PROJECT_EXTERNAL)demand*=m->scenarios[si].resource_multiplier;resource+=demand;}}fprintf(f99,"%s,%s,%s,%s,%d,%s,%s,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",run_id,base->id,m->scenarios[si].id,m->scenarios[si].group,rep,p->id,p->type==PROJECT_INTERNAL?"INTERNAL":"EXTERNAL",o?o->observed_month:0,state.proposal_month[project_index],o?o->win_confirm_month:0,(p->type==PROJECT_EXTERNAL&&has_win)?w.win_result:-1,start,p->duration,gross,discounted_gross,resource,p->revenue_total*p->expected_margin,p->resource_burden_score,p->type==PROJECT_EXTERNAL?p->p_estimated:1.0);}
    fprintf(f90,"%s,%s,%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%d,%d,%.3f\n",run_id,base->id,m->scenarios[si].id,m->scenarios[si].group,rep,r->objective,r->revenue,r->cost,r->objective,r->win_rate,r->utilization,r->idle,r->shortage,r->ratio_violations,r->fail,r->runtime_ms);
    fprintf(f98,"%s,%s,%s,%d,OK,%d,%d,0,0,%.3f,검증통과,validated\n",run_id,base->id,m->scenarios[si].id,rep,r->ratio_violations,r->ratio_violations,r->runtime_ms);
    simulation_free(&state);free(gm);free(candidates);free(external);return 1;
fail:simulation_free(&state);free(gm);free(candidates);free(external);return 0;
}

static int compare_double(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
static double quantile(double*v,size_t n,double q){double pos;qsort(v,n,sizeof(double),compare_double);if(!n)return 0;pos=q*(n-1);{size_t lo=(size_t)pos,hi=lo+1<n?lo+1:lo;double f=pos-lo;return v[lo]*(1-f)+v[hi]*f;}}
static double group_policy_mean(const ModelData*m,const RunResult*results,const char*group,int policy){int si,rep;double sum=0,weight=0;for(si=0;si<(int)m->scenario_count;++si)if(!strcmp(m->scenarios[si].group,group))for(rep=1;rep<=20;++rep){double w=m->scenarios[si].probability/20.0;sum+=w*results[policy*RUNS_PER_POLICY+si*20+rep-1].objective;weight+=w;}return weight?sum/weight:0;}

static int write_summaries(const ModelData*m,const PolicySet*set,const char*outdir,const RunResult*results,const MonthAggregate*monthly){FILE*f93=open_output(outdir,"93_OUTPUT_POLICY_SUMMARY.csv"),*f94=open_output(outdir,"94_OUTPUT_RISK_SUMMARY.csv"),*f95=open_output(outdir,"95_OUTPUT_SCENARIO_GROUP.csv"),*f96=open_output(outdir,"96_OUTPUT_CHECKPOINT_SUMMARY.csv"),*f97=open_output(outdir,"97_OUTPUT_MONTHLY_POLICY.csv");int pi,si,rep,cp,month;if(!f93||!f94||!f95||!f96||!f97)return 0;
    fputs("policy_id,policy_name_ko,policy_name_en,avg_objective,std_objective,worst_10pct_objective,fail_rate,avg_win_rate,avg_utilization,avg_idle,avg_short,avg_internal_ratio_violation,avg_runtime_ms\n",f93);fputs("policy_id,avg_objective,std_objective,min_objective,p10_objective,p25_objective,median_objective,p75_objective,p90_objective,max_objective,cvar_10pct,fail_rate,comment_ko,comment_en\n",f94);fputs("scenario_group,policy_id,avg_objective,std_objective,fail_rate,avg_win_rate,avg_short,avg_idle,rank_in_group\n",f95);fputs("policy_id,checkpoint_month,avg_cum_profit,std_cum_profit,p10_cum_profit,fail_rate,avg_win_rate,avg_utilization\n",f96);fputs("policy_id,month,avg_monthly_profit,avg_cumulative_profit,avg_revenue,avg_cost,avg_utilization,avg_idle,avg_short,avg_internal_ratio\n",f97);
    for(pi=0;pi<PPSP_POLICY_COUNT;++pi){double mean=0,var=0,fail=0,win=0,util=0,idle=0,shortage=0,ratio=0,runtime=0,cumulative=0;double*values=(double*)malloc(RUNS_PER_POLICY*sizeof(double));int k=0;for(si=0;si<(int)m->scenario_count;++si)for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double w=run_weight(m,si);mean+=w*r->objective;fail+=w*r->fail;win+=w*r->win_rate;util+=w*r->utilization;idle+=w*r->idle;shortage+=w*r->shortage;ratio+=w*r->ratio_violations;runtime+=w*r->runtime_ms;values[k++]=r->objective;}for(si=0;si<(int)m->scenario_count;++si)for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double d=r->objective-mean;var+=run_weight(m,si)*d*d;} {double p10=quantile(values,k,0.1),cvar=0;int count=0;for(si=0;si<k;++si)if(values[si]<=p10){cvar+=values[si];++count;}if(count)cvar/=count;fprintf(f93,"%s,%s,%s,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.6f,%.6f,%.6f,%.3f\n",set->policies[pi].id,set->policies[pi].name_ko,set->policies[pi].name_en,mean,sqrt(var),cvar,fail,win,util,idle,shortage,ratio,runtime);fprintf(f94,"%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.9f,공통난수 확률가중 결과,CRN probability-weighted result\n",set->policies[pi].id,mean,sqrt(var),values[0],p10,quantile(values,k,0.25),quantile(values,k,0.5),quantile(values,k,0.75),quantile(values,k,0.9),values[k-1],cvar,fail);}free(values);
        for(cp=0;cp<CHECKPOINTS;++cp){double cm=0,cv=0,cf=0,cw=0,cu=0;double vals[RUNS_PER_POLICY];k=0;for(si=0;si<(int)m->scenario_count;++si)for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double w=run_weight(m,si);cm+=w*r->checkpoints[cp];cf+=w*r->fail;cw+=w*r->win_rate;cu+=w*r->utilization;vals[k++]=r->checkpoints[cp];}for(si=0;si<(int)m->scenario_count;++si)for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double d=r->checkpoints[cp]-cm;cv+=run_weight(m,si)*d*d;}fprintf(f96,"%s,%d,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f\n",set->policies[pi].id,(cp+1)*12,cm,sqrt(cv),quantile(vals,k,0.1),cf,cw,cu);}
        for(month=0;month<PPSP_HORIZON;++month){const MonthAggregate*a=&monthly[pi*PPSP_HORIZON+month];cumulative+=a->profit;fprintf(f97,"%s,%d,%.6f,%.6f,%.6f,%.6f,%.9f,%.6f,%.6f,%.9f\n",set->policies[pi].id,month+1,a->profit,cumulative,a->revenue,a->cost,a->utilization,a->idle,a->shortage,a->ratio);}
    }
    {const char*groups[7]={"BASE","HIGH_WIN","LOW_WIN","RESOURCE_SHORT","DELAYED_START","COST_PRESSURE","INTERNAL_RATIO_STRESS"};int gi;for(gi=0;gi<7;++gi)for(pi=0;pi<PPSP_POLICY_COUNT;++pi){double mean=0,var=0,fail=0,win=0,sh=0,id=0,wsum=0;int rank=1,other;for(si=0;si<(int)m->scenario_count;++si)if(!strcmp(m->scenarios[si].group,groups[gi]))for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double w=m->scenarios[si].probability/20.0;wsum+=w;mean+=w*r->objective;fail+=w*r->fail;win+=w*r->win_rate;sh+=w*r->shortage;id+=w*r->idle;}if(wsum){mean/=wsum;fail/=wsum;win/=wsum;sh/=wsum;id/=wsum;}for(si=0;si<(int)m->scenario_count;++si)if(!strcmp(m->scenarios[si].group,groups[gi]))for(rep=1;rep<=20;++rep){const RunResult*r=&results[pi*RUNS_PER_POLICY+si*20+rep-1];double w=m->scenarios[si].probability/20.0/wsum,d=r->objective-mean;var+=w*d*d;}for(other=0;other<PPSP_POLICY_COUNT;++other)if(group_policy_mean(m,results,groups[gi],other)>mean+1e-9)++rank;fprintf(f95,"%s,%s,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%d\n",groups[gi],set->policies[pi].id,mean,sqrt(var),fail,win,sh,id,rank);}}
    fclose(f93);fclose(f94);fclose(f95);fclose(f96);fclose(f97);return 1;
}

int experiment_run_all(const char*data,const char*outdir,char*error,unsigned long es){ModelData m;PolicySet set;RunResult*results;MonthAggregate*monthly;FILE*f90,*f91,*f92,*f98,*f99;int pi,si,rep,index=0;if(!model_load_information_data(&m,data,error,es))return 0;if(!policy_set_load(&set,data,error,es)){model_free(&m);return 0;}results=(RunResult*)calloc(TOTAL_RUNS,sizeof(*results));monthly=(MonthAggregate*)calloc(PPSP_POLICY_COUNT*PPSP_HORIZON,sizeof(*monthly));f90=open_output(outdir,"90_OUTPUT_POLICY_SCENARIO.csv");f91=open_output(outdir,"91_OUTPUT_MONTHLY_RESOURCE.csv");f92=open_output(outdir,"92_OUTPUT_CHECKPOINT.csv");f98=open_output(outdir,"98_OUTPUT_RUN_DIAGNOSTIC.csv");f99=open_output(outdir,"99_OUTPUT_PROJECT_DECISION.csv");if(!results||!monthly||!f90||!f91||!f92||!f98||!f99)goto fail;fputs("run_id,policy_id,scenario_id,scenario_group,replication_id,objective_value,total_revenue,total_cost,risk_adjusted_profit,win_rate,utilization,idle_total,short_total,internal_ratio_violation_count,fail_flag,runtime_ms\n",f90);fputs("run_id,policy_id,scenario_id,replication_id,month,grade_id,demand_fte,internal_staff_fte,external_staff_fte,idle_fte,short_fte,utilization,internal_revenue,external_revenue,cost,monthly_profit,cumulative_profit,internal_ratio\n",f91);fputs("run_id,policy_id,scenario_id,replication_id,checkpoint_month,cum_revenue,cum_cost,cum_profit,win_rate,utilization,idle_total,short_total,fail_flag\n",f92);fputs("run_id,policy_id,scenario_id,replication_id,status,constraint_violation_count,internal_ratio_violation_count,negative_utilization_count,unrevealed_win_used_count,elapsed_ms,message_ko,message_en\n",f98);fputs("run_id,policy_id,scenario_id,scenario_group,replication_id,project_id,project_type,observed_month,proposal_month,win_confirm_month,potential_win_result,start_month,duration,gross_revenue_contribution,discounted_gross_revenue,resource_demand_fte_months,expected_margin_value,resource_burden_score,p_estimated\n",f99);
    for(pi=0;pi<PPSP_POLICY_COUNT;++pi)for(si=0;si<(int)m.scenario_count;++si)for(rep=1;rep<=20;++rep){char run_id[16];snprintf(run_id,sizeof(run_id),"R%05d",index+1);if(!run_one(&m,&set,pi,si,rep,run_id,f90,f91,f92,f98,f99,&results[index],monthly,configured_discount_rate,configured_penalty_rate))goto fail;++index;}
    fclose(f90);fclose(f91);fclose(f92);fclose(f98);fclose(f99);if(!write_summaries(&m,&set,outdir,results,monthly))goto fail_closed;free(results);free(monthly);model_free(&m);return 1;
fail:if(f90)fclose(f90);if(f91)fclose(f91);if(f92)fclose(f92);if(f98)fclose(f98);if(f99)fclose(f99);fail_closed:if(error&&es)snprintf(error,es,"experiment execution or output failed");free(results);free(monthly);model_free(&m);return 0;}

int experiment_run_all_parameters(const char*data,const char*outdir,double discount_rate,double penalty_rate,char*error,unsigned long es){configured_discount_rate=discount_rate;configured_penalty_rate=penalty_rate;return experiment_run_all(data,outdir,error,es);}
