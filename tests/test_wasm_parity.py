"""The browser (WASM) and native builds of the core agree: same program bytes, ADC codes, processed data, peak."""

import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
WASM = ROOT / "simulator/web/nmrcore.wasm"


@pytest.mark.skipif(not WASM.exists() or not shutil.which("node"), reason="needs simulator/web/nmrcore.wasm and node")
def test_wasm_matches_native():
    from nmrpy import core
    w = json.loads(subprocess.run(["node", str(ROOT / "tests/wasm_parity.mjs")], capture_output=True, text=True, check=True).stdout)
    assert w["sha"] == core.config_sha256(), "both builds from the same generated config"
    assert w["nvox"] == len(core.voxelize(0.005))
    s = core.default_sequence(0)
    prog = core.compile_sequence(0, n_avg=4, acq_len=0.2)
    assert bytes(w["prog"]) == prog, "identical compiled program bytes"
    recs = core.simulate(prog)
    assert w["nrec"] == len(recs)
    np.testing.assert_allclose(w["t_first"], [r["t_first"] for r in recs], rtol=0, atol=1e-12)
    di = np.abs(np.array(w["i0"]) - recs[0]["i"][:2000]).max()
    dq = np.abs(np.array(w["q3"]) - recs[3]["q"][:2000]).max()
    print(f"max ADC code difference native vs wasm: I {di}, Q {dq} LSB")
    assert di <= 1 and dq <= 1, "floating-point differences may move a rounding by at most 1 LSB"
    z, fs, t0 = core.process()
    pw = np.array(w["proc"])
    pn = np.empty(200); pn[0::2] = z[:100].real; pn[1::2] = z[:100].imag
    np.testing.assert_allclose(pw, pn, rtol=0, atol=2e-4 * np.abs(pn).max())
    f, sp = core.spectrum(z, fs, 0, 0, 4)
    pk = core.peak(f, sp, -400, 400, 10)
    assert w["peak"]["freq_hz"] == pytest.approx(pk["freq_hz"], abs=1e-3)
    assert w["peak"]["amplitude"] == pytest.approx(pk["amplitude"], rel=1e-4)
