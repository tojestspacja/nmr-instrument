# Resume the NMR Rewrite After an Account Switch

Read this before editing.

The handoff was prepared from `nmr-instrument.zip` and the attached execution transcript. **They describe different states.** The ZIP is `main` at `0914edbc5f9424503e6046394dd632a815474fc3`; the transcript records later v2 work through partial ESP-IDF firmware. This package is not a verified backup of that missing working tree.

Read [Current state](handoff/2026-10-03/CURRENT_STATE.md), follow [Recovery plan](handoff/2026-10-03/RECOVERY_PLAN.md), then use [Exact next actions](handoff/2026-10-03/NEXT_ACTIONS.md), [open issues](handoff/2026-10-03/OPEN_ISSUES.md) and [test evidence](handoff/2026-10-03/TEST_EVIDENCE.md).

The recorded original path is `C:/Users/Liang/tigp-2026/nmr-instrument`; verify it. Do not overwrite it with the old ZIP, replace its `.git`, discard WIP, or replay the legacy migration.

The stopping point is **firmware integration after RF component registration**, not “finish the YAML generator.” The latter was followed by native/Python tests, WASM work, backend planning and several firmware components in the transcript.

The [copy-paste prompt](handoff/2026-10-03/RESUME_PROMPT.md) contains takeover instructions. [Installation](handoff/2026-10-03/README.md) explains safe placement. [Agent assignments](handoff/2026-10-03/AGENT_TASKS.md) define focused recovery, integration and review roles.

After recovery, continue actual implementation and tests. Save the next checkpoint using the [template](handoff/2026-10-03/CHECKPOINT_TEMPLATE.md). An account/session change must not erase progress or turn unmeasured claims into facts.
