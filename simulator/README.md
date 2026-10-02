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

- **Field:** 3000 voxels of the sample in the B0 pair's field (Helmholtz, 4th-order error term) plus
  the Earth's field along B0.
- **Pulse:** the class coil driven at 7.95 V pk (5.6 mA at 89.4 kHz); B1 from the finite-solenoid
  on-axis profile, rotating component = half the linear field; an exact rotation per voxel about
  its effective field in the frame rotating at f_tx, off-resonance included.
- **FID:** free precession at each voxel's own offset, intrinsic T2 = 2 s, T1 recovery 2.5 s.
- **Receiver:** reciprocity pickup with the same coil profile, the Q = 10 tank tuned to 88.47 kHz
  (C710 only + the coax, `instrument.scad`), mixing to the 5.4 kHz IF, 1.2 ms blanking, tank +
  OPA1656 noise 15.6 nV/√Hz over 15 kHz, ADC gain 1.02 V / 40 µV, 25 kS/s.

What it shows about the design: the 90° pulse at the coil centre is **451 µs**, not the workbook's
417 µs (the coil is only 2.4 diameters long); the 50 mL tube gives **8 µV** at the tank, a fifth of
the 40 µV the receiver was sized for (single-sample SNR about 4); T2* is about **72 ms** (6 Hz line);
the Earth's field along B0 moves the line 42.6 Hz per µT.

## Files

| File | What it is |
|---|---|
| `index.html` | the page (three.js r147 and pako from CDNs) |
| `physics.js` | the simulation, pure functions; `build/test-physics.js` runs it in Node |
| `m/*.txt` | the meshes: gzip(float32 positions + uint32 indices), base64 |
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
