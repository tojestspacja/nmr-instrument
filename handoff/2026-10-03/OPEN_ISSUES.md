# Open Issues for Continuation

> This is a handoff issue register, not a fresh scientific/electrical audit. Issues derived from old agents remain historical until checked against the recovered source. Proposed owners are roles, not active agents. Nothing here is marked bench-validated.

| ID | Severity | Status | Issue |
|---|---|---|---|
| HANDOFF-001 | blocker | SOURCE_RECOVERY_REQUIRED | The uploaded archive lacks the v2 working tree |
| HANDOFF-002 | major | EVIDENCE_RECOVERY_REQUIRED | Raw v2 validation artifacts are absent from the ZIP |
| FW-RESUME-01 | blocker | TRANSCRIPT_REPORTED_WIP | Firmware main/control integration is the interrupted continuation area |
| FW-IQ-001 | major | TRANSCRIPT_REPORTED_PENDING | I/Q aperture-skew correction was announced but not shown completed |
| FW-ADC-001 | blocker | PROPOSED_NOT_BENCH_VALIDATED | I2S/DMA ADC framing needs protocol and timing verification |
| SYS-PHASE-001 | blocker | PROPOSED_NOT_BENCH_VALIDATED | TX/LO/acquisition phase-reference closure remains unmeasured |
| HW-TXR-001 | blocker | LEGACY_REPORTED_OPEN | Shared-coil transmit/receive isolation remains an instrument blocker |
| HW-B0-001 | blocker | LEGACY_REPORTED_OPEN | B0 current/field stability has not been experimentally closed |
| HW-SAFE-001 | critical | LEGACY_REPORTED_OPEN | Fault wiring, LNA feedback and power protection need source-specific closure |
| SIM-PARITY-001 | major | REVALIDATION_REQUIRED | WASM parity must be re-established after later config edits |
| DOCS-ADR-001 | major | AVAILABILITY_UNKNOWN | Referenced ADRs are not supplied as verified files |
| HW-INTEGRATION-001 | major | BOUNDARY_TO_DOCUMENT | Experimental successor firmware and course-source ownership need an explicit boundary |
| NMR-EVIDENCE-001 | blocker | NOT_EVIDENCED | Real FID/spectrum/echo validation is still not evidenced |

## HANDOFF-001 — The uploaded archive lacks the v2 working tree

**Basis:** Direct ZIP/Git inspection: main at 0914edb, no v2 ref or successor directories.

**Next action / closure:** Recover the live v2 tree and uncommitted firmware, or reconstruct missing source in isolation with provenance. Do not overwrite newer source.

**Evidence:** CURRENT_STATE.md; snapshot-manifest.json. **Suggested owner:** Recovery owner.

## HANDOFF-002 — Raw v2 validation artifacts are absent from the ZIP

**Basis:** The transcript reports saving outputs; the source snapshot has no validation tree.

**Next action / closure:** Recover raw logs/netlists or rerun the relevant checks. Preserve historical reports without promoting them.

**Evidence:** TRANSCRIPT L4575-L4597; evidence/legacy-*-audit.md. **Suggested owner:** Verification owner.

## FW-RESUME-01 — Firmware main/control integration is the interrupted continuation area

**Basis:** Gate engine, ADC and RF writes are recorded; next control-loop work was announced.

**Next action / closure:** Inspect live files for subsequent work, finish missing integration, and produce a complete build with raw exit status.

**Evidence:** TRANSCRIPT L10383-L10746. **Suggested owner:** Integration builder.

## FW-IQ-001 — I/Q aperture-skew correction was announced but not shown completed

**Basis:** Last ADC handback proposes a fractional-delay correction, with a prior estimate of about 7.8 degrees.

**Next action / closure:** Verify conversion/channel timing; implement shared correction once; test sign, phase, transients and replay.

**Evidence:** TRANSCRIPT L10584; NEXT_ACTIONS.md. **Suggested owner:** ADC/phase reviewer.

## FW-ADC-001 — I2S/DMA ADC framing needs protocol and timing verification

**Basis:** New streamer and 125 kS/s / decimation 5 config appear as source writes, not hardware throughput evidence.

**Next action / closure:** Verify ADC framing, startup latency, channel mapping, CS timing, loss detection and actual first-sample timing before accepting acquisition.

**Evidence:** TRANSCRIPT L9146-L9208; L10384-L10584. **Suggested owner:** ADC/phase reviewer.

## SYS-PHASE-001 — TX/LO/acquisition phase-reference closure remains unmeasured

**Basis:** Legacy audits raise coherence concerns; v2 proposes capture inputs labeled ECO-1.

**Next action / closure:** Confirm actual wiring and missing-input handling; validate timestamps and coherent known-tone acquisition before NMR averaging claims.

**Evidence:** evidence/legacy-firmware-audit.md; TRANSCRIPT L9182-L9184. **Suggested owner:** ADC/phase reviewer.

## HW-TXR-001 — Shared-coil transmit/receive isolation remains an instrument blocker

**Basis:** Historical electronics report describes separate TX/RX nets, clamp loading for a shared connection, and no isolating T/R switch.

**Next action / closure:** Check live board/source revision; validate a chosen topology and RX recovery. A calculated diode-drop parameter is not proof of applied hardware.

**Evidence:** evidence/legacy-electronics-audit.md; TRANSCRIPT L5111-L5146. **Suggested owner:** Hardware reviewer.

## HW-B0-001 — B0 current/field stability has not been experimentally closed

**Basis:** Historical report describes voltage-driven operation and absent B0 current sensing at its audited revision.

**Next action / closure:** Verify revised current-drive/sensing design, compliance and drift/noise evidence; do not substitute simulation for measurements.

**Evidence:** evidence/legacy-electronics-audit.md; evidence/legacy-physics-audit.md. **Suggested owner:** Hardware/physics reviewer.

## HW-SAFE-001 — Fault wiring, LNA feedback and power protection need source-specific closure

**Basis:** Later audit reports unwired PA flags, J3-dependent feedback and power-sequencing/protection concerns.

**Next action / closure:** Check actual connectivity and hardware revision; validate safe states before connecting/energizing the real chain. Do not inherit an automatic-abort claim.

**Evidence:** evidence/legacy-electronics-audit.md. **Suggested owner:** Hardware reviewer.

## SIM-PARITY-001 — WASM parity must be re-established after later config edits

**Basis:** One parity run precedes the 125 kS/s/decimation 5 and pin-map changes.

**Next action / closure:** Rebuild native and WASM from the same current sources/config; record passes, tolerances and skips.

**Evidence:** TRANSCRIPT L8996-L9065; L9146-L9208. **Suggested owner:** Verification owner.

## DOCS-ADR-001 — Referenced ADRs are not supplied as verified files

**Basis:** ADR identifiers appear in code/comments; no ADR directory is present in the archive.

**Next action / closure:** Find live ADRs; reconstruct missing rationale explicitly as retrospective records, never as fabricated historical approvals.

**Evidence:** DECISION_LOG.md; snapshot-manifest.json. **Suggested owner:** Continuity owner.

## HW-INTEGRATION-001 — Experimental successor firmware and course-source ownership need an explicit boundary

**Basis:** Archive README assigns real hardware/firmware to class-board; later v2 begins successor firmware in nmr-instrument.

**Next action / closure:** Document what is legacy-authoritative, experimental successor, and approved for promotion; do not overwrite the course repository.

**Evidence:** CURRENT_STATE.md; DECISION_LOG.md. **Suggested owner:** Systems owner.

## NMR-EVIDENCE-001 — Real FID/spectrum/echo validation is still not evidenced

**Basis:** Archive describes no measurements; later text contains software work and a USB-device observation, not an NMR dataset.

**Next action / closure:** Retain hardware and NMR gates; save raw records and controls when real tests become authorized and available.

**Evidence:** README.md in source archive; TRANSCRIPT L5049-L5063; TEST_EVIDENCE.md. **Suggested owner:** Independent verifier.

## Update rule

Preserve these initial statuses. Record live source/config identity and evidence when advancing an issue. A code commit can resolve an implementation defect without resolving the associated hardware-validation gate. Keep a new live register or carefully merge into an existing one; do not overwrite newer issue state.
