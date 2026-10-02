# mechanical/: the OpenSCAD project

The housing, the probe, the B0 coil pair, the whole bench and the printable data model, as one
OpenSCAD project with one set of shared numbers. The story of how it was designed is on the project
site: <https://tojestspacja.github.io/nmr-instrument/>.

The files stay together in one folder because they include each other (`include <nmr-params.scad>`,
`use <housing.scad>`, `cut-*.scad` on the 3D files); splitting them would mean rewriting those links.
The flat-part exports go to `../fabrication/`.

| File | What it is |
| --- | --- |
| `nmr-params.scad` | the shared numbers: firmware defaults, the coil, water, copper. Change them here and nowhere else |
| `housing.scad` → `base-shell.stl`, `hood.stl`, `coupon.stl`, `bottom-plate.dxf`, `housing.png` | the printed housing; `part` = `shell`, `hood`, `coupon`, `plate` |
| `probe.scad` → `probe-former.stl`, `probe-sleeve.stl`, `probe-cradle.stl`, `probe.png`, `probe-b0.png` | coil former, 50 mL sample sleeve, tripod cradle; the B0 pair as a reference design |
| `instrument.scad` → `instrument.png` | the whole bench and its cross-checks: cables, RX tank, B0 drive and drift, steel near the sample, TX/RX |
| `boards.scad` | the two boards stacked, from the housing's own numbers |
| `spectrum.py` → `spectrum-data.scad` | the probe simulator (field over the sample, dead time, tank, coil heating), or real records from `instrument nmr --csv` |
| `spectrum.scad` → `spectrum.stl`, `spectrum.png` | the printable data model (simulated data) |
| `cut-common.scad`, `cut-housing.scad`, `cut-probe.scad`, `cut-spectrum.scad`, `export_cut.py` | every part flat for the laser and the water-jet; `py export_cut.py` writes `../fabrication/` |
| `img/` | renders for the project site |

## The board meshes

The housing is built round the boards' meshes, which belong to class-board-2026
(`hardware/release/`) and are not copied here. `housing.scad`'s `board_dir` reads them from a checkout
of class-board-2026 beside this repository; the reference commit is in `../hardware/README.md`. Only
previews need them; no exported part does.

## The course hand-in

The course collects the housing from class-board-2026 (`docs/students/yi-tsai/housing/`). That copy is
generated from this folder by `py ../tools/handin.py`; edit here, never there.
