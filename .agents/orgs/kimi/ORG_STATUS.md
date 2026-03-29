# Kimi Org Status

**Mode:** Experiment-Analysis Sidecar (Sprint 7 - Final Handoff Complete)  
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
| **Experiment Analysis** | `.agents/orgs/kimi/` | **Phase 1 Handoff Complete** | `FINAL_PHASE1_HANDOFF.md` | Ready for Execution |

---

## Planning Gate (Sprint 7 - Complete)

### Problems Currently Owned
1. **Experiment Result Schema:** ✅ Design complete
2. **Batch Pipeline Architecture:** ✅ Design complete  
3. **LLM Summarization Workflow:** ✅ Design complete
4. **Batch Priorities:** ✅ P0/P1/P2 defined
5. **Phase 1 Implementation Pack:** ✅ Ready for handoff
6. **Final Handoff Packet:** ✅ Complete with corrected workflow

### Writable Scopes Currently Claimed
- `.agents/orgs/kimi/EXPERIMENT_RESULT_SCHEMA.md`
- `.agents/orgs/kimi/BATCH_PIPELINE_ARCHITECTURE.md`
- `.agents/orgs/kimi/LLM_SUMMARIZATION_WORKFLOW.md`
- `.agents/orgs/kimi/EXPERIMENT_BATCH_PRIORITIES.md`
- `.agents/orgs/kimi/PHASE1_IMPLEMENTATION_PACK.md`
- `.agents/orgs/kimi/FINAL_PHASE1_HANDOFF.md` (NEW - FINAL)
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
- ✅ YES (Final handoff complete, ready for implementation agent)

---

## Sprint 7 Deliverables

### Final Phase 1 Implementation Handoff
**Document:** `FINAL_PHASE1_HANDOFF.md`

| Component | Status | Location |
|-----------|--------|----------|
| Executable checklist | ✅ Complete | Section 1 |
| File-by-file specs | ✅ Complete | Section 2 |
| Test file specs | ✅ Complete | Section 3 |
| Corrected workflow (chicken-and-egg fixed) | ✅ Complete | Section 4 |
| Priority change assessment | ✅ Complete | Section 5 |
| One-page agent note | ✅ Complete | Section 6 |

### Key Fix from Original Pack

**Problem:** Original `RunResult.from_directory()` tried to load `summary.json` which doesn't exist during analysis (it's the OUTPUT of analysis).

**Solution:** Split into:
- `RunResult.from_raw_directory()` — loads manifest + samples only
- Summary is computed by Python, then optionally written to disk

**Impact:** Workflow is now internally consistent and executable.

---

## Priority Changes After SensorFusion Fix

### SensorFusion AHRS Convention Fix (Merged)

**Status:** Codex completed and merged the convention fix. Mainline host validation green.

**Impact on Priorities:**

| Batch | Change | Reason |
|-------|--------|--------|
| Wave A Baseline | ⚠️ A1 may be unblocked | ±90° yaw was ~118° RMS (catastrophic), now needs re-evaluation |
| All other batches | No change | Fix was specific to AHRS init/update convention |

**Recommendation:** Re-probe A1 staged entry path (identity, +90° yaw, -90° yaw) to verify if now within intermediate threshold.

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

| Priority | Batch | Runs | Effort | Blocked By | Notes |
|----------|-------|------|--------|------------|-------|
| P0 | Wave A Baseline | 45 | 1h | A1-A6 tests | A1 may be unblocked post-fix |
| P0 | Mahony Sweep | 36 | 2h | A4 test | Unchanged |
| P1 | Bias Sensitivity | 60 | 3h | A5 test | Unchanged |
| P1 | Seed Sensitivity | 20 | 4h | None | Unchanged |
| P2 | Noise Sensitivity | TBD | 4h | B4 gaps | Unchanged |
| P2 | Long Duration | TBD | 8h | A3 test | Unchanged |

**Total P0/P1:** 161 runs, ~10h compute, ~750 MB data

---

## LLM Integration (Phase 3)

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
| Exact file specs | ✅ schema.py, metrics.py, run_single_analysis.py |
| Dependencies listed | ✅ pydantic, numpy, argparse, pytest |
| Example artifacts | ✅ manifest.json, samples.csv, summary.json |
| Tests defined | ✅ test_schema.py, test_metrics.py |
| Boundaries clear | ✅ No C++ changes, new directory only |
| Workflow corrected | ✅ from_raw_directory() loads only manifest+samples |
| Chicken-and-egg fixed | ✅ Summary is output, not input |

**Verdict:** ✅ Final handoff complete. Implementation agent can start immediately.

### Success Criteria for Phase 1
- `python -m tools.analysis.run_single_analysis --help` works
- Can load example manifest/samples without errors
- Can compute metrics and print summary
- All unit tests pass (`pytest tools/analysis/tests/`)
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
| Total documents created | 21 |
| Implementation slices defined | 11 (RF + Magnetic) |
| Starter packs prepared | 2 (deferred) |
| Experiment batches defined | 6 (P0/P1/P2) |
| Schema definitions | 3 (manifest, samples, summary) |
| LLM prompt templates | 3 (summary, compare, triage) |
| Phase 1 files specified | 3 (schema, metrics, run_single) |
| Final handoff packets | 1 |

---

## Next Steps

1. **Phase 1 Implementation:** Hand off to implementation agent
   - Read `FINAL_PHASE1_HANDOFF.md`
   - Implement `tools/analysis/schema.py` (with corrected from_raw_directory)
   - Implement `tools/analysis/metrics.py`
   - Implement `tools/analysis/run_single_analysis.py`
   - Write tests for schema and metrics
   - Verify no C++ files modified

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

**Status:** ✅ Sprint 7 complete — Final Phase 1 handoff ready for execution
