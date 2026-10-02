# Recovery Plan — Preserve First, Resume Second

## A. Inspect the actual live checkout

Start with the known project locations, if present:

```text
C:/Users/Liang/tigp-2026/nmr-instrument
C:/Users/Liang/tigp-2026/class-board-2026
```

Do not search unrelated personal folders by default. Record actual paths and permissions.

Use read-only queries first:

```powershell
git status --short --branch
git branch -a
git rev-parse HEAD
git log -12 --oneline
git worktree list --porcelain
git reflog --all -20
git diff --stat
git diff --cached --stat
```

Enumerate untracked source and inspect the expected paths from `TRANSCRIPT_INDEX.md`. These are investigation steps, not claims that a branch or commit exists.

Do not start with pull, checkout/switch, reset, clean, restore, stash, generation or bulk moves. A stale-looking branch can coexist with important uncommitted files.

## B. Select the observed recovery case

### Case 1 — v2 working files and WIP exist

Keep the current tree. Identify committed, staged, unstaged and untracked changes. Preserve required source and evidence in an access-controlled backup/checkpoint before editing. A local WIP commit is an option only after inspecting its staged contents; do not commit everything automatically or push as part of recovery.

Use the transcript to understand intent, but prefer inspected live file contents over an earlier recorded write. Rerun targeted baseline checks and continue at the firmware interruption point.

### Case 2 — v2 exists in another worktree or only as a ref

Inspect the worktree read-only and open the correct one rather than changing the wrong directory. If only a branch/ref exists, first preserve the dirty current tree, then materialize a recovery checkout in an isolated location. Never force a branch out from under another active session.

Look for `b61c1e9`, `1223c06`, `de1a13f` and the WASM commit message. Do not invent full hashes. Recover working changes beyond the latest commit separately.

### Case 3 — only the old archive is available

Do not claim to continue directly from M4 source that is absent. Preserve the archive and its Git data. Check an authorized local backup, known worktree or separately exported WIP snapshot. A remote lookup requires existing authorization and does not establish that v2 was pushed.

If source cannot be recovered there, reconstruct **only missing work** in an isolated recovery tree from `evidence/prior-session-rebuild.txt`. It includes many complete Write bodies and later shell edits. `transcript-write-index.json` locates them; a first Write is not necessarily the final version.

For each recovered file, record source ranges and provenance, reconcile later Edit/Bash/sed/Python replacements, and flag omitted patch bodies. Never execute the transcript as a script. Regenerate derived files only after reviewing the recovered canonical inputs and generator. Build and test before assigning a restored status.

Use `TRANSCRIPT_RECONSTRUCTED_UNVERIFIED` until checks pass. Do not claim exact recovery of omitted bytes, binaries, local-only measurements or process state.

### Case 4 — mixed or conflicting versions

Preserve both. Compare content and provenance, not filenames or modification times alone. Record the discrepancy and its effect on the next task. Continue nonconflicting work without erasing evidence to create a clean-looking tree.

## C. Recover local-only evidence

The session used temporary audit directories below `C:/Users/Liang/AppData/Local/Temp/claude/` and reported copying some outputs to `validation/legacy-audit/`. Verify whether the real netlists, ERC/DRC files, build logs and scripts are preserved in the live v2 tree.

The decoded `evidence/legacy-*-audit.md` files are reports, not the original raw evidence. Preserve their uncertainty labels.

Do not treat old agent IDs or background task IDs as transferable handles. Inspect actual process state before rerunning downloads or stopping anything. A usage-limit interruption does not prove whether child processes completed or stopped.

## D. Recovery handshake

Before substantive edits, report:

```text
Actual workspace:
Branch and HEAD:
Working changes and untracked source protected:
Recovery case:
V2 source found or reconstructed:
Baseline commands/results:
Missing artifacts:
Active writer ownership:
Exact next task:
```

Then continue the next safe implementation task; do not stop after a status summary. Request only genuinely missing authorization or an irreducible decision.

## Forbidden shortcuts

Do not copy the old archive's `.git` over a live repository; run reset/clean/force-push/broad restore to match a transcript; or replay the legacy migration. Do not flash hardware, order PCBs, publish the site, or change account access as part of account recovery without the relevant authorization.
