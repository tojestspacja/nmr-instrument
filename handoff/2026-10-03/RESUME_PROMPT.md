# Prompt to Paste Into the New Coding-Agent Session

You are taking over an existing benchtop NMR instrument rewrite after an account/session interruption. Continue the existing work; do not restart the project or merely produce another proposal.

Open the authorized live `nmr-instrument` folder. The previous session used `C:/Users/Liang/tigp-2026/nmr-instrument`; verify the actual path.

Read `RESUME_AFTER_ACCOUNT_SWITCH.md`, then `handoff/2026-10-03/CURRENT_STATE.md`, `RECOVERY_PLAN.md`, `NEXT_ACTIONS.md`, `OPEN_ISSUES.md`, `DECISION_LOG.md`, `TEST_EVIDENCE.md` and `AGENT_TASKS.md`. Inspect existing live project instructions and actual ADRs too.

Critical discrepancy: the uploaded ZIP inspected for this handoff is legacy `main` at `0914edbc5f9424503e6046394dd632a815474fc3`. The attached execution record describes later v2 canonical configuration, shared C++ physics/DSP, pulse IR/compiler/backend, Python/WASM interfaces, tests and partial ESP-IDF firmware. Those v2 files are absent from that ZIP. They may still be in the live checkout, another worktree, local commits or a backup. Never overwrite newer live files or `.git` with the old archive.

Before editing, inspect actual branch/HEAD, relevant refs/worktrees, staged/unstaged changes, untracked source, saved evidence and installed toolchains. Use the included `capture_state.py` as a read-only aid. Protect WIP and select the recovery case. Do not reset, clean, force-push, rerun the legacy migration, or change account credentials. Do not treat old agent/task IDs as portable state.

The final recorded operation registered `firmware/components/rf` and checked I2C API symbols. Gate-engine and ADC-streamer source had been written. The firmware main/control loop and a planned I/Q sampling-skew correction are the continuation area. The transcript does not establish a complete new-firmware build or physical measurement. Check for newer live changes before duplicating work.

Preserve the physics-driven instrument mission and useful features. Keep archive observations, historical reports, live tests, estimates and measurements distinct. Do not repeat the complete legacy audit unless inputs changed or evidence is missing. Do not label proposed T/R, B0, phase-reference wiring or I2S ADC changes as bench-validated.

Report a compact recovery handshake: actual workspace/source state, v2 work available, transcript-only claims, baseline checks, open blockers, write ownership and exact next action. Then implement and test the next safe task in `NEXT_ACTIONS.md` and save a checkpoint. Do not stop at a summary. If the v2 source truly cannot be located, use the isolated, provenance-tracked reconstruction path in `RECOVERY_PLAN.md`; do not invent missing bytes or results.

Do not flash hardware, energize an unverified setup, publish the site, order boards or push remotely without the relevant explicit authorization. Save important reports and logs to durable project files so the next session does not depend on this account's memory.
