#include "ppsp/simulation.h"
#include "ppsp/info_state.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static double minimum(double a, double b) { return a < b ? a : b; }
static double maximum(double a, double b) { return a > b ? a : b; }

int simulation_init_run(SimulationState *s, const ModelData *m, int scenario,
                        int replication) {
    size_t p, g;
    if (!s || !m || scenario < 0 || (size_t)scenario >= m->scenario_count ||
        !m->grade_count || replication < 1) return 0;
    memset(s, 0, sizeof(*s));
    s->model = m;
    s->scenario = scenario;
    s->replication = replication;
    s->proposal_month = (int *)calloc(m->project_count, sizeof(int));
    s->start_month = (int *)calloc(m->project_count, sizeof(int));
    s->internal_staff = (double *)calloc(m->grade_count, sizeof(double));
    s->grade_hiring_pipeline = (double *)calloc(
        m->grade_count * (PPSP_HORIZON + 1), sizeof(double));
    if (!s->proposal_month || !s->start_month || !s->internal_staff ||
        !s->grade_hiring_pipeline) {
        simulation_free(s);
        return 0;
    }
    for (g = 0; g < m->grade_count; ++g)
        s->internal_staff[g] = m->grades[g].initial_staff;
    s->marketing_staff = m->marketing.initial_staff;
    for (p = 0; p < m->project_count; ++p)
        if (m->projects[p].type == PROJECT_FIXED)
            s->start_month[p] = m->projects[p].fixed_start_month;
    return 1;
}

int simulation_init(SimulationState *s, const ModelData *m, int scenario) {
    return simulation_init_run(s, m, scenario, 1);
}

void simulation_set_economics(SimulationState *s,double annual_discount_rate,
                              double internal_ratio_penalty_rate){
    if(!s)return;s->annual_discount_rate=maximum(annual_discount_rate,0.0);
    s->internal_ratio_penalty_rate=maximum(internal_ratio_penalty_rate,0.0);
}

void simulation_free(SimulationState *s) {
    if (!s) return;
    free(s->proposal_month);
    free(s->start_month);
    free(s->internal_staff);
    free(s->grade_hiring_pipeline);
    memset(s, 0, sizeof(*s));
}

int simulation_begin_month(SimulationState *s, int month) {
    size_t g;
    if (!s || !s->model || month != s->month + 1 || month < 1 ||
        month > PPSP_HORIZON) return 0;
    s->month = month;
    s->proposal_workload_used = 0.0;
    s->pending_month_cost = 0.0;
    for (g = 0; g < s->model->grade_count; ++g)
        s->internal_staff[g] += s->grade_hiring_pipeline[
            g * (PPSP_HORIZON + 1) + (size_t)month];
    s->marketing_staff += s->marketing_hiring_pipeline[month];
    return 1;
}

int simulation_hire_grade(SimulationState *s, int grade, double amount) {
    int available_month;
    const Grade *g;
    if (!s || !s->model || grade < 0 || (size_t)grade >= s->model->grade_count ||
        amount < 0.0 || amount > s->model->constraints.max_hiring_per_grade_month)
        return 0;
    g = &s->model->grades[grade];
    available_month = s->month + g->hiring_leadtime;
    if (available_month <= PPSP_HORIZON)
        s->grade_hiring_pipeline[(size_t)grade * (PPSP_HORIZON + 1) +
                                 (size_t)available_month] += amount;
    s->pending_month_cost += amount * g->hiring_cost;
    return 1;
}

int simulation_hire_marketing(SimulationState *s, double amount) {
    int available_month;
    if (!s || !s->model || amount < 0.0) return 0;
    available_month = s->month + s->model->marketing.hiring_leadtime;
    if (available_month <= PPSP_HORIZON)
        s->marketing_hiring_pipeline[available_month] += amount;
    s->pending_month_cost += amount * s->model->marketing.hiring_cost;
    return 1;
}

int simulation_can_propose(const SimulationState *s, int project) {
    const ModelData *m;
    const Observation *o;
    double capacity;
    if (!s || !(m = s->model) || project < 0 ||
        (size_t)project >= m->project_count ||
        m->projects[project].type != PROJECT_EXTERNAL ||
        s->proposal_month[project] != 0) return 0;
    o = model_observation(m, s->scenario, project);
    if (!o || s->month < o->observed_month || s->month > o->bid_deadline_month)
        return 0;
    capacity = s->marketing_staff * m->marketing.proposal_capacity_per_fte;
    return s->proposal_workload_used + m->projects[project].proposal_workload <= capacity;
}

int simulation_propose(SimulationState *s, int project) {
    if (!simulation_can_propose(s, project)) return 0;
    s->proposal_month[project] = s->month;
    s->proposal_workload_used += s->model->projects[project].proposal_workload;
    s->pending_month_cost += s->model->projects[project].proposal_cost;
    return 1;
}

int simulation_can_start(const SimulationState *s, int project) {
    const ModelData *m;
    const Observation *o;
    if (!s || !(m = s->model) || project < 0 ||
        (size_t)project >= m->project_count || s->start_month[project] != 0 ||
        m->projects[project].type == PROJECT_FIXED) return 0;
    o = model_observation(m, s->scenario, project);
    if (!o || s->month < o->start_window_from || s->month > o->start_window_to ||
        !information_is_project_observed(m, s->scenario, project, s->month)) return 0;
    if (m->projects[project].type == PROJECT_EXTERNAL) {
        WinResult generated;
        const WinResult *w = NULL;
        if (model_win_for_replication(m, s->scenario, project, s->replication,
                                      &generated)) w = &generated;
        if (!s->proposal_month[project] || s->proposal_month[project] >= o->win_confirm_month ||
            s->month < o->win_confirm_month || !w || !w->win_result) return 0;
    }
    return 1;
}

int simulation_start(SimulationState *s, int project) {
    const Observation *o;
    if (!simulation_can_start(s, project)) return 0;
    s->start_month[project] = s->month;
    o = model_observation(s->model, s->scenario, project);
    if (o && s->month > o->start_window_from)
        s->pending_month_cost += (s->month - o->start_window_from) *
                                 s->model->projects[project].delay_cost;
    return 1;
}

double simulation_grade_demand(const SimulationState *s,int grade,int month){
    const ModelData*m;size_t p;double demand=0.0;if(!s||(m=s->model)==NULL||grade<0||(size_t)grade>=m->grade_count||month<1||month>PPSP_HORIZON)return 0.0;
    for(p=0;p<m->project_count;++p){int offset;double value;if(!s->start_month[p]||month<s->start_month[p])continue;offset=month-s->start_month[p];value=model_project_resource(m,(int)p,offset,grade);if(m->projects[p].type==PROJECT_EXTERNAL)value*=m->scenarios[s->scenario].resource_multiplier;demand+=value;}return demand;
}

int simulation_finish_month_detailed(SimulationState *s,const double *external_requested,
                            MonthlyMetrics *out,GradeMonthlyMetrics *details) {
    const ModelData *m;
    size_t p, g;
    double revenue = 0.0, internal_revenue = 0.0, external_revenue = 0.0;
    double cost, demand_total = 0.0, staff_total = 0.0, external_total = 0.0;
    double idle_total = 0.0, short_total = 0.0;
    if (!s || !(m = s->model) || !out || s->month < 1) return 0;
    memset(out, 0, sizeof(*out));
    for (p = 0; p < m->project_count; ++p) {
        int offset;
        double value;
        if (!s->start_month[p] || s->month < s->start_month[p]) continue;
        offset = s->month - s->start_month[p];
        value = model_project_revenue(m, (int)p, offset);
        if (m->projects[p].type == PROJECT_EXTERNAL)
            value *= m->scenarios[s->scenario].market_multiplier;
        revenue += value;
        if (m->projects[p].type == PROJECT_EXTERNAL) external_revenue += value;
        else internal_revenue += value;
    }
    cost = s->pending_month_cost + s->marketing_staff * m->marketing.monthly_cost;
    for (g = 0; g < m->grade_count; ++g) {
        double demand = 0.0, requested = external_requested ? external_requested[g] : 0.0;
        double external, supply, idle, shortage;
        demand = simulation_grade_demand(s, (int)g, s->month);
        external = minimum(maximum(requested, 0.0), m->grades[g].external_limit);
        supply = s->internal_staff[g] + external;
        idle = maximum(supply - demand, 0.0);
        shortage = maximum(demand - supply, 0.0);
        cost += s->internal_staff[g] * m->grades[g].internal_cost;
        cost += external * m->grades[g].external_cost;
        cost += idle * m->grades[g].idle_penalty;
        cost += shortage * m->grades[g].short_penalty;
        demand_total += demand; staff_total += s->internal_staff[g];
        external_total += external; idle_total += idle; short_total += shortage;
        if(details){details[g].demand=demand;details[g].internal_staff=s->internal_staff[g];details[g].external_staff=external;details[g].idle=idle;details[g].shortage=shortage;details[g].utilization=supply>0.0?minimum(demand,supply)/supply:0.0;}
    }
    cost *= m->scenarios[s->scenario].cost_multiplier;
    out->month = s->month; out->revenue = revenue;
    out->internal_revenue = internal_revenue; out->external_revenue = external_revenue;
    out->cost = cost; out->profit = revenue - cost;
    s->cumulative_profit += out->profit; out->cumulative_profit = s->cumulative_profit;
    out->demand = demand_total; out->internal_staff = staff_total;
    out->external_staff = external_total; out->idle = idle_total;
    out->shortage = short_total;
    {
        int year=(s->month-1)/12;
        s->annual_internal_revenue[year]+=internal_revenue;
        s->annual_total_revenue[year]+=revenue;
        out->internal_ratio=s->annual_total_revenue[year]>0.0?
            s->annual_internal_revenue[year]/s->annual_total_revenue[year]:0.0;
        if(s->month%12==0){double excess=s->annual_internal_revenue[year]-m->constraints.internal_ratio_limit*s->annual_total_revenue[year];if(excess>0.0)out->internal_ratio_penalty=excess*s->internal_ratio_penalty_rate;}
    }
    out->discount_factor=pow(1.0+s->annual_discount_rate,-(double)(s->month-1)/12.0);
    out->adjusted_profit=out->discount_factor*(out->profit-out->internal_ratio_penalty);
    s->cumulative_adjusted_profit+=out->adjusted_profit;
    out->cumulative_adjusted_profit=s->cumulative_adjusted_profit;
    return 1;
}

int simulation_finish_month(SimulationState *s,const double *external_requested,MonthlyMetrics*out){return simulation_finish_month_detailed(s,external_requested,out,NULL);}
