# nmr-render — local visual oracle for the NMR bench scene

A tiny native renderer that turns the 3-D NMR bench into a PNG from the command line, so camera framing,
sample-holder visibility, B0/B1 readability, transparency and the exploded view can be **inspected**, not guessed
from source. It is a development/verification tool — **not** a replacement for the public Three.js site.

```
         canonical geometry  (legacy/simulator/m/*.txt, from the OpenSCAD via build/pack.py)
                /        \
               v          v
   Three.js/WebGL      nmr-render (this tool)
   public site              |
                            v
                           PNG
```

## Why it exists

Visual work on the scene was being judged from source code alone, which is unreliable. This tool makes the loop:
*edit presentation → render → inspect the PNG → revise.*

## Why a CPU rasteriser (not GLFW/OpenGL)

The primary user runs in a headless automation sandbox where a GPU OpenGL context is unreliable. A small software
rasteriser is deterministic and depends only on `stb_image_write.h` (vendored), so it always runs here. It is the
"smallest reliable native pipeline." A GLFW/OpenGL backend could be added later for GPU rendering, behind the same CLI.

## Geometry source (no duplication)

Geometry is **not** defined in this tool. `transcode.mjs` re-containers the canonical packed meshes
`legacy/simulator/m/*.txt` — the exact meshes the Three.js site loads, themselves generated from the OpenSCAD in
`legacy/mechanical/` + `legacy/simulator/build/*.scad` by `pack.py` — into one `assets/scene.bin`. Only
**presentation** (per-part colours, per-preset visibility/opacity, cameras) lives in `src/main.cpp`.

Canonical coordinate frame (unchanged, verified): **+x** = sample / RF-solenoid axis, **+y** = Helmholtz / B0 axis,
**+z** = up.

## Build & render

```sh
node transcode.mjs                       # legacy/simulator/m/*.txt -> assets/scene.bin  (needs Node)
cmake -S . -B build -G Ninja
cmake --build build
./build/nmr-render.exe --preset instrument --scene assets/scene.bin --output artifacts/instrument.png
```

## Presets

| Preset | Question it answers | Shows |
|---|---|---|
| `instrument` | "What is this instrument?" | probe centred, Helmholtz pair around it, holder legible, PCB/wiring hidden, narrow-FOV (low distortion) |
| `physics` | "How does the NMR geometry work?" | **y-up**: B0 (blue) vertical, B1/coil axis (orange) horizontal, pair as two coils — B0 ⟂ B1 obvious |
| `probe` | "What holds the sample?" | close-up of tube, sleeve, former, winding, cradle, tripod |
| `exploded` | "How is it assembled?" | probe stack separated along real axes, cabling to the PCB |

## CLI

`--preset <name>` · `--output <path>` · `--scene <scene.bin>` · `--projection ortho|perspective` ·
`--azimuth <deg>` · `--elevation <deg>` · `--distance <mm>` · `--fov <deg>` · `--width/--height` ·
`--show-fields/--no-fields` · `--show-wiring/--no-wiring` · `--show-pcb/--no-pcb` · `--show-grid/--no-grid` ·
`--no-holder`. Exit code is nonzero on failure.

## Generated vs tracked

Generated (git-ignored): `build/`, `assets/scene.bin`, `artifacts/*.png`.
Tracked: source, `transcode.mjs`, `CMakeLists.txt`, vendored `third_party/stb_image_write.h`, and the reference
renders in `artifacts/render-reference/`.

## Relationship to the WebGL site

Use this tool to iterate camera/visibility fast. Once a composition is chosen, apply the corresponding choices to the
public Three.js page (`legacy/simulator/index.html`) and do a final browser check when browser rendering is available.
A good OpenGL/CPU render does **not** prove CSS layout, WebGL transparency or responsive behaviour are correct.
