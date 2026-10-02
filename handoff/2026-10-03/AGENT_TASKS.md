# Resumed Agent Assignments

## First wave: recovery owner

Use one agent to establish the actual live tree, backups/WIP, source availability, baseline identity and write ownership. Do not start implementation workers against a possibly stale checkout.

Output: a live checkpoint based on `CHECKPOINT_TEMPLATE.md`, identifying recovery case and the exact next task.

## Subsequent roles — up to three focused agents

| Role | Scope | Allowed ownership | Deliverable |
|---|---|---|---|
| Integration builder | Finish existing M4 main/control integration and build failures | `firmware/main/`, build registration and specifically assigned integration files | Changes, full build logs, error/safe-state tests and remaining gaps |
| ADC/phase reviewer | Independently challenge I2S/ADS8688 framing, aperture timing, skew, capture and faults | Assigned tests/review files or an isolated worktree; no concurrent builder-file edits | Counterexamples, protocol questions, synthetic tests and bench acceptance criteria |
| Continuity/verification owner | Preserve evidence; check native/WASM/config parity and traceability; independently test integration | Handoff/evidence and agreed verification scripts | PASS/FAIL/NOT_RUN/SKIPPED tied to actual source; no self-certified hardware result |

If only one agent is available, do the roles sequentially and acknowledge that verification is not independently authored. Do not invent agents or expect old task IDs to survive.

## Cross-domain questions

Require precise answers for conversion-to-channel mapping, I/Q aperture times, first-sample timestamps, captured phase meaning, missing ECO-input detection, overrun reporting, and whether simulator/firmware correct skew exactly once.

Ask the hardware reviewer to separate actual board connectivity from proposed changes. A current-source design, T/R topology or capture-pin proposal is not applied hardware until evidence says so.

## Ownership and durable handbacks

Each task has a task ID, baseline HEAD/config hash, allowed paths, dependencies, output and acceptance test. Do not let two agents edit the same files or both rewrite shared status. A coordinator checks which revision each report examined before integration.

Every handback must be saved:

```text
Task and scope:
Baseline/final source identity:
Files inspected:
Files changed:
Verified result and raw log:
Failed / skipped / not run:
Unresolved question:
Next exact action:
```

Preserve relevant temporary outputs in stable project evidence paths. Do not include credentials or unrelated personal material.

## Avoid repeated directory audits

The three original reports are already preserved in `evidence/legacy-*-audit.md`. Reuse their historical findings; rerun targeted checks only for changed inputs, missing evidence or contradictions. The immediate goal is recovery and completion of the interrupted integration.

## Background tasks

Use only actual supported execution facilities. Record command, directory, output and observed process state. Do not assume an account switch resumes a child process or makes an old task ID valid. Do not assign a reasoning agent simply to wait for a build.
