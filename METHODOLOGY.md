# PPSP simulation methodology and audit trail

## Replication-level common random numbers

The supplied `11_EXTERNAL_WIN_RESULTS.csv` contains one potential win result for
each scenario and external project, while `15_RUN_MATRIX.csv` specifies 20
replications. Reusing the supplied result would make those replications exact
duplicates. The run-matrix seeds also differ by policy, so using them directly
would violate the common-random-number requirement in Chapter 4.

The simulator therefore generates a corrected potential win result for every
`scenario_id x replication_id x project_id`. The generator is deterministic
SplitMix64 (`splitmix64-v1`) keyed only by:

- the scenario `random_seed` from `02_SCENARIOS.csv`;
- replication number (1--20); and
- the stable project index established by the input project files.

Policy ID is deliberately absent from the key. Consequently, P0--P9 see the
same potential win result for a given scenario, replication, and project, while
different replications receive different draws. A win occurs when the generated
uniform variate is below `scenario_win_prob` from
`11_EXTERNAL_WIN_RESULTS.csv`. Values become available to a policy only at the
specified win-confirm month.

Generate the auditable table with:

```text
ppsp_sim --generate-replicated-wins <data-directory> <output.csv>
```

The current generated table contains 154,000 unique rows
(`70 x 20 x 110`), no duplicate keys, and generator version metadata in every
row. Its SHA-256 is recorded with each final experiment package rather than
treated as a permanent constant, because regenerating after an intentional
input or generator change must produce a new hash.

## Statistical aggregation

Replications are averaged within each scenario first. Scenario-level results
are then aggregated with the probabilities in `02_SCENARIOS.csv`. Policy
comparisons use paired results under the same scenario and replication. Raw
run-level files remain available for reproducibility. This avoids treating the
scenario-replication hierarchy as 1,400 unrelated observations.

## Information timing

Scenario-wide cost conditions are public before the monthly decision. Potential
external win results and realized external revenue/resource multipliers enter
the information state at the win-confirm month regardless of proposal history.
Contractual start obligation still requires a proposal submitted before that
month and a realized win.

## Required interpretation

Generated numerical output is not, by itself, a dissertation conclusion. Final
tables must be accompanied by input and executable hashes, validation results,
weighted estimands, uncertainty measures, and a written account of all model
assumptions and limitations.

## Economic evaluation parameters

The reference evaluation uses a 5% annual discount rate with monthly factors
`beta_t = (1 + 0.05)^(-(t-1)/12)` and an internal-ratio excess penalty of 1.0.
At each year end, excess is `max(internal revenue - 0.70 * total revenue, 0)`.
The sensitivity grid crosses annual rates `{0%, 3%, 5%, 7%}` with penalty
rates `{0.5, 1.0, 2.0}`. The generated `101` table is explicitly a
fixed-decision evaluation sensitivity: it holds CRN policy decisions constant
to isolate valuation effects. It must not be described as re-optimization under
each parameter combination.

## Failure classification and interpretation

The run-level fail flag is the union of three conditions: final adjusted profit
below the configured minimum, cumulative shortage divided by cumulative demand
above the configured limit, or shortage in the same grade for at least the
configured number of consecutive months. Reconstruction from all monthly-grade
records matched all 14,000 recorded flags. In the reference experiment every
flag was caused solely by the three-consecutive-month condition; no run failed
the profit or cumulative-shortage-rate condition. The thesis therefore reports
this measure as a composite operational-warning classification and accompanies
it with the mean shortage rate and a 2--6 month threshold sensitivity analysis.

## Held-out policy-selection validation

Two internal validation designs assess whether policy selection depends on a
small subset of scenarios. Leave-one-scenario-group-out validation selects a
policy using six groups and evaluates it in the seventh. Stratified ten-fold
validation holds out one scenario from each group, selects a policy on the
remaining 63 scenarios, and evaluates it on the seven held-out scenarios.
Policies are predefined rather than fitted, so these are held-out policy-selection
checks, not predictive model training. Because all scenarios come from the same
data-generating design, these checks do not replace validation on new enterprise
data or a genuinely independent scenario generator.

## Practical-effect translation

The dataset monetary unit is KRW million. Practical-effect reporting converts
the paired P8-minus-P0 objective difference into five-year present value, simple
annual average, five-percent equivalent annual value, expected changes in project
starts, discounted candidate-project gross revenue, and the residual modelled
cost-and-penalty burden. Shared workforce, idle, shortage, and marketing costs
are not arbitrarily allocated to individual projects; project-level revenue is
therefore described as gross contribution rather than causal net profit.

## Checkpoint win-rate definition

At months 12, 24, 36, 48, and 60, the checkpoint win rate is wins divided by
proposals whose win-confirm month has occurred by that checkpoint. Proposals
that remain unresolved are excluded from both numerator and denominator. This
prevents pending proposals from being incorrectly counted as losses. The final
run-level win rate remains total wins divided by total proposals over the full
horizon.
