# Reproducibility procedure

## Reference configuration

- Horizon: 60 months
- Policies: P0-P9
- Scenarios: 70
- Replications: 20 per policy/scenario pair
- Total runs: 10 x 70 x 20 = 14,000
- Annual discount rate: 0.05
- Internal-revenue-ratio penalty multiplier: 1.0
- Random-number design: policy-independent common random numbers (CRN)

The input directory used for the reported result is
`../thesis_ppsp_v04_paper_D_drive_ready/data` and the result directory is
`experiment_output_base`.

## Build and test

Run from this directory in a Visual Studio x64 developer prompt:

```powershell
cmake -S . -B build-msvc -G Ninja
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

The project also builds warning-free with Clang using `build-clang`.

## Full experiment and tables

```powershell
./build-msvc/ppsp_sim.exe --run-experiment-params ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_base 0.05 1.0
python ./tools/analyze_results.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_base
python ./tools/sensitivity_analysis.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_base
python ./tools/generate_thesis_charts.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_base
```

For the extended project-decision replication, use `experiment_output_project`
as the output directory and then run:

```powershell
python ./tools/analyze_project_decisions.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_project
python ./tools/analyze_failure_practical_oos.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_project
python ./tools/generate_thesis_charts.py ../thesis_ppsp_v04_paper_D_drive_ready/data ./experiment_output_project
```

`99_OUTPUT_PROJECT_DECISION.csv` contains 2,660,000 candidate-project traces and
is intentionally excluded from Git. Project-level gross revenue is reported
without arbitrary allocation of shared workforce and shortage costs.

The raw monthly table is approximately 515 MB and is intentionally excluded
from Git. Preserve the audit JSON files and summary tables with an archived raw
result when preparing the dissertation replication package.

## Interpretation boundary

`101_SENSITIVITY_SUMMARY.csv` is a fixed-decision evaluation sensitivity
analysis. It changes discount and penalty parameters when evaluating the same
simulated decisions; it does not re-optimize policy decisions for each parameter
combination. See `METHODOLOGY.md` for assumptions and validity limitations.
