# Source Manifest and Provenance

## Inputs actually used

| Input | Size | SHA-256 | Role |
|---|---:|---|---|
| `nmr-instrument.zip` | 13,750,572 bytes | `5b47c19c3b04185bcf76827f23cc0bc47bf2260fd1638702b030e88a8a95b8ba` | Available project bytes and archived Git metadata |
| `Pasted text.txt` | 654092 bytes | `c4f56a1a92747fb1d7b77dd57802df4431ef472958be1cf2dc8c7db71106579d` | 10,747-line historical execution record |

The mounted ZIP is the received file at the supplied path. Repeated uploads with the same filename cannot be distinguished by title alone; the SHA-256 identifies the exact bytes inspected.

The available `class-board-2026 (2).zip` was not used for a fresh electronics audit in this handoff. Hardware findings here are attributed to the prior agent handbacks. No live GitHub, account or Windows-machine state was fetched.

## Archive observations

Direct inspection established branch `main`, HEAD `0914edbc5f9424503e6046394dd632a815474fc3`, and no v2 branch reference or successor working directories. All 160 non-Git files match the archived index after CRLF/LF normalization; 48 match byte-for-byte and 112 have only line-ending differences. Original source bytes were not normalized or altered.

Raw metadata queries are retained in `evidence/archive-git-observation.json`. Source entry/README excerpts are in `evidence/archive-source-excerpts.txt`. The full file fingerprint list is `snapshot-manifest.json`. That list is observation evidence, not an instruction to overwrite a newer local tree.

## Transcript evidence

`evidence/prior-session-rebuild.txt` preserves original lines 3883–10746 with original line labels. The final original line 10747 is the usage-limit message; the handoff records the interruption without treating its reset time as a present schedule.

The three decoded Markdown handbacks come from original lines 4390, 4398 and 4406. They preserve the earlier agents' wording and uncertainty. They have not been independently validated by this package author.

`TRANSCRIPT_INDEX.md` and `transcript-write-index.json` locate source writes and subsequent work. They are not complete, final source snapshots. A textual Write event proves only that the session record contains a write, not that its final bytes were supplied here.

## Generated package

All new operational documents and `capture_state.py` are handoff aids. Initial statuses are observations or historical reports. The `CHECKPOINT_TEMPLATE.md` intentionally contains UNKNOWN fields for future live inspection; this is the only unfilled status template. Recovery/next-task proposals are explicitly distinguished from completed work.

No existing project source, Git history or physical hardware was changed. The package deliberately does not include a replacement `.git`, a fabricated v2 source tree, credentials, the unrelated chemical inventory, or a claim of NMR measurement.
