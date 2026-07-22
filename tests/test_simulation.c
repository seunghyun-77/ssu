#include "ppsp/simulation.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ModelData m;
    SimulationState s;
    MonthlyMetrics metrics;
    double external[1] = {0.0};
    model_init(&m);
    m.scenario_count=1; m.scenarios=calloc(1,sizeof(*m.scenarios));
    m.project_count=1; m.projects=calloc(1,sizeof(*m.projects));
    m.grade_count=1; m.grades=calloc(1,sizeof(*m.grades));
    m.observation_count=1; m.observations=calloc(1,sizeof(*m.observations));
    m.win_count=1; m.wins=calloc(1,sizeof(*m.wins));
    m.revenue_profile=calloc(PPSP_HORIZON,sizeof(double));
    m.resource_profile=calloc(PPSP_HORIZON,sizeof(double));
    assert(m.scenarios&&m.projects&&m.grades&&m.observations&&m.wins&&m.revenue_profile&&m.resource_profile);
    snprintf(m.projects[0].id,sizeof(m.projects[0].id),"E1"); m.projects[0].type=PROJECT_EXTERNAL;
    m.projects[0].proposal_workload=1.0; m.projects[0].proposal_cost=2.0;
    m.projects[0].delay_cost=3.0;
    m.grades[0].initial_staff=1.0; m.grades[0].external_limit=2.0;
    m.grades[0].internal_cost=4.0; m.grades[0].short_penalty=10.0;
    m.marketing.initial_staff=1.0; m.marketing.proposal_capacity_per_fte=2.0;
    m.observations[0]=(Observation){0,0,1,1,2,2,3};
    m.wins[0]=(WinResult){0,0,1,2,0.5};
    m.revenue_profile[0]=100.0; m.resource_profile[0]=1.5;
    m.scenarios[0].market_multiplier=1.0; m.scenarios[0].resource_multiplier=1.0;
    m.scenarios[0].cost_multiplier=1.0;
    assert(simulation_init_run(&s,&m,0,1));
    simulation_set_economics(&s,0.05,1.0);
    assert(simulation_begin_month(&s,1));
    assert(!simulation_can_start(&s,0));
    assert(simulation_propose(&s,0));
    assert(simulation_finish_month(&s,external,&metrics));
    assert(metrics.revenue==0.0);
    assert(simulation_begin_month(&s,2));
    assert(simulation_can_start(&s,0));
    assert(simulation_start(&s,0));
    assert(!simulation_start(&s,0));
    external[0]=0.5;
    assert(simulation_finish_month(&s,external,&metrics));
    assert(metrics.revenue==100.0);
    assert(metrics.demand==1.5);
    assert(metrics.shortage==0.0);
    assert(metrics.discount_factor<1.0);
    simulation_free(&s); model_free(&m);
    return 0;
}
