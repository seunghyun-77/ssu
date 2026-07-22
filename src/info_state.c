#include "ppsp/info_state.h"

#include <string.h>

#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)

static void hash_bytes(uint64_t *hash, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; ++i) { *hash ^= p[i]; *hash *= FNV_PRIME; }
}
static void hash_int(uint64_t *hash, int value) { hash_bytes(hash, &value, sizeof(value)); }
static void hash_string(uint64_t *hash, const char *value) { hash_bytes(hash, value, strlen(value) + 1); }

int information_is_project_observed(const ModelData *model, int scenario,
                                    int project, int month) {
    const Observation *o = model_observation(model, scenario, project);
    return o && o->observed_month <= month;
}

int information_is_external_profile_revealed(const ModelData *model, int scenario,
                                              int project, int month) {
    const Observation *o;
    if (model->projects[project].type != PROJECT_EXTERNAL) return 0;
    o = model_observation(model, scenario, project);
    return o && o->win_confirm_month > 0 && o->win_confirm_month <= month;
}

int information_key_build_replication(const ModelData *model, int scenario,
                                      int replication, int month,
                                      InformationKey *key) {
    size_t p;
    uint64_t hash = FNV_OFFSET;
    if (!model || !key || scenario < 0 || (size_t)scenario >= model->scenario_count ||
        month < 1 || month > PPSP_HORIZON) return 0;
    memset(key, 0, sizeof(*key));
    key->month = month;
    hash_int(&hash, month);
    /* Scenario-wide cost conditions are public before monthly decisions. */
    hash_bytes(&hash, &model->scenarios[scenario].cost_multiplier, sizeof(double));
    for (p = 0; p < model->project_count; ++p) {
        const Observation *o = model_observation(model, scenario, (int)p);
        if (!o || o->observed_month > month) continue;
        ++key->observed_project_count;
        hash_string(&hash, model->projects[p].id);
        hash_int(&hash, o->observed_month);
        hash_int(&hash, o->bid_deadline_month);
        hash_int(&hash, o->start_window_from);
        hash_int(&hash, o->start_window_to);
        if (model->projects[p].type == PROJECT_EXTERNAL && o->win_confirm_month <= month) {
            WinResult generated;
            const WinResult *w = model_win(model, scenario, (int)p);
            if (replication > 0) {
                if (!model_win_for_replication(model, scenario, (int)p,
                                               replication, &generated)) return 0;
                w = &generated;
            }
            if (!w) return 0;
            ++key->revealed_external_count;
            hash_int(&hash, o->win_confirm_month);
            hash_int(&hash, w->win_result);
            /* Dataset mapping for revealed scenario-specific R and q profiles. */
            hash_bytes(&hash, &model->scenarios[scenario].market_multiplier, sizeof(double));
            hash_bytes(&hash, &model->scenarios[scenario].resource_multiplier, sizeof(double));
        }
    }
    key->hash = hash;
    return 1;
}

int information_key_build(const ModelData *model, int scenario, int month,
                          InformationKey *key) {
    return information_key_build_replication(model, scenario, 0, month, key);
}

int information_key_equal(const InformationKey *left, const InformationKey *right) {
    return left && right && left->month == right->month && left->hash == right->hash &&
           left->observed_project_count == right->observed_project_count &&
           left->revealed_external_count == right->revealed_external_count;
}
