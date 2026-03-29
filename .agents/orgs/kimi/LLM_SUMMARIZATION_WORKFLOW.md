# LLM Summarization Workflow

**Document ID:** KIMI-EXP-LLM-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** Local Ollama integration for experiment analysis  
**Status:** Design Complete

---

## Overview

This document defines how local LLMs (via Ollama) should be used to summarize, compare, and triage simulation experiment results. Designed for **optional** use — the pipeline works without LLM support.

**Key Principles:**
- Local-first: Ollama runs on same machine, no cloud APIs
- Deterministic: Same experiment → same summary (temperature=0)
- Verifiable: LLM outputs are suggestions, not ground truth
- Scoped: LLM handles natural language; Python handles numbers

---

## Ollama Setup

### Installation (One-time)
```bash
# Install Ollama (Linux)
curl -fsSL https://ollama.com/install.sh | sh

# Pull appropriate model (3B params minimum, 7B recommended)
ollama pull llama3.2:3b      # Fast, minimal RAM (~4GB)
ollama pull qwen2.5:7b       # Better reasoning (~8GB)
ollama pull deepseek-r1:7b   # Best for analysis (~8GB)
```

### Verification
```bash
ollama list
# Should show: llama3.2:3b, qwen2.5:7b, etc.

ollama run llama3.2:3b --version
# Test basic functionality
```

---

## Use Cases

### Use Case 1: Single Run Summary
**When:** After individual experiment completes  
**Input:** manifest.json + summary.json  
**Output:** narrative.txt (natural language summary)

**Value:**
- Human-readable explanation of results
- Highlights unexpected patterns
- Suggests follow-up experiments

### Use Case 2: Run Comparison
**When:** Comparing two parameter settings (e.g., Ki=0 vs Ki=0.1)  
**Input:** Two run summaries + delta metrics  
**Output:** comparison.txt

**Value:**
- Explains why one configuration outperforms another
- Identifies trade-offs (convergence vs steady-state)
- Suggests optimal parameter range

### Use Case 3: Batch Triage
**When:** After parameter sweep (e.g., A4 Kp/Ki sweep)  
**Input:** Batch results with 12+ runs  
**Output:** triage_report.md

**Value:**
- Identifies outliers and anomalies
- Flags potential implementation bugs
- Suggests which regions of parameter space to investigate

---

## Prompt Templates

### Template 1: Run Summary

**File:** `tools/analysis/templates/run_summary.txt`

```
You are analyzing a sensor fusion experiment for a wearable motion capture device.

EXPERIMENT DETAILS:
- ID: {{experiment_id}}
- Description: {{description}}
- Mahony Kp: {{mahony_kp}}, Ki: {{mahony_ki}}
- Motion profile: {{motion_profile}}

RESULTS SUMMARY:
- Samples: {{sample_count}}
- Duration: {{duration_seconds}}s
- RMS angular error: {{rms_error_deg}}°
- Maximum error: {{max_error_deg}}°
- Final error: {{final_error_deg}}°
- Drift rate: {{drift_rate_dpm}}°/min

ACCEPTANCE CRITERIA:
{% for check in acceptance_checks %}
- {{check.name}}: {{check.actual}} (threshold: {{check.threshold}}) → {{check.status}}
{% endfor %}

OVERALL STATUS: {{overall_status}}

TASK:
Write a 2-3 paragraph summary of this experiment result. Include:
1. Whether the results meet specification
2. Any notable patterns or anomalies
3. Comparison to typical performance for this type of test
4. Suggested follow-up if results are marginal

Be concise and technical. Use specific numbers from the results.
```

**Example Output:**
```
This static pose test at 90° yaw achieved an RMS angular error of 2.34°, 
well within the 3° threshold. The maximum error of 5.67° occurred during 
initial convergence (first 1.5s), after which the filter stabilized to 
under 2° steady-state error.

The drift rate of 0.45°/min indicates excellent stability with the default
Mahony tuning (Kp=1.0, Ki=0.02). This is typical for clean sensor data 
without injected bias. No anomalies detected.

Follow-up: Consider testing with gyro bias injection to verify Ki 
effectiveness under degraded conditions.
```

---

### Template 2: Run Comparison

**File:** `tools/analysis/templates/run_comparison.txt`

```
You are comparing two sensor fusion configurations for a wearable motion capture device.

CONFIGURATION A ({{config_a.name}}):
- Mahony Kp: {{config_a.kp}}, Ki: {{config_a.ki}}
- RMS error: {{config_a.rms}}°
- Max error: {{config_a.max}}°
- Convergence time: {{config_a.convergence}}s

CONFIGURATION B ({{config_b.name}}):
- Mahony Kp: {{config_b.kp}}, Ki: {{config_b.ki}}
- RMS error: {{config_b.rms}}°
- Max error: {{config_b.max}}°
- Convergence time: {{config_b.convergence}}s

DIFFERENCES:
- RMS error delta: {{delta_rms}}° ({{delta_rms_pct}}%)
- Max error delta: {{delta_max}}° ({{delta_max_pct}}%)
- Convergence delta: {{delta_conv}}s

TASK:
Compare these two configurations. Explain:
1. Which performs better and by how much
2. Trade-offs between configurations (if any)
3. Recommendations for which to use and why

Be specific with numbers. If differences are <10%, note they are marginal.
```

---

### Template 3: Batch Triage

**File:** `tools/analysis/templates/batch_triage.txt`

```
You are triaging a batch of {{run_count}} sensor fusion experiments.

BATCH: {{batch_id}}
DESCRIPTION: {{batch_description}}

RUN RESULTS:
{% for run in runs %}
Run {{run.id}}: Kp={{run.kp}}, Ki={{run.ki}} → RMS={{run.rms}}°, Max={{run.max}}°, Status={{run.status}}
{% endfor %}

STATISTICS:
- Mean RMS: {{stats.mean_rms}}° (std: {{stats.std_rms}}°)
- Min/Max RMS: {{stats.min_rms}}° / {{stats.max_rms}}°
- Failed runs: {{stats.failed_count}}/{{run_count}}

ANOMALIES DETECTED (by Python preprocessor):
{% for anomaly in anomalies %}
- {{anomaly.description}}: {{anomaly.run_id}}
{% endfor %}

TASK:
1. Identify any surprising or concerning patterns
2. Flag potential implementation bugs (e.g., unexpected sensitivity to parameters)
3. Suggest which parameter combinations warrant deeper investigation
4. Recommend regions of parameter space to avoid or prioritize

Format as bullet points. Be specific with run IDs.
```

---

## LLM Trust Boundaries

### What LLM CAN Do (Reliable)
- ✅ Summarize numeric results in natural language
- ✅ Compare two configurations qualitatively
- ✅ Flag obvious anomalies (e.g., "Ki=0.1 performs worse than Ki=0")
- ✅ Suggest follow-up experiments based on patterns

### What LLM CANNOT Do (Do Not Trust)
- ❌ Compute statistics (use Python for numbers)
- ❌ Determine statistical significance (use scipy)
- ❌ Generate code for fixes (human review required)
- ❌ Replace C++ test assertions (suggestions only)

### What LLM MIGHT Do (Verify Independently)
- ⚠️ Explain physical phenomena (verify with domain knowledge)
- ⚠️ Suggest optimal parameters (test empirically)
- ⚠️ Identify "optimal" configurations (validate with sweeps)

---

## Implementation

### Ollama Client (`tools/analysis/llm_client.py`)

```python
import requests
from typing import Optional

class OllamaClient:
    def __init__(self, model: str = "llama3.2:3b", 
                 host: str = "http://localhost:11434"):
        self.model = model
        self.host = host
        self.available = self._check_availability()
        
    def _check_availability(self) -> bool:
        """Check if Ollama is running and model is available."""
        try:
            resp = requests.get(f"{self.host}/api/tags", timeout=2)
            if resp.status_code == 200:
                models = [m["name"] for m in resp.json()["models"]]
                return self.model in models
        except:
            pass
        return False
    
    def generate(self, prompt: str, temperature: float = 0.0) -> str:
        """Generate text with deterministic output."""
        if not self.available:
            return "[LLM unavailable - install Ollama and pull model]"
            
        resp = requests.post(
            f"{self.host}/api/generate",
            json={
                "model": self.model,
                "prompt": prompt,
                "temperature": temperature,
                "stream": False
            }
        )
        return resp.json()["response"]
```

### Summarizer Class (`tools/analysis/llm_summarizer.py`)

```python
from jinja2 import Template

class LLMSummarizer:
    def __init__(self, client: Optional[OllamaClient] = None):
        self.client = client or OllamaClient()
        self.templates = self._load_templates()
        
    def _load_templates(self) -> Dict[str, Template]:
        """Load Jinja2 templates from templates/ directory."""
        templates = {}
        for name in ["run_summary", "run_comparison", "batch_triage"]:
            path = Path(__file__).parent / "templates" / f"{name}.txt"
            templates[name] = Template(path.read_text())
        return templates
        
    def summarize_run(self, run_result: RunResult) -> str:
        """Generate natural language summary of a run."""
        context = self._build_run_context(run_result)
        prompt = self.templates["run_summary"].render(context)
        return self.client.generate(prompt, temperature=0.0)
        
    def compare_runs(self, run_a: RunResult, run_b: RunResult) -> str:
        """Compare two runs."""
        context = self._build_comparison_context(run_a, run_b)
        prompt = self.templates["run_comparison"].render(context)
        return self.client.generate(prompt, temperature=0.0)
        
    def triage_batch(self, batch_result: BatchResult) -> str:
        """Triage batch results."""
        # Pre-compute anomalies with Python (don't trust LLM for this)
        anomalies = self._detect_anomalies(batch_result)
        
        context = self._build_batch_context(batch_result, anomalies)
        prompt = self.templates["batch_triage"].render(context)
        return self.client.generate(prompt, temperature=0.0)
        
    def _detect_anomalies(self, batch_result: BatchResult) -> List[Anomaly]:
        """Python-based anomaly detection (deterministic)."""
        anomalies = []
        rms_values = [r.summary.rms_error_deg for r in batch_result.runs]
        
        # Z-score outlier detection
        mean_rms = statistics.mean(rms_values)
        std_rms = statistics.stdev(rms_values) if len(rms_values) > 1 else 0
        
        for run in batch_result.runs:
            z_score = (run.summary.rms_error_deg - mean_rms) / std_rms if std_rms > 0 else 0
            if abs(z_score) > 2.5:
                anomalies.append(Anomaly(
                    run_id=run.experiment_id,
                    description=f"RMS error {z_score:.1f}σ from mean"
                ))
                
        return anomalies
```

---

## Model Selection Guide

| Model | Size | RAM | Speed | Use Case |
|-------|------|-----|-------|----------|
| `llama3.2:3b` | 3B | 4GB | Fast | Basic summaries, quick triage |
| `qwen2.5:7b` | 7B | 8GB | Medium | Detailed analysis, comparisons |
| `deepseek-r1:7b` | 7B | 8GB | Medium | Best for reasoning about anomalies |
| `mixtral:8x7b` | 47B | 32GB | Slow | Not recommended (overkill) |

**Recommendation:** Start with `llama3.2:3b` for development. Use `qwen2.5:7b` for final reports.

---

## Anti-Scope-Creep

### LLM Does NOT:
- ❌ Generate C++ code for fixes
- ❌ Modify test assertions
- ❌ Access the internet (local only)
- ❌ Store conversation history
- ❌ Replace human code review

### LLM Does:
- ✅ Summarize already-computed metrics
- ✅ Flag patterns for human attention
- ✅ Suggest (not mandate) follow-ups
- ✅ Run entirely offline

---

## Start Here First

### 1. Verify Ollama Works
```bash
ollama pull llama3.2:3b
ollama run llama3.2:3b
>>> Hello, can you hear me?
```

### 2. Test Python Integration
```python
from tools.analysis.llm_client import OllamaClient
client = OllamaClient()
print(client.available)  # Should be True
print(client.generate("Summarize: RMS error is 2.3 degrees"))
```

### 3. Generate First Summary
```bash
python -m tools.analysis.summarize_run \
    --run experiments/runs/20260329/A1_static_pose/run_001 \
    --output experiments/analysis/llm_summaries/
```

---

**Status:** ✅ LLM workflow ready for implementation
