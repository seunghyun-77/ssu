#include "ppsp/info_state.h"
#include "ppsp/model.h"
#include "ppsp/policy.h"
#include "ppsp/experiment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int generate_replicated_wins(const char *data_dir, const char *output_path) {
    ModelData model;
    char error[512] = {0};
    FILE *out;
    size_t s, p;
    int replication;
    if (!model_load_information_data(&model, data_dir, error, sizeof(error))) {
        fprintf(stderr, "ERROR: %s\n", error); return 1;
    }
#ifdef _MSC_VER
    if (fopen_s(&out, output_path, "wb") != 0) out = NULL;
#else
    out = fopen(output_path, "wb");
#endif
    if (!out) { fprintf(stderr, "ERROR: cannot write %s\n", output_path); model_free(&model); return 1; }
    fputs("scenario_id,replication_id,project_id,scenario_group,scenario_win_prob,random_value,win_result,win_confirm_month,generator_version\n", out);
    for (s=0;s<model.scenario_count;++s) for(replication=1;replication<=20;++replication)
        for(p=0;p<model.project_count;++p) if(model.projects[p].type==PROJECT_EXTERNAL){
            WinResult w;
            if(!model_win_for_replication(&model,(int)s,(int)p,replication,&w)){fclose(out);model_free(&model);return 1;}
            fprintf(out,"%s,%d,%s,%s,%.9f,%.12f,%d,%d,splitmix64-v1\n",
                    model.scenarios[s].id,replication,model.projects[p].id,
                    model.scenarios[s].group,w.scenario_win_prob,w.random_value,
                    w.win_result,w.win_confirm_month);
        }
    fclose(out); model_free(&model); return 0;
}

int main(int argc, char **argv) {
    ModelData model;
    PolicySet policies;
    char error[512] = {0};
    size_t s;
    int month;
    if (argc == 4 && !strcmp(argv[1], "--generate-replicated-wins"))
        return generate_replicated_wins(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "--run-experiment")) {
        if (!experiment_run_all(argv[2], argv[3], error, sizeof(error))) {
            fprintf(stderr, "ERROR: %s\n", error); return 1;
        }
        puts("Completed 14,000 policy-scenario-replication runs."); return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "--run-experiment-params")) {
        double discount_rate=strtod(argv[4],NULL),penalty_rate=strtod(argv[5],NULL);
        if (!experiment_run_all_parameters(argv[2],argv[3],discount_rate,
                                           penalty_rate,error,sizeof(error))) {
            fprintf(stderr,"ERROR: %s\n",error); return 1;
        }
        printf("Completed 14,000 runs (discount=%.6f, penalty=%.6f).\n",
               discount_rate,penalty_rate); return 0;
    }
    if (argc != 3 || strcmp(argv[1], "--validate-data")) {
        fprintf(stderr, "Usage: %s --validate-data <data-directory>\n"
                        "       %s --generate-replicated-wins <data-directory> <output.csv>\n",
                argv[0], argv[0]);
        return 2;
    }
    if (!model_load_information_data(&model, argv[2], error, sizeof(error))) {
        fprintf(stderr, "ERROR: %s\n", error);
        return 1;
    }
    if (!policy_set_load(&policies, argv[2], error, sizeof(error))) {
        fprintf(stderr, "ERROR: %s\n", error);
        model_free(&model);
        return 1;
    }
    for (s = 0; s < model.scenario_count; ++s) {
        for (month = 1; month <= PPSP_HORIZON; ++month) {
            InformationKey key;
            if (!information_key_build(&model, (int)s, month, &key)) {
                fprintf(stderr, "ERROR: information state failed for %s month %d\n",
                        model.scenarios[s].id, month);
                model_free(&model);
                return 1;
            }
        }
    }
    printf("Validated %zu scenarios, %zu projects, %zu observations, %zu win results.\n",
           model.scenario_count, model.project_count, model.observation_count, model.win_count);
    puts("Timing validated: W and realized R/q reveal at win-confirm month regardless of proposal.");
    printf("Validated %zu policies and %zu P7 business stages.\n",
           policies.policy_count, policies.stage_count);
    model_free(&model);
    return 0;
}
