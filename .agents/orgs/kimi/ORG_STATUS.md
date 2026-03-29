# Kimi Org Status

**Mode:** Experiment-Analysis Sidecar (Sprint 6)  
**Last Updated:** 2026-03-29  
**Worktree:** `.agents/orgs/kimi/`

---

## Org Lead

- Lead session: Kimi experiment-analysis sidecar
- Lead worktree: `.agents/orgs/kimi/`
- Integration worktree: repo root (merge via PR)

---

## Active Teams

| Team | Worktree | Mission | Write Scope | Status |
|------|----------|---------|-------------|--------|
| RF-Sync Research | `.agents/orgs/kimi/` | Complete | `docs/rf-*.md` | Complete |
| Pose Feasibility | `.agents/orgs/kimi/` | Complete | `docs/pose-*.md` | Complete |
| Hardware Futures | `.agents/orgs/kimi/` | Complete | `docs/hardware-futures.md` | Complete |
| Adversarial Review | `.agents/orgs/kimi/` | Complete | `docs/adversarial-review-*.md` | Complete |
| Implementation Support | `.agents/orgs/kimi/` | Complete | `*-SLICES.md, *-STARTER_PACK.md` | Complete |
| **Experiment Analysis** | `.agents/orgs/kimi/` | **Phase 1 Handoff** | `PHASE1_IMPLEMENTATION_PACK.md` | Ready |

---

## Planning Gate (Sprint 6)

### Problems Currently Owned
1. **Experiment Result Schema:** Design complete
2. **Batch Pipeline Architecture:** Design complete  
3. **LLM Summarization Workflow:** Design complete
4. **Batch Priorities:** P0/P1/P2 defined
5. **Phase 1 Implementation Pack:** Ready for handoff

### Writable Scopes Currently Claimed
- `.agents/orgs/kimi/EXPERIMENT_RESULT_SCHEMA.md`
- `.agents/orgs/kimi/BATCH_PIPELINE_ARCHITECTURE.md`
- `.agents/orgs/kimi/LLM_SUMMARIZATION_WORKFLOW.md`
- `.agents/orgs/kimi/EXPERIMENT_BATCH_PRIORITIES.md`
- `.agents/orgs/kimi/PHASE1_IMPLEMENTATION_PACK.md` (NEW)
- `.agents/orgs/kimi/ORG_STATUS.md` (this file)

### Review-Only Scopes
- `simulators/` (Codex owned - Wave A/B implementation)
- `tests/` (Codex owned - Wave A/B implementation)
- `tools/` (shared - Codex owns core tools, Kimi owns analysis/)
- `docs/` (Claude owned - architecture sequencing)
- `.agents/` (Claude owned - org management)

### Blocked or Contested Scopes
- `tools/analysis/` — **CLAIMED by Kimi for implementation**
- No conflicts with Codex (new directory)
- No conflicts with Claude (analysis sidecar)

### No-Duplication Check Completed
- Kimi owns experiment-analysis sidecar
- Codex owns Wave A/B test implementation (M2 focus)
- Claude owns architecture sequencing and acceptance guidance
- Kimi claims `tools/analysis/` — new directory, zero overlap
- No file conflicts: all sidecar work is additive

### Approved to Execute
- YES (Phase 1 pack ready for implementation agent)

---

## Sprint 6 Deliverables

### Phase 1 Implementation Pack
**Document:** `PHASE1_IMPLEMENTATION_PACK.md`

| Component | Status | Specified In Pack |
|-----------|--------|-------------------|
| `schema.py` | Specified | Exact Pydantic models |
| `metrics.py` | Specified | Exact functions with docstrings |
| `run_single_analysis.py` | Specified | CLI with argparse |
| Example artifacts | Provided | manifest.json, samples.csv, summary.json |
| First tests | Specified | test_schema.py, test_metrics.py |
| Boundaries | Clear | No C++ changes, additive only |
| Ollama guidance | Provided | llama3.2:3b recommended |

### Pack Contents Summary
```
PHASE1_IMPLEMENTATION_PACK.md
├── File 1: tools/analysis/schema.py (exact spec)
├── File 2: tools/analysis/metrics.py (exact spec)
├── File 3: tools/analysis/run_single_analysis.py (exact spec)
├── Example Artifacts
│   ├── manifest.json (full example)
│   ├── samples.csv (header + schema)
│   └── summary.json (full example)
├── First Tests
│   ├── test_schema.py (exact test cases)
│   └── test_metrics.py (exact test cases)
├── Boundaries (what NOT to touch)
├── Ollama Model Recommendation
└── Implementation Order + Success Criteria
```

---

## Sidecar vs Mainline Separation

### This Sidecar Does NOT Touch:
- `simulators/` (Codex owns)
- `tests/` (Codex owns)
- `firmware/` (Claude/Codex own)
- Existing `tools/` (Codex owns)
- Any C++ code
- Any existing test files

### This Sidecar Creates:
- `tools/analysis/` (NEW — Kimi owns)
- `tools/analysis/tests/` (NEW — test suite)
- `experiments/` (NEW — output directory, gitignored)
- Python analysis scripts (additive only)

### Integration Point:
- Sidecar **consumes** C++ test output via CSV export
- Sidecar **produces** reports/plots/summaries for human review
- Sidecar **suggests** follow-ups (not mandates)

---

## Batch Priority Summary

| Priority | Batch | Runs | Effort | Value | Blocked By |
|----------|-------|------|--------|-------|------------|
| P0 | Wave A Baseline | 45 | 1h | Foundation | A1-A6 tests |
| P0 | Mahony Sweep | 36 | 2h | Tuning data | A4 test |
| P1 | Bias Sensitivity | 60 | 3h | Validates A5 | A5 test |
| P1 | Seed Sensitivity | 20 | 4h | Determinism check | None |
| P2 | Noise Sensitivity | TBD | 4h | Wave B | B4 gaps |
| P2 | Long Duration | TBD | 8h | Post-M2 | A3 test |

**Total P0/P1:** 161 runs, ~10h compute, ~750 MB data

---

## LLM Integration

### Model Recommendations
| Model | Size | RAM | Use Case | When |
|-------|------|-----|----------|------|
| `llama3.2:3b` | 3B | 4GB | Development, quick summaries | Phase 3 |
| `qwen2.5:7b` | 7B | 8GB | Production reports | Phase 3 |
| `deepseek-r1:7b` | 7B | 8GB | Anomaly analysis | Phase 3 |

**Phase 1:** No LLM required — pure Python computation

### Trust Boundaries
- LLM summarizes pre-computed metrics
- LLM compares runs qualitatively
- LLM does not compute statistics
- LLM does not generate code fixes
- LLM suggestions require human validation

---

## Readiness Assessment

### Can an Implementation Agent Start Phase 1?

| Criterion | Status |
|-----------|--------|
| Exact file specs | schema.py, metrics.py, run_single_analysis.py |
| Dependencies listed | pydantic, numpy, argparse |
| Example artifacts | manifest.json, samples.csv, summary.json |
| Tests defined | test_schema.py, test_metrics.py |
| Boundaries clear | No C++ changes, new directory only |
| Ollama guidance | llama3.2:3b recommended for Phase 3 |

**Verdict:** Implementation pack is ready. Can hand off to implementation agent immediately.

### Success Criteria for Phase 1
- `python -m tools.analysis.run_single_analysis --help` works
- Can load example manifest/samples/summary without errors
- Can compute metrics and print summary
- All unit tests pass
- No C++ code modified
- No existing tests broken

---

## Anti-Scope-Creep Summary

### Sidecar Does NOT:
- Modify simulator code
- Modify test assertions
- Require cloud services
- Replace C++ tests
- Add M2 blocking dependencies
- Generate LLM summaries in Phase 1 (Phase 3 only)

### Sidecar DOES:
- Parse existing test outputs
- Generate plots and reports
- Use local Ollama (optional)
- Enable parameter sweeps
- Feed findings to future sprints

---

## Integration with Future Sprints

### Output Feeds Into:
- **M2 Close:** Wave A baseline validates acceptance criteria
- **M3 Planning:** Mahony sweep informs tuning decisions
- **M5 Planning:** Bias sensitivity validates calibration needs
- **Hardware Bring-up:** Seed sensitivity proves determinism

### Handoff to Codex/Claude:
- LLM summaries provide human-readable evidence
- Plots provide visual validation
- Statistical reports provide hard numbers
- Recommendations suggest (not mandate) next steps

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total documents created | 20 |
| Implementation slices defined | 11 (RF + Magnetic) |
| Starter packs prepared | 2 (deferred) |
| Experiment batches defined | 6 (P0/P1/P2) |
| Schema definitions | 3 (manifest, samples, summary) |
| LLM prompt templates | 3 (summary, compare, triage) |
| Phase 1 files specified | 3 (schema, metrics, run_single) |

---

## Next Steps

1. **Phase 1 Implementation:** Hand off to implementation agent
   - Implement `tools/analysis/schema.py`
   - Implement `tools/analysis/metrics.py`
   - Implement `tools/analysis/run_single_analysis.py`
   - Write tests for schema and metrics

2. **Phase 2 Planning:** After A1-A6 tests pass
   - Implement `tools/analysis/batch_runner.py`
   - Run Batch 1 (Wave A Baseline)
   - Run Batch 2 (Mahony Sweep)

3. **Phase 3 Planning:** LLM integration
   - Implement `tools/analysis/llm_summarizer.py`
   - Add Ollama client
   - Generate narrative summaries

4. **Future:** Regression detection, automated anomaly detection

---

**Status:** Sprint 6 complete — Phase 1 implementation pack ready for handoff
