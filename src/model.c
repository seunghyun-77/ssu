#include "ppsp/model.h"
#include "ppsp/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void error_set(char *out, size_t size, const char *what, const char *path) {
    if (out && size) snprintf(out, size, "%s: %s", what, path);
}
static void path_make(char *out, size_t size, const char *dir, const char *file) {
    size_t n = strlen(dir);
    snprintf(out, size, "%s%s%s", dir,
             n && (dir[n-1] == '/' || dir[n-1] == '\\') ? "" : "/", file);
}
static int append(void **array, size_t *count, size_t item_size, const void *item) {
    void *next = realloc(*array, (*count + 1) * item_size);
    if (!next) return 0;
    *array = next;
    memcpy((char *)next + *count * item_size, item, item_size);
    ++*count;
    return 1;
}
static int as_int(const char *v) { return v && *v ? (int)strtol(v, NULL, 10) : 0; }
static double as_double(const char *v) { return v && *v ? strtod(v, NULL) : 0.0; }

void model_init(ModelData *model) { memset(model, 0, sizeof(*model)); }
void model_free(ModelData *model) {
    if (!model) return;
    free(model->scenarios); free(model->projects); free(model->grades);
    free(model->revenue_profile); free(model->resource_profile);
    free(model->observations); free(model->wins);
    free(model->observation_index); free(model->win_index);
    model_init(model);
}
int model_find_scenario(const ModelData *model, const char *id) {
    size_t i;
    for (i=0; i<model->scenario_count; ++i)
        if (!strcmp(model->scenarios[i].id, id)) return (int)i;
    return -1;
}
int model_find_project(const ModelData *model, const char *id) {
    size_t i;
    for (i=0; i<model->project_count; ++i)
        if (!strcmp(model->projects[i].id, id)) return (int)i;
    return -1;
}

static int load_scenarios(ModelData *m, const char *path, char *err, size_t es) {
    CsvReader r; CsvRow h={0}, row={0}; int st;
    int id, group, probability, market, cost, resource, seed;
    if (!csv_open(&r,path) || csv_read_row(&r,&h)!=1) { error_set(err,es,"cannot read",path); return 0; }
    id=csv_column(&h,"scenario_id"); group=csv_column(&h,"scenario_group");
    probability=csv_column(&h,"probability"); market=csv_column(&h,"market_multiplier");
    cost=csv_column(&h,"cost_multiplier"); resource=csv_column(&h,"resource_multiplier");
    seed=csv_column(&h,"random_seed");
    if (id<0||group<0||probability<0||market<0||cost<0||resource<0||seed<0) goto bad;
    while ((st=csv_read_row(&r,&row))==1) {
        Scenario s={0};
        if (row.count < h.count) goto bad;
        snprintf(s.id,sizeof(s.id),"%s",row.fields[id]);
        snprintf(s.group,sizeof(s.group),"%s",row.fields[group]);
        s.probability=as_double(row.fields[probability]);
        s.market_multiplier=as_double(row.fields[market]);
        s.cost_multiplier=as_double(row.fields[cost]);
        s.resource_multiplier=as_double(row.fields[resource]);
        s.random_seed=(uint64_t)strtoull(row.fields[seed],NULL,10);
        if (!append((void **)&m->scenarios,&m->scenario_count,sizeof(s),&s)) goto bad;
        csv_row_free(&row);
    }
    csv_row_free(&h); csv_close(&r); return st==0;
bad:
    error_set(err,es,"invalid scenario CSV",path);
    csv_row_free(&row); csv_row_free(&h); csv_close(&r); return 0;
}

static int load_projects(ModelData *m, const char *path, ProjectType type,
                         char *err, size_t es) {
    CsvReader r; CsvRow h={0}, row={0}; int st;
    int id,duration,revenue,margin,p,p0,pmax,delta,expertise,internal_expertise;
    int workload,pcost,strategy,growth,external_revenue,stability,burden,delay;
    int start_month,earliest_start_offset;
    if (!csv_open(&r,path)||csv_read_row(&r,&h)!=1) { error_set(err,es,"cannot read",path); return 0; }
    id=csv_column(&h,"project_id"); duration=csv_column(&h,"duration");
    revenue=csv_column(&h,"revenue_total"); margin=csv_column(&h,"expected_margin");
    p=csv_column(&h,"p_estimated"); expertise=csv_column(&h,"expertise_fit");
    p0=csv_column(&h,"p0"); pmax=csv_column(&h,"p_max"); delta=csv_column(&h,"delta");
    internal_expertise=csv_column(&h,"internal_expertise_score");
    start_month=csv_column(&h,"start_month");
    earliest_start_offset=csv_column(&h,"earliest_start_offset");
    workload=csv_column(&h,"proposal_workload"); pcost=csv_column(&h,"proposal_cost");
    strategy=csv_column(&h,"strategic_score"); growth=csv_column(&h,"growth_score");
    external_revenue=csv_column(&h,"external_revenue_score");
    stability=csv_column(&h,"stability_score"); burden=csv_column(&h,"resource_burden_score");
    delay=csv_column(&h,"delay_cost");
    if (id<0||duration<0||revenue<0||margin<0) goto bad;
    while ((st=csv_read_row(&r,&row))==1) {
        Project x={0};
        if (row.count < h.count) goto bad;
        snprintf(x.id,sizeof(x.id),"%s",row.fields[id]); x.type=type;
        if(start_month>=0)x.fixed_start_month=as_int(row.fields[start_month]);
        if(earliest_start_offset>=0)x.earliest_start_offset=as_int(row.fields[earliest_start_offset]);
        x.duration=as_int(row.fields[duration]); x.revenue_total=as_double(row.fields[revenue]);
        x.expected_margin=as_double(row.fields[margin]);
        if(p>=0)x.p_estimated=as_double(row.fields[p]);
        if(p0>=0)x.p0=as_double(row.fields[p0]); if(pmax>=0)x.p_max=as_double(row.fields[pmax]);
        if(delta>=0)x.delta=as_double(row.fields[delta]);
        if(expertise>=0)x.expertise_fit=as_double(row.fields[expertise]);
        if(internal_expertise>=0)x.expertise_fit=as_double(row.fields[internal_expertise]);
        if(workload>=0)x.proposal_workload=as_double(row.fields[workload]); if(pcost>=0)x.proposal_cost=as_double(row.fields[pcost]);
        if(strategy>=0)x.strategic_score=as_double(row.fields[strategy]); if(growth>=0)x.growth_score=as_double(row.fields[growth]);
        if(external_revenue>=0)x.external_revenue_score=as_double(row.fields[external_revenue]);
        if(stability>=0)x.stability_score=as_double(row.fields[stability]); if(burden>=0)x.resource_burden_score=as_double(row.fields[burden]);
        if(delay>=0)x.delay_cost=as_double(row.fields[delay]);
        if (!append((void **)&m->projects,&m->project_count,sizeof(x),&x)) goto bad;
        csv_row_free(&row);
    }
    csv_row_free(&h); csv_close(&r); return st==0;
bad:
    error_set(err,es,"invalid project CSV",path);
    csv_row_free(&row); csv_row_free(&h); csv_close(&r); return 0;
}

static int load_grades(ModelData *m, const char *path, char *err, size_t es) {
    CsvReader r; CsvRow h={0}, row={0}; int st;
    int id,order,initial,lead,limit,internal,hiring,external,idle,shortage;
    if(!csv_open(&r,path)||csv_read_row(&r,&h)!=1){error_set(err,es,"cannot read",path);return 0;}
    id=csv_column(&h,"grade_id"); order=csv_column(&h,"grade_order");
    initial=csv_column(&h,"initial_staff"); lead=csv_column(&h,"hiring_leadtime");
    limit=csv_column(&h,"external_limit"); internal=csv_column(&h,"internal_cost");
    hiring=csv_column(&h,"hiring_cost"); external=csv_column(&h,"external_cost");
    idle=csv_column(&h,"idle_penalty"); shortage=csv_column(&h,"short_penalty");
    if(id<0||order<0||initial<0||lead<0||limit<0||internal<0||hiring<0||external<0||idle<0||shortage<0)goto bad;
    while((st=csv_read_row(&r,&row))==1){
        Grade g={0};
        if(row.count<h.count)goto bad;
        snprintf(g.id,sizeof(g.id),"%s",row.fields[id]); g.order=as_int(row.fields[order]);
        g.initial_staff=as_double(row.fields[initial]); g.hiring_leadtime=as_int(row.fields[lead]);
        g.external_limit=as_double(row.fields[limit]); g.internal_cost=as_double(row.fields[internal]);
        g.hiring_cost=as_double(row.fields[hiring]); g.external_cost=as_double(row.fields[external]);
        g.idle_penalty=as_double(row.fields[idle]); g.short_penalty=as_double(row.fields[shortage]);
        if(!append((void **)&m->grades,&m->grade_count,sizeof(g),&g))goto bad;
        csv_row_free(&row);
    }
    csv_row_free(&h); csv_close(&r); return st==0&&m->grade_count>0;
bad:
    error_set(err,es,"invalid workforce CSV",path); csv_row_free(&row); csv_row_free(&h); csv_close(&r); return 0;
}

static int model_find_grade(const ModelData *m, const char *id) {
    size_t i; for(i=0;i<m->grade_count;++i)if(!strcmp(m->grades[i].id,id))return (int)i; return -1;
}

static int load_revenue_profile(ModelData *m,const char *path,char *err,size_t es){
    CsvReader r; CsvRow h={0},row={0}; int st,id,offset,revenue;
    if(!csv_open(&r,path)||csv_read_row(&r,&h)!=1){error_set(err,es,"cannot read",path);return 0;}
    id=csv_column(&h,"project_id");offset=csv_column(&h,"start_offset");revenue=csv_column(&h,"revenue");
    if(id<0||offset<0||revenue<0)goto bad;
    while((st=csv_read_row(&r,&row))==1){int p,o;if(row.count<h.count)goto bad;p=model_find_project(m,row.fields[id]);o=as_int(row.fields[offset]);if(p<0||o<0||o>=PPSP_HORIZON)goto bad;m->revenue_profile[(size_t)p*PPSP_HORIZON+(size_t)o]=as_double(row.fields[revenue]);csv_row_free(&row);}
    csv_row_free(&h);csv_close(&r);return st==0;
bad:error_set(err,es,"invalid revenue CSV",path);csv_row_free(&row);csv_row_free(&h);csv_close(&r);return 0;
}

static int load_resource_profile(ModelData *m,const char *path,char *err,size_t es){
    CsvReader r; CsvRow h={0},row={0}; int st,id,offset,grade,required;
    if(!csv_open(&r,path)||csv_read_row(&r,&h)!=1){error_set(err,es,"cannot read",path);return 0;}
    id=csv_column(&h,"project_id");offset=csv_column(&h,"start_offset");grade=csv_column(&h,"grade_id");required=csv_column(&h,"required_staff");
    if(id<0||offset<0||grade<0||required<0)goto bad;
    while((st=csv_read_row(&r,&row))==1){int p,o,g;if(row.count<h.count)goto bad;p=model_find_project(m,row.fields[id]);o=as_int(row.fields[offset]);g=model_find_grade(m,row.fields[grade]);if(p<0||g<0||o<0||o>=PPSP_HORIZON)goto bad;m->resource_profile[((size_t)p*PPSP_HORIZON+(size_t)o)*m->grade_count+(size_t)g]=as_double(row.fields[required]);csv_row_free(&row);}
    csv_row_free(&h);csv_close(&r);return st==0;
bad:error_set(err,es,"invalid resource CSV",path);csv_row_free(&row);csv_row_free(&h);csv_close(&r);return 0;
}

static int load_named_values(ModelData *m,const char *path,int constraints,char *err,size_t es){
    CsvReader r;CsvRow h={0},row={0};int st,name,value;
    if(!csv_open(&r,path)||csv_read_row(&r,&h)!=1){error_set(err,es,"cannot read",path);return 0;}
    name=csv_column(&h,constraints?"constraint_name":"parameter");value=csv_column(&h,"value");if(name<0||value<0)goto bad;
    while((st=csv_read_row(&r,&row))==1){const char*n;double v;if(row.count<h.count)goto bad;n=row.fields[name];v=as_double(row.fields[value]);
        if(!constraints){if(!strcmp(n,"initial_marketing_staff"))m->marketing.initial_staff=v;else if(!strcmp(n,"marketing_leadtime"))m->marketing.hiring_leadtime=(int)v;else if(!strcmp(n,"proposal_capacity_per_fte"))m->marketing.proposal_capacity_per_fte=v;else if(!strcmp(n,"marketing_cost"))m->marketing.monthly_cost=v;else if(!strcmp(n,"marketing_hiring_cost"))m->marketing.hiring_cost=v;}
        else {if(!strcmp(n,"internal_ratio_limit"))m->constraints.internal_ratio_limit=v;else if(!strcmp(n,"fail_short_rate_limit"))m->constraints.fail_short_rate_limit=v;else if(!strcmp(n,"fail_consecutive_short_months"))m->constraints.fail_consecutive_short_months=(int)v;else if(!strcmp(n,"min_profit_for_success"))m->constraints.min_profit_for_success=v;else if(!strcmp(n,"external_staff_ratio_warning"))m->constraints.external_staff_ratio_warning=v;else if(!strcmp(n,"max_hiring_per_grade_month"))m->constraints.max_hiring_per_grade_month=v;else if(!strcmp(n,"marketing_capacity_soft_buffer"))m->constraints.marketing_capacity_soft_buffer=v;else if(!strcmp(n,"epsilon_tie_break"))m->constraints.epsilon_tie_break=v;}
        csv_row_free(&row);
    }
    csv_row_free(&h);csv_close(&r);return st==0;
bad:error_set(err,es,"invalid parameter CSV",path);csv_row_free(&row);csv_row_free(&h);csv_close(&r);return 0;
}

static int load_observations(ModelData *m, const char *path, char *err, size_t es) {
    CsvReader r; CsvRow h={0}, row={0}; int st;
    int sid,pid,observed,bid,win,start_from,start_to;
    if (!csv_open(&r,path)||csv_read_row(&r,&h)!=1) { error_set(err,es,"cannot read",path); return 0; }
    sid=csv_column(&h,"scenario_id"); pid=csv_column(&h,"project_id");
    observed=csv_column(&h,"observed_month"); bid=csv_column(&h,"bid_deadline_month");
    win=csv_column(&h,"win_confirm_month"); start_from=csv_column(&h,"start_window_from");
    start_to=csv_column(&h,"start_window_to");
    if(sid<0||pid<0||observed<0||bid<0||win<0||start_from<0||start_to<0) goto bad;
    while ((st=csv_read_row(&r,&row))==1) {
        Observation o={0};
        if (row.count < h.count) goto bad;
        o.scenario=model_find_scenario(m,row.fields[sid]);
        o.project=model_find_project(m,row.fields[pid]);
        o.observed_month=as_int(row.fields[observed]);
        o.bid_deadline_month=as_int(row.fields[bid]);
        o.win_confirm_month=as_int(row.fields[win]);
        o.start_window_from=as_int(row.fields[start_from]);
        o.start_window_to=as_int(row.fields[start_to]);
        if(o.scenario<0||o.project<0||!append((void **)&m->observations,&m->observation_count,sizeof(o),&o)) goto bad;
        m->observation_index[(size_t)o.scenario*m->project_count+(size_t)o.project]=(int)m->observation_count-1;
        csv_row_free(&row);
    }
    csv_row_free(&h); csv_close(&r); return st==0;
bad:
    error_set(err,es,"invalid observation CSV",path);
    csv_row_free(&row); csv_row_free(&h); csv_close(&r); return 0;
}

static int load_wins(ModelData *m, const char *path, char *err, size_t es) {
    CsvReader r; CsvRow h={0}, row={0}; int st;
    int sid,pid,result,month,prob,random_value;
    if (!csv_open(&r,path)||csv_read_row(&r,&h)!=1) { error_set(err,es,"cannot read",path); return 0; }
    sid=csv_column(&h,"scenario_id"); pid=csv_column(&h,"project_id");
    result=csv_column(&h,"win_result"); month=csv_column(&h,"win_confirm_month");
    prob=csv_column(&h,"scenario_win_prob");
    random_value=csv_column(&h,"random_value");
    if(sid<0||pid<0||result<0||month<0||prob<0) goto bad;
    while ((st=csv_read_row(&r,&row))==1) {
        WinResult w={0};
        if (row.count < h.count) goto bad;
        w.scenario=model_find_scenario(m,row.fields[sid]);
        w.project=model_find_project(m,row.fields[pid]);
        w.win_result=as_int(row.fields[result]);
        w.win_confirm_month=as_int(row.fields[month]);
        w.scenario_win_prob=as_double(row.fields[prob]);
        if(random_value>=0)w.random_value=as_double(row.fields[random_value]);
        if(w.scenario<0||w.project<0||!append((void **)&m->wins,&m->win_count,sizeof(w),&w)) goto bad;
        m->win_index[(size_t)w.scenario*m->project_count+(size_t)w.project]=(int)m->win_count-1;
        csv_row_free(&row);
    }
    csv_row_free(&h); csv_close(&r); return st==0;
bad:
    error_set(err,es,"invalid win CSV",path);
    csv_row_free(&row); csv_row_free(&h); csv_close(&r); return 0;
}

int model_load_information_data(ModelData *m, const char *dir,
                                char *err, size_t es) {
    char path[1024];
    model_init(m);
    path_make(path,sizeof(path),dir,"02_SCENARIOS.csv");
    if(!load_scenarios(m,path,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"03_FIXED_PROJECTS.csv");
    if(!load_projects(m,path,PROJECT_FIXED,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"04_INTERNAL_CANDIDATES.csv");
    if(!load_projects(m,path,PROJECT_INTERNAL,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"05_EXTERNAL_CANDIDATES.csv");
    if(!load_projects(m,path,PROJECT_EXTERNAL,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"08_WORKFORCE.csv");
    if(!load_grades(m,path,err,es)) goto fail;
    m->revenue_profile=(double *)calloc(m->project_count*PPSP_HORIZON,sizeof(double));
    m->resource_profile=(double *)calloc(m->project_count*PPSP_HORIZON*m->grade_count,sizeof(double));
    if(!m->revenue_profile||!m->resource_profile){error_set(err,es,"out of memory",dir);goto fail;}
    path_make(path,sizeof(path),dir,"06_PROJECT_REVENUE.csv");
    if(!load_revenue_profile(m,path,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"07_PROJECT_RESOURCE.csv");
    if(!load_resource_profile(m,path,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"09_MARKETING.csv");
    if(!load_named_values(m,path,0,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"13_CONSTRAINTS.csv");
    if(!load_named_values(m,path,1,err,es)) goto fail;
    {
        size_t i, count=m->scenario_count*m->project_count;
        m->observation_index=(int *)malloc(count*sizeof(int));
        m->win_index=(int *)malloc(count*sizeof(int));
        if(!m->observation_index||!m->win_index){error_set(err,es,"out of memory",dir);goto fail;}
        for(i=0;i<count;++i){m->observation_index[i]=-1;m->win_index[i]=-1;}
    }
    path_make(path,sizeof(path),dir,"10_SCENARIO_OBSERVATIONS.csv");
    if(!load_observations(m,path,err,es)) goto fail;
    path_make(path,sizeof(path),dir,"11_EXTERNAL_WIN_RESULTS.csv");
    if(!load_wins(m,path,err,es)) goto fail;
    return 1;
fail:
    model_free(m); return 0;
}

double model_project_revenue(const ModelData *m,int project,int start_offset){
    if(!m||!m->revenue_profile||project<0||(size_t)project>=m->project_count||start_offset<0||start_offset>=PPSP_HORIZON)return 0.0;
    return m->revenue_profile[(size_t)project*PPSP_HORIZON+(size_t)start_offset];
}

double model_project_resource(const ModelData *m,int project,int start_offset,int grade){
    if(!m||!m->resource_profile||project<0||(size_t)project>=m->project_count||start_offset<0||start_offset>=PPSP_HORIZON||grade<0||(size_t)grade>=m->grade_count)return 0.0;
    return m->resource_profile[((size_t)project*PPSP_HORIZON+(size_t)start_offset)*m->grade_count+(size_t)grade];
}

const Observation *model_observation(const ModelData *m, int scenario, int project) {
    int index;
    if(!m||scenario<0||project<0||(size_t)scenario>=m->scenario_count||(size_t)project>=m->project_count)return NULL;
    if(!m->observation_index){size_t i;for(i=0;i<m->observation_count;++i)if(m->observations[i].scenario==scenario&&m->observations[i].project==project)return &m->observations[i];return NULL;}
    index=m->observation_index[(size_t)scenario*m->project_count+(size_t)project];
    return index>=0?&m->observations[index]:NULL;
}
const WinResult *model_win(const ModelData *m, int scenario, int project) {
    int index;
    if(!m||scenario<0||project<0||(size_t)scenario>=m->scenario_count||(size_t)project>=m->project_count)return NULL;
    if(!m->win_index){size_t i;for(i=0;i<m->win_count;++i)if(m->wins[i].scenario==scenario&&m->wins[i].project==project)return &m->wins[i];return NULL;}
    index=m->win_index[(size_t)scenario*m->project_count+(size_t)project];
    return index>=0?&m->wins[index]:NULL;
}

static uint64_t splitmix64(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

int model_win_for_replication(const ModelData *m,int scenario,int project,
                              int replication,WinResult *out){
    const WinResult *base;
    uint64_t key, bits;
    double random_value;
    if(!m||!out||replication<1||scenario<0||project<0||
       (size_t)scenario>=m->scenario_count||(size_t)project>=m->project_count)return 0;
    base=model_win(m,scenario,project); if(!base)return 0;
    key=m->scenarios[scenario].random_seed;
    key^=UINT64_C(0xd1b54a32d192ed03)*(uint64_t)replication;
    key^=UINT64_C(0x94d049bb133111eb)*(uint64_t)(project+1);
    bits=splitmix64(key);
    random_value=(double)(bits>>11)*(1.0/9007199254740992.0);
    *out=*base; out->random_value=random_value;
    out->win_result=random_value<base->scenario_win_prob;
    return 1;
}
