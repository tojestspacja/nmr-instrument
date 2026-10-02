# nmr-instrument — notes for Claude

A from-scratch benchtop NMR instrument (Project 3). Active work is on branch **`v2`**; the public site is on `v2`
(GitHub Pages). The original build walkthrough and 3-D bench live under `legacy/` and are first-class — integrate with
them, don't bury or replace them.

## Verified physics geometry — do not "fix" for visual reasons

Checked against the hardware (CAD engraving "AXIS 90° TO B0", `exp-probe.scad`, `physics/`): **B0 axis = y**
(Helmholtz pair), **sample axis = RF-solenoid axis = x**, **B1 ≈ x at the sample**, **B1 ⟂ B0**. One physical RF coil
does TX and RX; I/Q is demodulation, not two orthogonal coils. Do not rotate the probe, invent a second coil, or change
B0/B1 for presentation reasons.

## Visual changes to the 3-D scene — render and look, don't guess

For any change to the NMR 3-D scene, camera, visibility, transparency, material, labels or assembly presentation, do
**not** judge the result from source alone. Use the local renderer and inspect the PNG:

```sh
cd tools/nmr-render
node transcode.mjs                 # refresh assets/scene.bin from legacy/simulator/m/*.txt (only if meshes changed)
cmake --build build
./build/nmr-render.exe --preset instrument --output artifacts/instrument.png   # then read the PNG
```

Presets: `instrument` (hero), `physics` (B0⟂B1, y-up), `probe` (sample holder), `exploded` (assembly). The renderer
reuses the canonical meshes (`legacy/simulator/m/*.txt`); it does not define geometry. See `tools/nmr-render/README.md`.
After choosing a composition there, apply it to the public Three.js page (`legacy/simulator/index.html`) and do a final
browser check when browser rendering is available.

## Canonical sources

- Physical quantities: `design/instrument.yaml` → `generated/*` (via `tools/gen_config.py`; never hand-edit `generated/`).
- 3-D meshes: the OpenSCAD in `legacy/mechanical/` + `legacy/simulator/build/*.scad` → packed `legacy/simulator/m/*.txt`
  (by `pack.py`), loaded by both the website and `nmr-render`.
- Firmware/core: shared C++ in `physics/`, `pulse/`, `firmware/`; the class board's firmware (`class-board-2026`) stays
  authoritative for the real board.
