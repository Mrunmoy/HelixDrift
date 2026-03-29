# Batch Pipeline Architecture

**Document ID:** KIMI-EXP-PIPELINE-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** Python-based batch execution and analysis pipeline  
**Status:** Design Complete

---

## Overview

A lightweight, deterministic batch pipeline for running simulation experiments and analyzing results. Designed to run in parallel with Codex's M2 work without interference.

**Principles:**
- Pure Python 3.10+ (no external services)
- Local-first execution (no cloud required)
- Deterministic outputs (reproducible runs)
- Additive to existing codebase (no C++ changes required)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Batch Pipeline                                   │
│                                                                          │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐              │
│  │   Batch      │───▶│   Runner     │───▶│   Storage    │              │
│  │   Definition │    │   (Python)   │    │   (JSON/CSV) │              │
│  └──────────────┘    └──────────────┘    └──────────────┘              │
│         │                   │                    │                      │
│         ▼                   ▼                    ▼                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐              │
│  │  Parameter   │    │  C++ Test    │    │  Schema      │              │
│  │  Matrix      │    │  Executable  │    │  Validation  │              │
│  └──────────────┘    └──────────────┘    └──────────────┘              │
│                               │                                         │
│                               ▼                                         │
│  ┌──────────────────────────────────────────────────────────┐          │
│  │                   Analysis Layer                          │          │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │          │
│  │  │  Metrics │  │  Plots   │  │  LLM     │  │  Report  │  │          │
│  │  │  Compute │  │  Generate│  │  Summarize│  │  Export  │  │          │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │          │
│  └──────────────────────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Component Specifications

### 1. Batch Runner (`tools/analysis/batch_runner.py`)

**Purpose:** Execute a batch of experiments defined by a JSON matrix.

**Interface:**
```bash
python -m tools.analysis.batch_runner \
    --batch experiments/batches/wave_A_batch.json \
    --output experiments/runs/20260329/ \
    --parallel 4 \
    --verbose
```

**Key Features:**
- Parameter matrix expansion (Cartesian product)
- Parallel execution with `multiprocessing.Pool`
- Deterministic ordering (sorted parameter combinations)
- Progress reporting (tqdm)
- Resume capability (skip completed runs)

**Implementation Sketch:**
```python
class BatchRunner:
    def __init__(self, batch_def: BatchDefinition, output_dir: Path):
        self.batch = batch_def
        self.output_dir = output_dir
        
    def generate_runs(self) -> List[RunConfiguration]:
        """Expand parameter matrix into individual run configs."""
        matrix = self.batch.matrix
        # Cartesian product of all parameter combinations
        # Returns list of RunConfiguration
        
    def execute_run(self, config: RunConfiguration) -> RunResult:
        """Execute single C++ test and capture output."""
        # Build command line from config
        # Run helix_integration_tests with gtest_filter
        # Parse CSV output from test
        # Write manifest.json, samples.csv, summary.json
        
    def run_all(self, parallel: int = 4) -> BatchResult:
        """Execute all runs with progress tracking."""
        # Use ProcessPoolExecutor for parallel runs
        # Each process calls execute_run()
        # Aggregate results into BatchResult
```

---

### 2. Metrics Computer (`tools/analysis/metrics.py`)

**Purpose:** Compute summary statistics from raw samples.

**Key Functions:**
```python
def compute_angular_error_metrics(samples: pd.DataFrame) -> ErrorMetrics:
    """RMS, max, mean, percentiles of angular error."""
    
def compute_drift_rate(samples: pd.DataFrame, 
                       method: str = "linear_regression") -> float:
    """Degrees per minute drift, using specified method."""
    
def detect_convergence(samples: pd.DataFrame,
                      threshold_deg: float = 3.0) -> float:
    """Time to first enter and stay within threshold."""
    
def compare_runs(baseline: RunResult, 
                variant: RunResult) -> ComparisonReport:
    """Statistical comparison between two runs."""
```

---

### 3. Plot Generator (`tools/analysis/plotter.py`)

**Purpose:** Generate standard plots for visual inspection.

**Standard Plots:**

| Plot Name | Filename | Description |
|-----------|----------|-------------|
| Error timeseries | `{run_id}_error_ts.png` | Angular error over time |
| Convergence curve | `{run_id}_convergence.png` | Error vs time with threshold |
| Histogram | `{run_id}_histogram.png` | Error distribution |
| Quaternion trajectory | `{run_id}_quat_3d.png` | 3D orientation trace |
| Parameter sweep heatmap | `{batch_id}_heatmap.png` | RMS error vs Kp/Ki |
| Comparison overlay | `{comparison_id}_overlay.png` | Two runs side-by-side |

**Interface:**
```python
class PlotGenerator:
    def __init__(self, style: str = "helix_default"):
        plt.style.use(style)
        
    def plot_error_timeseries(self, samples: pd.DataFrame, 
                             output_path: Path) -> None:
        """Line plot of angular error vs time."""
        
    def plot_parameter_heatmap(self, batch_result: BatchResult,
                               x_param: str, y_param: str,
                               metric: str, output_path: Path) -> None:
        """2D heatmap of metric across parameter sweep."""
```

---

### 4. LLM Integration (`tools/analysis/llm_summarizer.py`)

**Purpose:** Use local Ollama to summarize and triage experiment results.

**Interface:**
```python
class OllamaSummarizer:
    def __init__(self, model: str = "llama3.2:3b", 
                 host: str = "http://localhost:11434"):
        self.model = model
        self.host = host
        
    def summarize_run(self, run_result: RunResult) -> str:
        """Generate natural language summary of a single run."""
        
    def compare_runs_llm(self, run_a: RunResult, 
                        run_b: RunResult) -> str:
        """Generate LLM comparison of two runs."""
        
    def triage_batch(self, batch_result: BatchResult) -> TriageReport:
        """Identify anomalies and potential issues in batch."""
```

**Ollama Dependency:**
- Optional: pipeline works without LLM (plots + metrics only)
- Auto-detect: if `ollama` not available, skip LLM features
- Local-only: no API keys, no cloud dependencies

---

## Directory Layout

```
tools/analysis/
├── __init__.py
├── README.md                      # Usage guide
├── requirements.txt               # Python deps (numpy, pandas, matplotlib)
│
├── batch_runner.py                # Main entry point
├── metrics.py                     # Statistics computation
├── plotter.py                     # Visualization
├── llm_summarizer.py              # Ollama integration
├── schema.py                      # Pydantic models for JSON validation
│
├── templates/                     # LLM prompt templates
│   ├── run_summary.txt
│   ├── run_comparison.txt
│   └── batch_triage.txt
│
└── tests/                         # Unit tests for analysis tools
    ├── test_metrics.py
    └── test_schema.py

experiments/
├── batches/                       # Batch definitions (JSON)
│   ├── wave_A_batch.json
│   ├── mahony_sweep_batch.json
│   └── sensor_noise_sensitivity.json
│
├── runs/                          # Actual run outputs (gitignored)
│   └── 20260329/
│
└── analysis/                      # Analysis outputs (gitignored)
    ├── plots/
    ├── llm_summaries/
    └── regression_reports/
```

---

## Execution Flow

### Single Run
```
1. Load batch definition (JSON)
2. Expand parameter matrix → list of run configs
3. For each config:
   a. Generate run directory (experiments/runs/YYYYMMDD/experiment_name/run_NNN/)
   b. Build C++ test command with parameters
   c. Execute test, capture CSV output
   d. Compute metrics, generate summary.json
   e. (Optional) Generate plots
   f. (Optional) Generate LLM summary
4. Aggregate all runs into batch report
5. Generate batch-level plots (heatmaps, comparisons)
6. (Optional) Generate LLM batch triage report
```

### Resume Capability
```
- Check for existing manifest.json in run directory
- If present and valid, skip execution (unless --force)
- If corrupt/missing, re-run
- Enables interrupted batch recovery
```

---

## Integration with C++ Tests

### No C++ Changes Required

The pipeline **consumes** existing test output via:
1. **CSV Export:** Tests export samples when `HELIX_TEST_EXPORT=1` set
2. **Return Codes:** Test pass/fail determines run status
3. **Stdout Parsing:** Extract metrics printed to console

### Future C++ Enhancement (Optional)

If Codex wants tighter integration later:
```cpp
// Optional: Add to VirtualMocapNodeHarness
void exportToDirectory(const std::string& path);
// Writes manifest, samples.csv, summary.json directly
```

**This is NOT required for pipeline operation.**

---

## Determinism Guarantees

### What Makes It Deterministic
1. Fixed seeds in manifest → reproducible C++ runs
2. Sorted parameter combinations → consistent ordering
3. No timestamps in output filenames → git-friendly diffs
4. Fixed-precision float output → byte-stable CSVs

### What Can Vary (Acceptable)
- Wall-clock execution time
- Process scheduling order (but results identical)
- Matplotlib backend rendering (PNG pixel diffs OK)

---

## Anti-Scope-Creep Notes

### Pipeline Does NOT:
- ❌ Modify C++ simulator code (read-only consumption)
- ❌ Require cloud services (local-first)
- ❌ Run experiments in real-time (batch only)
- ❌ Replace C++ tests (complements them)
- ❌ Require GPU (CPU-only, optional local GPU for LLM)

### Pipeline Does:
- ✅ Parse existing test outputs
- ✅ Compute statistics and generate plots
- ✅ Use local Ollama for summaries (optional)
- ✅ Produce human-readable reports
- ✅ Enable parameter sweeps

---

## Start Here First

### Phase 1: Minimal Viable (Week 1)
1. Implement `schema.py` (Pydantic models)
2. Implement `metrics.py` (basic statistics)
3. Single-run analysis script
4. Test on one A1 experiment

### Phase 2: Batch Execution (Week 2)
1. Implement `batch_runner.py`
2. Add parameter matrix expansion
3. Parallel execution
4. Test on A4 Kp/Ki sweep

### Phase 3: LLM Integration (Week 3)
1. Implement `llm_summarizer.py`
2. Add Ollama auto-detection
3. Prompt templates
4. Batch triage reports

### Phase 4: Advanced Analysis (Deferred)
- Regression detection across commits
- Statistical significance testing
- Automated anomaly detection

---

**Status:** ✅ Architecture ready for implementation
