# Assembly order & bill of materials

How to build the instrument from the fabrication files, in order. The detailed parts list (every laser/water-jet piece,
with material, quantity and file) is generated into [`../fabrication/CUT-LIST.md`](../fabrication/CUT-LIST.md) by
`legacy/mechanical/export_cut.py`; the printed parts come from `legacy/mechanical/*.scad` via OpenSCAD. Design rationale
and the 60 mm active-region decision are in [`design-closure.md`](design-closure.md). Frame: **+x** coil/sample axis,
**+y** B0 (Helmholtz) axis, **+z** up.

> Design stage — nothing here has been built or measured. No steel anywhere near the sample or coil (use brass/nylon
> fasteners), and check every cable and plug with a phone magnetometer before it goes near the probe.

## Bill of materials (summary)

**Printed (PLA/PETG)** — `OpenSCAD -o <part>.stl -D 'part="…"' probe.scad`:
- `former` ×1 (85 mm, 266-turn / 60 mm winding), `sleeve` ×1, `cradle` ×1. Housing shell + hood if printing the housing.

**Laser-cut (3 / 6 mm acrylic)** and **water-jet (6 mm polycarbonate, B0 rings)** — see `fabrication/CUT-LIST.md`:
- Probe: cheeks ×4 (2 plain + 2 lead), sleeve flange, cradle base/saddles/anchor/nut-plate.
- Magnet: B0 ring flanges + spacers (water-jet), B0 rails ×2 + braces ×2, baseplate.
- Housing: cover/walls or printed shell, bottom plate, OLED hood frames, spectrum card.

**Bought:**
- Former tube 40×2 mm (~95 mm), sleeve tube 35×2 mm (~95 mm), 50 mL centrifuge tube (the sample).
- Class coil: AWG26 enamelled, **~45 m** (34 m needed for 266 turns). B0 pair: AWG16, **784 m (~9.2 kg)**.
- Nylon M4/M6 threaded rod + nuts (B0 ring stacks, 8/coil), M3 brass/nylon screws (cradle, housing), **brass ¼"-20 nut**
  (tripod), M3×12 ×4 + M3×10 + nut ×8 (housing). Solvent cement for acrylic. Two-core screened cable (probe), RG174 (RX).

## Assembly order

1. **Probe.** Glue the two cheeks (2× 3 mm laser pieces each) onto the former tube at 0–6 mm and the lead-end position.
   Wind **266 turns AWG26 in two layers** over the 60 mm length (start lead out through the lead cheek, tensioned, a drop
   of cyanoacrylate on the first turns; both leads leave at the stub). Cable-tie the leads + cable to the stub.
2. **Sample fit.** Push the sleeve into the former bore; the sleeve flange seats against the plain cheek. The 50 mL tube
   goes in cap-first until the **cap stops at the flange** — this is the axial datum that returns the sample to the
   active ROI every time (the winding z 6–66 is covered by the sample; the tube overhangs 16 mm, which is fine).
3. **Cradle.** Assemble the laser cradle (base + two saddles + anchor + nut-plate), press the **brass ¼"-20 tripod nut**
   into the nut-plate. The base is engraved **"AXIS 90° TO B0 | EARTH FIELD: E–W"** and marks the winding centre.
4. **Magnet.** For each coil, stack flange + spacers + flange on 8 nylon rods and wind **312 turns AWG16**. Seat both
   ring stacks in the two **rails** (notches fix the 200 mm Helmholtz spacing) and tie the rails with the **braces**.
5. **Base/frame.** Fix the two rails to the **baseplate**; mount the probe cradle (on its tripod) to the baseplate so the
   sample ROI sits at the magnetic centre (x = y = 0). This ties magnet + probe to one reference (verified clear: probe
   fits the ring aperture, rings clear the plate by 10 mm — `cut-probe.scad` clearance echo).
6. **Electronics.** Mount the board stack in the housing (M3 standoffs from the cover; bottom plate on the nut-pocket
   bosses; feet underneath). Place the housing **≈450 mm from the sample** (RF/magnet isolation; nearest steel then
   ≥300 mm) with its TX-COIL edge facing the probe.
7. **Cables.** Route TX (two-core → TX COIL terminal), RX (RG174 → RX SMA on the cover), B0 (AWG18 pair → H-BRIDGE). Keep
   RF leads short; zip-tie the probe end to the stub anchor and the housing end to the **strain-relief bar** so a pull
   reaches the bar, not J32 / the OPA564 output or the SMA.

Then follow the **commissioning sequence** (mechanical → B0 → RF → NMR → shim) and **acceptance criteria** in
[`design-closure.md`](design-closure.md). Bench-only steps that remain: tune/match the tank, **recalibrate t90 by
nutation** (the 369 µs value is a calculation), map B0 over the ROI vs the simulation, acquire the first FID, and decide
on shimming from the measured linewidth. Note the open board-side blocker: **TX and RX on one coil** trips the OPA564 on
the first pulse (needs a series TX diode pair + RX series element, or a separate receive coil) before a real scan.
