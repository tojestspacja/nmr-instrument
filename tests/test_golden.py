"""Golden comparison: the C++ field code (via the C ABI) against every voxel of the legacy Mathematica field maps."""

import json
import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
GOLD = ROOT / "validation/physics/golden"

nmrpy = pytest.importorskip("nmrpy", reason="build the core first")
core = nmrpy.core
LEG = json.loads((GOLD / "legacy-constants.json").read_text())


def transverse(b0, b1):
    """the legacy frame: x' = coil axis (x) projected perpendicular to B0, y' = B0_hat x x'"""
    n = b0 / np.linalg.norm(b0)
    ex = np.array([1.0, 0, 0]) - n[0] * n
    ex /= np.linalg.norm(ex)
    ey = np.cross(n, ex)
    bp = b1 - np.dot(b1, n) * n
    return np.linalg.norm(bp), np.arctan2(np.dot(bp, ey), np.dot(bp, ex))


@pytest.mark.parametrize("sample", ["tube", "bottle"])
def test_fieldmap_every_voxel(sample):
    d = json.loads((GOLD / f"fieldmap-{sample}.json").read_text())
    p = np.array(d["p"]) / 1000.0
    df_err = b1_err = phi_err = 0.0
    for k in range(len(p)):
        b0, b1 = core.fields_at(*p[k])
        df = LEG["gamma_bar_hz_per_t"] * np.linalg.norm(b0) * LEG["b0_current_a"] - LEG["f_tx_hz"]
        b1p, phi = transverse(b0, b1)
        df_err = max(df_err, abs(df - d["df"][k]))
        b1_err = max(b1_err, abs(b1p / d["b1"][k] - 1))
        phi_err = max(phi_err, abs(phi - d["phi"][k]))
    # tolerances: the golden data's own rounding (df 1 mHz, b1 6 s.f., phi 1e-4 rad) plus 1 unit
    print(f"{sample}: {len(p)} voxels; max |d df| {df_err:.2e} Hz, max rel b1 {b1_err:.2e}, max |d phi| {phi_err:.2e}")
    assert df_err <= 2e-3
    assert b1_err <= 1e-5
    assert phi_err <= 1.5e-4
    b0c, b1c = core.fields_at(0, 0, 0)
    assert np.linalg.norm(b0c) * 1.5 * 1e3 == pytest.approx(d["B0_centre_mT_at_1.5A"], rel=1e-9)
    assert b1c[0] == pytest.approx(d["centre"]["b1"], rel=1e-9)


def test_voxelization_volume_and_centring():
    v = core.voxelize(0.0025)
    vol = v[:, 3].sum()
    # analytic: cylinder r 14 mm over 83 mm + cone 20 mm to r 2 mm = 55.88 mL (legacy grid was off-centre: 55.55)
    assert vol * 1e6 == pytest.approx(55.88, rel=0.01)
    assert abs(v[:, 1].mean()) < 1e-12 and abs(v[:, 2].mean()) < 1e-12, "grid centred on the sample axis"
