#ifndef PPSP_POLICY_H
#define PPSP_POLICY_H

#include "ppsp/model.h"

#define PPSP_POLICY_COUNT 10
#define PPSP_POLICY_NAME_LEN 128

typedef struct {
    char id[PPSP_ID_LEN];
    char name_ko[PPSP_POLICY_NAME_LEN];
    char name_en[PPSP_POLICY_NAME_LEN];
    double profit_weight, win_prob_weight, expertise_weight, strategy_weight;
    double growth_weight, stability_weight, proposal_workload_weight;
    double resource_burden_weight, bottleneck_weight, delay_risk_weight;
    double internal_ratio_weight;
    double proposal_threshold, start_threshold;
    int max_proposal_per_month, max_start_per_month;
    double resource_guard_level;
    char external_staff_mode[16];
    double hiring_aggressiveness, internal_ratio_guard;
} PolicyConfig;

typedef struct {
    int from_month, to_month;
    PolicyConfig override_values;
} PolicyStage;

typedef struct {
    PolicyConfig policies[PPSP_POLICY_COUNT];
    size_t policy_count;
    PolicyStage stages[4];
    size_t stage_count;
} PolicySet;

int policy_set_load(PolicySet *set, const char *data_dir, char *error,
                    size_t error_size);
const PolicyConfig *policy_find(const PolicySet *set, const char *id);
PolicyConfig policy_for_month(const PolicySet *set, const PolicyConfig *base,
                              int month);

#endif
