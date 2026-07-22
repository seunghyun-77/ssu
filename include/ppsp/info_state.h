#ifndef PPSP_INFO_STATE_H
#define PPSP_INFO_STATE_H

#include "ppsp/model.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int month;
    uint64_t hash;
    size_t observed_project_count;
    size_t revealed_external_count;
} InformationKey;

/*
 * Chapter 3, section 3.2.5 timing:
 *   reveal candidates and public parameters -> reveal W/R/q whose reveal
 *   month has arrived -> update information state -> make decisions.
 *
 * Potential W is public at its reveal month regardless of proposal history.
 * Proposal history determines contractual obligation, not Gamma_t grouping.
 */
int information_key_build(const ModelData *model, int scenario, int month,
                          InformationKey *key);
int information_key_build_replication(const ModelData *model, int scenario,
                                      int replication, int month,
                                      InformationKey *key);
int information_key_equal(const InformationKey *left, const InformationKey *right);
int information_is_project_observed(const ModelData *model, int scenario,
                                    int project, int month);
int information_is_external_profile_revealed(const ModelData *model, int scenario,
                                              int project, int month);

#endif
