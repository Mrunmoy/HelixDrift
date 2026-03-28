# Python Analysis Plan

HelixDrift does not need MATLAB to succeed. The project needs a deterministic
analysis layer that can inspect host-test output, plot regressions, and help
us detect drift, bias, convergence, and timing problems early. Python is the
right fit for that layer.

## Goals

- Analyze deterministic simulator and harness runs without changing core C++
  architecture.
- Produce reproducible plots and summary metrics from exported test runs.
- Support Wave A and Wave B validation work with evidence that is easy to
  inspect and easy to gate in CI.
- Keep the analysis layer optional, additive, and friendly to `nix develop`.

## Non-Goals

- Do not replace the C++ simulator, harness, or host tests.
- Do not add new runtime dependencies to `firmware/common`.
- Do not require MATLAB or any licensed tooling.
- Do not add a hard dependency on notebooks for normal CI usage.

## Recommended Python Stack

Use the smallest set of common packages that solve the analysis problem:

- `numpy` for vector math and summary calculations
- `scipy` for optional curve fitting, filtering, and statistics
- `matplotlib` for plots
- `pandas` only if CSV inspection becomes easier than using the standard
  library

Keep scripts runnable in a plain `nix develop` shell. If a dependency is not
available, prefer a standard-library fallback over adding complexity.

## Suggested Directory Layout

The analysis layer should stay separate from simulator logic:

```text
simulators/scripts/
  plot_test_run.py          # render plots from exported CSV
  summarize_test_run.py     # optional CLI for metrics only

tools/
  analysis/
    README.md               # optional usage notes
    requirements.txt        # only if a lightweight helper set is needed
```

If the repo later wants notebooks for exploration, keep them out of the CI
path and treat them as disposable analysis artifacts.

## CSV And Export Expectations

The Python layer should consume the CSV schema already described in
[`docs/simulation-harness-interface.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/simulation-harness-interface.md).

Expected input characteristics:

- One header row
- One row per captured sample
- Deterministic ordering
- Floating-point fields serialized consistently
- Empty runs still produce header-only files

Expected outputs from Python:

- Plots for orientation tracking, angular error, raw sensor traces, and error
  histograms
- Optional summary JSON or text for CI-friendly comparisons
- Clear failure messages when the CSV is missing columns or contains NaN/Inf

## First Scripts

Start with two small, focused scripts:

1. `simulators/scripts/plot_test_run.py`
   - Input: one or more exported CSV files
   - Output: PNG plots in a sibling directory or a caller-provided output path
   - Behavior: render the standard diagnostics used by Wave A and Wave B

2. `simulators/scripts/summarize_test_run.py`
   - Input: one exported CSV file
   - Output: RMS, max error, drift rate, sample count, and pass/fail summary
   - Behavior: keep the script usable in CI or local triage

If one script is all that is needed initially, prioritize plotting first and
let summary-only functionality live in the same file behind a CLI flag.

## How This Integrates With Wave A And Wave B

### Wave A

Wave A is about closing M2 honestly. Python should help inspect these runs:

- Static multi-pose orientation accuracy
- Dynamic single-axis tracking
- 60s heading drift
- Mahony Kp/Ki sweeps
- Gyro bias rejection
- Two-node joint angle recovery

Python should not define the pass/fail thresholds. The C++ tests do that.
Python should make it easy to see why a run passed or failed.

### Wave B

Wave B adds evidence tooling and broader inspection support:

- CSV export from `NodeRunResult` or equivalent harness results
- Plotting scripts for deterministic comparisons
- Optional motion-profile catalog inspection
- Optional calibration-analysis helpers

Wave B should stay additive and should not force the Wave A harness to change
shape.

## Workflow

Recommended analysis workflow:

1. Run the relevant host tests with deterministic seeds.
2. Export the run only when `HELIX_TEST_EXPORT=1` is set.
3. Feed the CSV into the Python script.
4. Inspect plots and summary metrics for regressions.
5. Keep the Python output reproducible so it can be compared across runs.

## Testing And CI

- Python scripts should be testable with small golden inputs.
- Keep normal CI free of heavy plotting requirements unless a dedicated
  evidence job is explicitly enabled.
- Prefer pure functions and small CLI wrappers so behavior is easy to unit
  test.
- Do not make Python analysis a prerequisite for the host C++ tests unless it
  is specifically being used as a regression gate.

## Practical Priority

1. Plot exported runs.
2. Summarize metrics from exported runs.
3. Add optional comparison helpers for before/after regressions.
4. Add notebooks only if they materially reduce investigation time.

## Current Recommendation

Python should be used to make HelixDrift more deterministic, easier to debug,
and easier to regress-test. It should support the project, not drive the core
architecture.
