#include "ppsp/policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    PolicySet set={0};PolicyConfig p;const PolicyConfig*base;
    set.policy_count=1;set.stage_count=2;
    snprintf(set.policies[0].id,sizeof(set.policies[0].id),"P7");set.policies[0].max_proposal_per_month=5;
    set.stages[0].from_month=1;set.stages[0].to_month=12;
    set.stages[0].override_values.max_proposal_per_month=3;
    set.stages[1].from_month=25;set.stages[1].to_month=42;
    set.stages[1].override_values.max_proposal_per_month=6;
    base=policy_find(&set,"P7");assert(base);
    p=policy_for_month(&set,base,1);assert(p.max_proposal_per_month==3);
    p=policy_for_month(&set,base,25);assert(p.max_proposal_per_month==6);
    assert(policy_find(&set,"P9")==0);
    return 0;
}
