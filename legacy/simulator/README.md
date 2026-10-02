# NMR bench simulator (Project 3: `simulator/`)

Part of the Project 3 site: <https://tojestspacja.github.io/nmr-instrument/simulator/>. **Simulation only:** it does not
control the instrument; the real pulse sequence is `runOneScan()` in class-board-2026
`firmware/src/blocks/nmr/Sequencer.cpp` (reference commit in `../hardware/README.md`).


A live 3D view of the whole setup — probe on its tripod inside the B0 Helmholtz pair, cabled to the
housing — with a pulse experiment that solves the Bloch equations for the real design. Open
`index.html` through a web server (it loads the meshes with `fetch`, which a `file://` page cannot):

```sh
py -m http.server 8000      # in this folder, then open http://localhost:8000/
```

## What the pulse experiment computes

- **Fields, exact (default):** precomputed in Mathematica by `build/nmr_exact.wls` and stored in
  `m/fieldmap-tube.json` / `m/fieldmap-bottle.json` (one entry per voxel on a 2.5 mm / 3 mm grid):
  Biot–Savart for the B0 pair with its real 28 × 27 mm winding cross-section (5 × 5 filaments per
  coil) and for every one of the class coil's 400 turns (222 in the first layer, 178 in the second),
  giving each voxel its line offset and the strength and direction of B1 perpendicular to the local B0.
  The page scales B0 with the current and adds the Earth's field along B0. **Approximate** (switch on
  the page, and the fallback if the maps do not load): 4th-order Helmholtz error and the on-axis
  finite-solenoid profile.
- **Pulse:** the class coil driven at 7.95 V pk (5.6 mA at 89.4 kHz); rotating component = half the
  linear field; an exact rotation per voxel about its effective field in the frame rotating at f_tx,
  off-resonance and the B1 direction included.
- **FID:** free precession at each voxel's own offset, intrinsic T2 = 2 s, T1 recovery 2.5 s.
- **Receiver:** reciprocity pickup with the same coil field (and phase), the Q = 10 tank tuned to
  88.47 kHz (C710 only + the coax, `instrument.scad`), mixing to the 5.4 kHz IF, 1.2 ms blanking,
  tank + OPA1656 noise 15.6 nV/√Hz over 15 kHz, ADC gain 1.02 V / 40 µV, 25 kS/s.

### Checked against SpinDynamica

`build/nmr_exact.wls` also runs the pulse on every voxel with SpinDynamica 3.7.1 (C. Bengs and
M. H. Levitt, *Magn. Reson. Chem.* 2017, doi:10.1002/mrc.4642) and computes the FID and spectrum
independently. Same voxels, f_tx 89.4 kHz, 417 µs, 1.5 A, no Earth field:

| | tube: `physics.js` | tube: SpinDynamica | bottle: `physics.js` | bottle: SpinDynamica |
|---|---|---|---|---|
| coil emf, µV pk | 0.902 | 0.902 | 1.643 | 1.643 |
| proton line, Hz | 89 559 | 89 560 | 89 559 | 89 560 |
| linewidth FWHM, Hz | 6 | 6 | 6 | 6 |
| T2* (envelope to 1/e), ms | 79.9 | 80.5 | 41.0 | 41.5 |
| 90° at the coil centre, µs | 417.4 | 417.4 | 417.4 | 417.4 |

What it shows about the design: at 1.5 A the pair gives **2.1035 mT**, not the thin-wire formula's
2.0997 mT, so the line sits at **89 560 Hz**, 160 Hz above the default f_tx (retune, or 1.497 A);
the 90° pulse at the coil centre is **417 µs** as the workbook says (the second layer is only 178
turns long, which the approximate model's even winding misses and puts at 451 µs); the 50 mL tube
gives **0.90 µV** at the coil, **8.8 µV** at the tank, a fifth of the 40 µV the receiver was sized
for (single-sample SNR about 4; the bottle 15.9 µV); T2* is **80 ms** in the tube, **42 ms** in the
bottle (it reaches the coil ends), line 6 Hz; the Earth's field along B0 moves the line 42.6 Hz per µT.

### Recomputing the field maps

Needs Mathematica (13.3 used) and SpinDynamica 3.7.x (free, <https://www.spindynamica.soton.ac.uk/downloads/>;
the newest release needs Mathematica 14.3). Point `SPINDYNAMICA` at its folder (the one holding
`SpinDynamica/`), then:

```sh
cd build
wolframscript -file nmr_exact.wls 2.5 Infinity tube      # about 5 min; writes exact-tube-*.json/.png
wolframscript -file nmr_exact.wls 3 Infinity bottle      # about 5 min
py compact.py tube ../m/fieldmap-tube.json
py compact.py bottle ../m/fieldmap-bottle.json
```

## Files

| File | What it is |
|---|---|
| `index.html` | the page (three.js r147 and pako from CDNs) |
| `physics.js` | the simulation, pure functions; `build/test-physics.js` runs it in Node |
| `m/*.txt` | the meshes: gzip(float32 positions + uint32 indices), base64 |
| `m/fieldmap-*.json` | the exact field maps, with the SpinDynamica results they were checked against |
| `build/nmr_exact.wls`, `build/compact.py` | the Mathematica / SpinDynamica computation and its packer |
| `build/exp-probe.scad`, `build/exp-housing.scad` | one component per run, in `instrument.scad`'s frame |
| `build/pack.py` | ASCII STL → the `m/*.txt` files |

## Rebuilding the meshes after a design change

```sh
cd build      # sources: ../../mechanical/*.scad; board meshes: ../../../class-board-2026/hardware/release/
# one STL per component (OpenSCAD on PATH); the list is in index.html, PARTS
openscad -o meshes/cradle.stl -D 'comp="cradle"' exp-probe.scad      # ... and so on
openscad -o meshes/box.stl    -D 'comp="box"'    exp-housing.scad
py pack.py                  # writes ../m/*.txt
node test-physics.js        # the physics numbers
```

The checks list in `index.html` quotes `instrument.scad`'s echoes; update it when they change.
