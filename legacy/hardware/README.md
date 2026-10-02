# Hardware and firmware

The instrument's electronics, firmware and on-board user interface are **not** in this repository.
They are developed in the shared course repository `class-board-2026`, which the instructor and other
students also change, and that repository is the source of truth for them. Nothing here copies them:
no firmware, no PCB project, no second version of the pulse sequence.

## Project 3 reference version

| | |
| --- | --- |
| Repository | [TIGP-Experimental-Methods/class-board-2026](https://github.com/TIGP-Experimental-Methods/class-board-2026) |
| Branch | `main` |
| Commit | [`eef00d20a10deca994a8efe07c588c22af2b9db0`](https://github.com/TIGP-Experimental-Methods/class-board-2026/tree/eef00d20a10deca994a8efe07c588c22af2b9db0) |
| Date | 2026-10-02 |

Everything in this repository was designed and checked against that commit: the housing against its
board meshes, and the simulator and documentation against its firmware. My working branch
`w1-yi-tsai` contains this commit and differs from it only under `docs/students/`.

| What | Path in class-board-2026 (links are pinned to the reference commit) |
| --- | --- |
| Main PCB | [`hardware/class-board.kicad_pro`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/hardware/class-board.kicad_pro) |
| Front-panel PCB | [`hardware/front-panel/front-panel.kicad_pro`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/hardware/front-panel/front-panel.kicad_pro) |
| Board meshes the housing is built round | [`hardware/release/`](https://github.com/TIGP-Experimental-Methods/class-board-2026/tree/eef00d20a10deca994a8efe07c588c22af2b9db0/hardware/release) (`class-board.stl`, `front-panel.stl`, `.step`) |
| NMR pulse sequencer | [`firmware/src/blocks/nmr/Sequencer.cpp`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/firmware/src/blocks/nmr/Sequencer.cpp), one scan in [`runOneScan()`, lines 386–500](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/firmware/src/blocks/nmr/Sequencer.cpp#L386-L500) |
| NMR firmware contract and default settings | [`firmware/NMR-FIRMWARE.md`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/firmware/NMR-FIRMWARE.md) |
| Commands and record format | [`firmware/PROTOCOL.md`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/firmware/PROTOCOL.md) |
| NMR instrument web UI (served by the ESP32) | [`host/pwa/panels/nmr.js`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/host/pwa/panels/nmr.js) |
| Command-line client (`instrument nmr --csv`) | [`host/instrument.py`](https://github.com/TIGP-Experimental-Methods/class-board-2026/blob/eef00d20a10deca994a8efe07c588c22af2b9db0/host/instrument.py) |

## How a real scan runs

The pulse program runs on the ESP32. `runOneScan()` in `Sequencer.cpp` does this for each scan:

1. optional pre-polarization (`polarize_ms`, then `t_polarize_settle_ms`);
2. sets this scan's pulse phase;
3. blanks the receiver, then holds `TX_EN` high for the 90° pulse (`t90_us`); in echo mode it waits
   `tau_us` and gives the 180° pulse;
4. waits out the dead time (`t_dead_us`), while the coil is still ringing, then releases `RX_BLANK`;
5. starts the ADC burst at `t_acq_start_us` after the pulse and records for `t_acq_ms`.

The scan set repeats this `n_avg` times, `t_repeat_ms` apart, and cycles the pulse phase between
scans (CYCLOPS) when `cyclops` is on. The default values of these settings are in `NMR-FIRMWARE.md` at
the reference commit; they are deliberately not copied here, so they cannot go stale.

Note that class-board-2026's own guidance (`CLAUDE.md`, "Real-hardware code is unverified until
measured") treats the real-hardware branch of this code as a hypothesis until `hardware/docs/bring-up.md`
says otherwise; only its SIM mode has run.

## Three interfaces, three jobs

```text
Course showcase card ──────────► "What is this project?"   (one card, links here)

Public browser (GitHub Pages)
   └─► this site + /simulator/  ──► "How is it built, how does it work?"   simulation only

Phone or laptop on the local Wi-Fi
   └─► instrument-XXXX.local    ──► "Operate the real instrument"
          └─► ESP32 ─► TX / RX / ADC hardware
```

The public site cannot drive the board: it is served over HTTPS from the internet, and the
instrument is a plain `ws://` device on your local network. Real scans are started from
`instrument-XXXX.local` → **NMR** → **Apply settings** → **Start** (or **Pulse** for a single test
pulse), or from a laptop with `instrument nmr --n-avg 8 --csv fid.csv`.

## Building this repository's CAD against the boards

The OpenSCAD files in `../mechanical/` read the board meshes from a sibling checkout:

```text
tigp-2026/
├── class-board-2026/      (at the reference commit, or a later one)
└── nmr-instrument/        (this repository)
```

Only previews use them (`boards.scad`, the housing's assembly view, the simulator's mesh build);
none of the exported parts depends on them. If the boards change in `class-board-2026`, re-check the
housing against the new meshes and update the reference commit above.
