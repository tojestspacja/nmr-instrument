"""exact-<sample>-fieldmap.json + summary (Mathematica) -> simulator/m/fieldmap-<sample>.json for physics.js."""
import json, sys
from pathlib import Path
sample, out = sys.argv[1], Path(sys.argv[2])
vox = json.loads(Path(f"exact-{sample}-fieldmap.json").read_text())
summ = json.loads(Path(f"exact-{sample}-summary.json").read_text())
iTx = 7.95 / (6.7 ** 2 + (2 * 3.141592653589793 * 89400 * 2.53e-3) ** 2) ** 0.5
m = {
    "source": "nmr_exact.wls (Mathematica 13.3, Biot-Savart; pulses checked with SpinDynamica 3.7.1)",
    "sample": sample, "grid_mm": summ["grid_mm"], "B0_centre_mT_at_1.5A": summ["B0_centre_mT"],
    "centre": {"b1": summ["B1_centre_uT"] * 1e-6 * 2 / iTx},      # T/A, perpendicular, at the coil centre
    "p": [v["p"] for v in vox], "df": [v["df"] for v in vox],
    "b1": [float(f"{v['b1']:.6g}") for v in vox], "phi": [v["phi"] for v in vox],
    "spindynamica": {k: summ[k] for k in ["t90_centre_us", "flip_centre_deg", "emf_uV", "tank_uV", "line_Hz", "fwhm_Hz", "T2star_ms", "spread_Hz", "water_mL"]},
}
out.write_text(json.dumps(m, separators=(",", ":")))
print(out, out.stat().st_size, "bytes,", len(vox), "voxels")
