# Account-Switch Handoff Package

## Purpose

Continue the benchtop NMR rewrite after an account/session interruption without restarting work, losing uncommitted files, or inheriting unsupported claims. All handoff documentation is in English.

This package preserves project intent, prior agent reports, recorded implementation progress, the interruption point, and a safe recovery procedure. It does not transfer credentials, hidden model state, running agents, or processes.

## Critical source discrepancy

**The supplied `nmr-instrument.zip` is not the `v2` working tree described in the attached session transcript.**

The ZIP contains `main` at `0914edbc5f9424503e6046394dd632a815474fc3`, the public-site/simulator/OpenSCAD layout, and no `v2` reference. The transcript records later canonical configuration, a C++ core, pulse programming, Python/WASM integration, tests, and partial ESP-IDF firmware.

This does not prove that the user's live Windows folder lost any work. The actual folder may still contain `v2`, including valuable uncommitted firmware. Inspect and protect that folder first.

## Installation on the original computer

1. Pause other agents writing to the same working tree. Do not assume logging out stopped child processes.
2. Preserve an access-controlled backup/checkpoint of the live source, including required untracked work and local-only evidence. Review ignored files and secrets separately.
3. Extract this package into a staging folder.
4. Copy **only** `RESUME_AFTER_ACCOUNT_SWITCH.md` and `handoff/2026-10-03/` into the live `nmr-instrument` root. Compare rather than blindly overwrite any existing destination.
5. Do **not** extract the old source ZIP over the live project or replace its `.git` directory.
6. Open the live project with the new session and paste `RESUME_PROMPT.md`.

Expected placement, based on the historical local path:

```text
C:/Users/Liang/tigp-2026/nmr-instrument/
├── RESUME_AFTER_ACCOUNT_SWITCH.md
├── handoff/2026-10-03/
│   ├── CURRENT_STATE.md
│   ├── RECOVERY_PLAN.md
│   ├── NEXT_ACTIONS.md
│   ├── ...
│   └── capture_state.py
└── <all existing project files, unchanged>
```

That Windows path is a locator from the transcript, not a live directory inspected during package preparation.

## Read order

Start with the repository-root entry file, then `CURRENT_STATE.md`, `RECOVERY_PLAN.md`, `NEXT_ACTIONS.md` and `OPEN_ISSUES.md`. Consult `DECISION_LOG.md` and `TEST_EVIDENCE.md` before changing architecture or interpreting a historical test claim. `TRANSCRIPT_INDEX.md` locates targeted source evidence.

The previously drafted `PROJECT_CONTRACT.md`, `AGENT_PLAN.md` and traceability documents remain useful intent if installed in the live tree. Do not assume they exist merely because they appeared in chat, and do not let an old example silently revert a later implemented change.

## Optional read-only diagnostic

From the live project root:

```powershell
py handoff/2026-10-03/capture_state.py --root .
```

The script prints JSON. It does not modify source, Git refs/index/config, credentials or the checkout; it does not access the network, install dependencies, build, flash or operate the instrument. Exit code `3` means expected v2 continuation files are not sufficiently present, not that an NMR test failed.

## Limits

Missing v2 source has **not** been silently recreated or installed. The evidence directory is historical text, not a verified source backup. The user's current Windows processes, local branches, remote state and board connection must be checked by the receiving agent.

No original implementation file was changed. Creating a handoff does not promote any NMR validation state.
