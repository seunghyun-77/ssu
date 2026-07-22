#include "ppsp/policy.h"
#include "ppsp/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_path(char *out,size_t size,const char *dir,const char *file){
    size_t n=strlen(dir);snprintf(out,size,"%s%s%s",dir,n&&(dir[n-1]=='/'||dir[n-1]=='\\')?"":"/",file);
}
static double number(const CsvRow *r,int column){return column>=0&&(size_t)column<r->count?strtod(r->fields[column],NULL):0.0;}
static int column(const CsvRow *h,const char *name){return csv_column(h,name);}
static void text_copy(char *out,size_t size,const CsvRow *r,int c){snprintf(out,size,"%s",c>=0&&(size_t)c<r->count?r->fields[c]:"");}

static int load_policies(PolicySet *set,const char *path,char *error,size_t es){
    CsvReader reader;CsvRow h={0},r={0};int st;
    int id,name_ko,name_en,pw,ww,ew,sw,gw,stw,pww,rw,bw,dw,iw,pt,stt,mp,ms,guard,mode,hire,ratio;
    if(!csv_open(&reader,path)||csv_read_row(&reader,&h)!=1)goto bad;
    id=column(&h,"policy_id");name_ko=column(&h,"policy_name_ko");name_en=column(&h,"policy_name_en");
    pw=column(&h,"profit_weight");ww=column(&h,"win_prob_weight");ew=column(&h,"expertise_weight");sw=column(&h,"strategy_weight");gw=column(&h,"growth_weight");stw=column(&h,"stability_weight");pww=column(&h,"proposal_workload_weight");rw=column(&h,"resource_burden_weight");bw=column(&h,"bottleneck_weight");dw=column(&h,"delay_risk_weight");iw=column(&h,"internal_ratio_weight");pt=column(&h,"proposal_threshold");stt=column(&h,"start_threshold");mp=column(&h,"max_proposal_per_month");ms=column(&h,"max_start_per_month");guard=column(&h,"resource_guard_level");mode=column(&h,"external_staff_mode");hire=column(&h,"hiring_aggressiveness");ratio=column(&h,"internal_ratio_guard");
    if(id<0||pw<0||pt<0||stt<0||mp<0||ms<0)goto bad;
    while((st=csv_read_row(&reader,&r))==1){PolicyConfig*p;if(r.count<h.count||set->policy_count>=PPSP_POLICY_COUNT)goto bad;p=&set->policies[set->policy_count++];memset(p,0,sizeof(*p));text_copy(p->id,sizeof(p->id),&r,id);text_copy(p->name_ko,sizeof(p->name_ko),&r,name_ko);text_copy(p->name_en,sizeof(p->name_en),&r,name_en);p->profit_weight=number(&r,pw);p->win_prob_weight=number(&r,ww);p->expertise_weight=number(&r,ew);p->strategy_weight=number(&r,sw);p->growth_weight=number(&r,gw);p->stability_weight=number(&r,stw);p->proposal_workload_weight=number(&r,pww);p->resource_burden_weight=number(&r,rw);p->bottleneck_weight=number(&r,bw);p->delay_risk_weight=number(&r,dw);p->internal_ratio_weight=number(&r,iw);p->proposal_threshold=number(&r,pt);p->start_threshold=number(&r,stt);p->max_proposal_per_month=(int)number(&r,mp);p->max_start_per_month=(int)number(&r,ms);p->resource_guard_level=number(&r,guard);text_copy(p->external_staff_mode,sizeof(p->external_staff_mode),&r,mode);p->hiring_aggressiveness=number(&r,hire);p->internal_ratio_guard=number(&r,ratio);csv_row_free(&r);}
    csv_row_free(&h);csv_close(&reader);if(st!=0||set->policy_count!=PPSP_POLICY_COUNT)goto fail;return 1;
bad:csv_row_free(&r);csv_row_free(&h);csv_close(&reader);
fail:if(error&&es)snprintf(error,es,"invalid policy CSV: %s",path);return 0;
}

static int load_stages(PolicySet *set,const char *path,char *error,size_t es){
    CsvReader reader;CsvRow h={0},r={0};int st;int from,to,pw,ww,ew,gw,stw,iw,pt,stt,mp,ms,hire;
    if(!csv_open(&reader,path)||csv_read_row(&reader,&h)!=1)goto bad;
    from=column(&h,"from_month");to=column(&h,"to_month");pw=column(&h,"profit_weight");ww=column(&h,"win_prob_weight");ew=column(&h,"expertise_weight");gw=column(&h,"growth_weight");stw=column(&h,"stability_weight");iw=column(&h,"internal_ratio_weight");pt=column(&h,"proposal_threshold");stt=column(&h,"start_threshold");mp=column(&h,"max_proposal_per_month");ms=column(&h,"max_start_per_month");hire=column(&h,"hiring_aggressiveness");if(from<0||to<0)goto bad;
    while((st=csv_read_row(&reader,&r))==1){PolicyStage*x;if(r.count<h.count||set->stage_count>=4)goto bad;x=&set->stages[set->stage_count++];memset(x,0,sizeof(*x));x->from_month=(int)number(&r,from);x->to_month=(int)number(&r,to);x->override_values.profit_weight=number(&r,pw);x->override_values.win_prob_weight=number(&r,ww);x->override_values.expertise_weight=number(&r,ew);x->override_values.growth_weight=number(&r,gw);x->override_values.stability_weight=number(&r,stw);x->override_values.internal_ratio_weight=number(&r,iw);x->override_values.proposal_threshold=number(&r,pt);x->override_values.start_threshold=number(&r,stt);x->override_values.max_proposal_per_month=(int)number(&r,mp);x->override_values.max_start_per_month=(int)number(&r,ms);x->override_values.hiring_aggressiveness=number(&r,hire);csv_row_free(&r);}
    csv_row_free(&h);csv_close(&reader);if(st!=0||set->stage_count!=4)goto fail;return 1;
bad:csv_row_free(&r);csv_row_free(&h);csv_close(&reader);
fail:if(error&&es)snprintf(error,es,"invalid policy-stage CSV: %s",path);return 0;
}

int policy_set_load(PolicySet *set,const char *dir,char *error,size_t es){char path[1024];if(!set||!dir)return 0;memset(set,0,sizeof(*set));make_path(path,sizeof(path),dir,"12_POLICIES.csv");if(!load_policies(set,path,error,es))return 0;make_path(path,sizeof(path),dir,"14_POLICY_STAGE.csv");return load_stages(set,path,error,es);}
const PolicyConfig *policy_find(const PolicySet *set,const char *id){size_t i;if(!set||!id)return NULL;for(i=0;i<set->policy_count;++i)if(!strcmp(set->policies[i].id,id))return &set->policies[i];return NULL;}
PolicyConfig policy_for_month(const PolicySet *set,const PolicyConfig *base,int month){PolicyConfig p=*base;size_t i;if(strcmp(base->id,"P7"))return p;for(i=0;i<set->stage_count;++i)if(month>=set->stages[i].from_month&&month<=set->stages[i].to_month){const PolicyConfig*o=&set->stages[i].override_values;p.profit_weight=o->profit_weight;p.win_prob_weight=o->win_prob_weight;p.expertise_weight=o->expertise_weight;p.growth_weight=o->growth_weight;p.stability_weight=o->stability_weight;p.internal_ratio_weight=o->internal_ratio_weight;p.proposal_threshold=o->proposal_threshold;p.start_threshold=o->start_threshold;p.max_proposal_per_month=o->max_proposal_per_month;p.max_start_per_month=o->max_start_per_month;p.hiring_aggressiveness=o->hiring_aggressiveness;break;}return p;}
