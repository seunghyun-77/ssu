#ifndef PPSP_SIMULATION_H
#define PPSP_SIMULATION_H

#include "ppsp/model.h"

typedef struct {
    int month;
    double revenue;
    double internal_revenue;
    double external_revenue;
    double cost;
    double profit;
    double cumulative_profit;
    double demand;
    double internal_staff;
    double external_staff;
    double idle;
    double shortage;
    double internal_ratio;
    double discount_factor;
    double internal_ratio_penalty;
    double adjusted_profit;
    double cumulative_adjusted_profit;
} MonthlyMetrics;

typedef struct {
    double demand, internal_staff, external_staff, idle, shortage, utilization;
} GradeMonthlyMetrics;

typedef struct {
    const ModelData *model;
    int scenario;
    int replication;
    int month;
    int *proposal_month;
    int *start_month;
    double *internal_staff;
    double *grade_hiring_pipeline;
    double marketing_staff;
    double marketing_hiring_pipeline[PPSP_HORIZON + 1];
    double proposal_workload_used;
    double pending_month_cost;
    double cumulative_profit;
    double cumulative_adjusted_profit;
    double annual_discount_rate;
    double internal_ratio_penalty_rate;
    double annual_internal_revenue[5];
    double annual_total_revenue[5];
} SimulationState;

int simulation_init(SimulationState *state, const ModelData *model, int scenario);
int simulation_init_run(SimulationState *state, const ModelData *model, int scenario,
                        int replication);
void simulation_set_economics(SimulationState *state, double annual_discount_rate,
                              double internal_ratio_penalty_rate);
void simulation_free(SimulationState *state);
int simulation_begin_month(SimulationState *state, int month);
int simulation_hire_grade(SimulationState *state, int grade, double amount);
int simulation_hire_marketing(SimulationState *state, double amount);
int simulation_can_propose(const SimulationState *state, int project);
int simulation_propose(SimulationState *state, int project);
int simulation_can_start(const SimulationState *state, int project);
int simulation_start(SimulationState *state, int project);
int simulation_finish_month(SimulationState *state, const double *external_requested,
                            MonthlyMetrics *metrics);
int simulation_finish_month_detailed(SimulationState *state,
                                     const double *external_requested,
                                     MonthlyMetrics *metrics,
                                     GradeMonthlyMetrics *grade_metrics);
double simulation_grade_demand(const SimulationState *state, int grade, int month);

#endif
