#include "ppsp/info_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ModelData m; InformationKey a,b; WinResult r1,r1_again,r2;
    model_init(&m);
    m.scenario_count=2; m.scenarios=calloc(2,sizeof(*m.scenarios));
    snprintf(m.scenarios[0].id,sizeof(m.scenarios[0].id),"S1"); snprintf(m.scenarios[1].id,sizeof(m.scenarios[1].id),"S2");
    m.scenarios[0].market_multiplier=m.scenarios[1].market_multiplier=1.0;
    m.scenarios[0].resource_multiplier=m.scenarios[1].resource_multiplier=1.0;
    m.scenarios[0].cost_multiplier=m.scenarios[1].cost_multiplier=1.0;
    m.project_count=1; m.projects=calloc(1,sizeof(*m.projects));
    snprintf(m.projects[0].id,sizeof(m.projects[0].id),"E1"); m.projects[0].type=PROJECT_EXTERNAL;
    m.observation_count=2; m.observations=calloc(2,sizeof(*m.observations));
    m.observations[0]=(Observation){0,0,2,3,4,4,8};
    m.observations[1]=(Observation){1,0,2,3,4,4,8};
    m.win_count=2; m.wins=calloc(2,sizeof(*m.wins));
    m.wins[0]=(WinResult){0,0,0,4,0.5}; m.wins[1]=(WinResult){1,0,1,4,0.5};
    m.scenarios[0].random_seed=12345;
    assert(model_win_for_replication(&m,0,0,1,&r1));
    assert(model_win_for_replication(&m,0,0,1,&r1_again));
    assert(model_win_for_replication(&m,0,0,2,&r2));
    assert(r1.random_value==r1_again.random_value);
    assert(r1.win_result==r1_again.win_result);
    assert(r1.random_value!=r2.random_value);
    assert(information_key_build(&m,0,1,&a)&&information_key_build(&m,1,1,&b));
    assert(information_key_equal(&a,&b));
    assert(information_key_build(&m,0,3,&a)&&information_key_build(&m,1,3,&b));
    assert(information_key_equal(&a,&b));
    assert(information_key_build(&m,0,4,&a)&&information_key_build(&m,1,4,&b));
    assert(!information_key_equal(&a,&b));
    m.scenarios[1].cost_multiplier=1.1;
    assert(information_key_build(&m,0,1,&a)&&information_key_build(&m,1,1,&b));
    assert(!information_key_equal(&a,&b));
    assert(!information_is_external_profile_revealed(&m,0,0,3));
    assert(information_is_external_profile_revealed(&m,0,0,4));
    model_free(&m); puts("info_state tests passed"); return 0;
}
