#ifndef PPSP_MODEL_H
#define PPSP_MODEL_H

#include <stddef.h>
#include <stdint.h>

#define PPSP_ID_LEN 16
#define PPSP_GROUP_LEN 32
#define PPSP_HORIZON 60
#define PPSP_MAX_GRADES 16

typedef enum {
    PROJECT_FIXED = 0,
    PROJECT_INTERNAL = 1,
    PROJECT_EXTERNAL = 2
} ProjectType;

typedef struct {
    char id[PPSP_ID_LEN];
    char group[PPSP_GROUP_LEN];
    double probability;
    double market_multiplier;
    double cost_multiplier;
    double resource_multiplier;
    uint64_t random_seed;
} Scenario;

typedef struct {
    char id[PPSP_ID_LEN];
    ProjectType type;
    int fixed_start_month;
    int earliest_start_offset;
    int duration;
    double revenue_total;
    double expected_margin;
    double p_estimated;
    double p0;
    double p_max;
    double delta;
    double expertise_fit;
    double proposal_workload;
    double proposal_cost;
    double strategic_score;
    double growth_score;
    double external_revenue_score;
    double stability_score;
    double resource_burden_score;
    double delay_cost;
} Project;

typedef struct {
    char id[PPSP_ID_LEN];
    int order;
    double initial_staff;
    int hiring_leadtime;
    double external_limit;
    double internal_cost;
    double hiring_cost;
    double external_cost;
    double idle_penalty;
    double short_penalty;
} Grade;

typedef struct {
    double initial_staff;
    int hiring_leadtime;
    double proposal_capacity_per_fte;
    double monthly_cost;
    double hiring_cost;
} MarketingSettings;

typedef struct {
    double internal_ratio_limit;
    double fail_short_rate_limit;
    int fail_consecutive_short_months;
    double min_profit_for_success;
    double external_staff_ratio_warning;
    double max_hiring_per_grade_month;
    double marketing_capacity_soft_buffer;
    double epsilon_tie_break;
} ConstraintSettings;

typedef struct {
    int scenario;
    int project;
    int observed_month;
    int bid_deadline_month;
    int win_confirm_month;
    int start_window_from;
    int start_window_to;
} Observation;

typedef struct {
    int scenario;
    int project;
    int win_result;
    int win_confirm_month;
    double scenario_win_prob;
    double random_value;
} WinResult;

typedef struct {
    Scenario *scenarios;
    size_t scenario_count;
    Project *projects;
    size_t project_count;
    Grade *grades;
    size_t grade_count;
    double *revenue_profile;
    double *resource_profile;
    MarketingSettings marketing;
    ConstraintSettings constraints;
    Observation *observations;
    size_t observation_count;
    WinResult *wins;
    size_t win_count;
    int *observation_index;
    int *win_index;
} ModelData;

void model_init(ModelData *model);
void model_free(ModelData *model);
int model_load_information_data(ModelData *model, const char *data_dir,
                                char *error, size_t error_size);
int model_find_scenario(const ModelData *model, const char *id);
int model_find_project(const ModelData *model, const char *id);
const Observation *model_observation(const ModelData *model, int scenario, int project);
const WinResult *model_win(const ModelData *model, int scenario, int project);
int model_win_for_replication(const ModelData *model, int scenario, int project,
                              int replication, WinResult *result);
double model_project_revenue(const ModelData *model, int project, int start_offset);
double model_project_resource(const ModelData *model, int project, int start_offset,
                              int grade);

#endif
