"""Milestone 1: the canonical instrument model and its generated representations."""

import copy
import json
import re
import subprocess
import sys
from pathlib import Path

import jsonschema
import pytest
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_config  # noqa: E402


def load():
    return yaml.load((ROOT / "design/instrument.yaml").read_text(encoding="utf-8"), Loader=gen_config.Loader)


def schema():
    return json.loads((ROOT / "design/schema/instrument.schema.json").read_text(encoding="utf-8"))


def test_generated_is_up_to_date():
    r = subprocess.run([sys.executable, str(ROOT / "tools/gen_config.py"), "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_schema_accepts_the_canonical_file():
    jsonschema.validate(load(), schema())


@pytest.mark.parametrize("mutate, why", [
    (lambda d: d["tx"]["frequency"].update(unit="kHz"), "unit outside the SI set"),
    (lambda d: d["tx"]["frequency"].pop("source"), "quantity without a source"),
    (lambda d: d["tx"]["frequency"].update(kind="guess"), "unknown kind"),
    (lambda d: d["tx"]["frequency"].update(value="89.4k"), "non-numeric value"),
    (lambda d: d.update(extra={}), "unknown top-level group"),
])
def test_schema_rejects(mutate, why):
    d = copy.deepcopy(load())
    mutate(d)
    with pytest.raises(jsonschema.ValidationError):
        jsonschema.validate(d, schema())


def test_scientific_notation_without_point_is_a_number():
    assert yaml.load("x: 417e-6", Loader=gen_config.Loader)["x"] == pytest.approx(417e-6)


def test_derived_values_follow_their_formulas():
    q, _ = gen_config.flatten(load())
    d = gen_config.derive(q)
    v = {p: x["value"] for p, x in {**q, **d}.items()}
    assert v["rf.if_frequency"] == v["tx.frequency"] - v["lo.frequency"]
    # Helmholtz thin-wire value checked independently (legacy audit: 2.10407 mT for N=312, I=1.5 A, R=0.2 m)
    assert v["b0.field_thin_wire"] == pytest.approx(2.10407e-3, rel=2e-5)
    # B0 pair resistance and power (legacy probe.scad echo: 10.3 ohm, 23 W)
    assert v["b0.resistance_20c"] == pytest.approx(10.33, rel=2e-3)
    # voltage-drive drift reproduces the legacy 138 Hz/min within 1 %
    assert v["b0.voltage_drive_drift_rate"] * 60 == pytest.approx(-138.6, rel=1e-2)
    # Curie magnetization (audit: 6.9094e-6 A/m at 2.0997 mT, 293 K, N=6.69e28) scaled to this B0 and N
    expect = 6.9094e-6 * (v["b0.field_thin_wire"] / 2.0997e-3) * (v["sample.proton_density"] / 6.69e28) \
        * (v["nucleus.gamma_bar"] / 42.577e6) ** 2
    assert v["sample.curie_magnetization"] == pytest.approx(expect, rel=2e-4)


def test_consistency_checks_catch_violations():
    q, _ = gen_config.flatten(load())
    bad = copy.deepcopy(q)
    bad["acquisition.decimation"]["value"] = 64          # 1.56 kS/s < 2 x 5.4 kHz
    assert any("decimated rate" in e for e in gen_config.check(bad, gen_config.derive(bad)))
    bad = copy.deepcopy(q)
    bad["tx.t90"]["value"] = 417e-6                       # without the diode drop: >2 % off
    assert any("t90" in e for e in gen_config.check(bad, gen_config.derive(bad)))
    bad = copy.deepcopy(q)
    bad["acquisition.averages"]["value"] = 6
    assert any("CYCLOPS" in e for e in gen_config.check(bad, gen_config.derive(bad)))


def test_identifiers_carry_units():
    cfg = json.loads((ROOT / "generated/instrument_config.json").read_text(encoding="utf-8"))
    for name, x in cfg["params"].items():
        suf = gen_config.UNIT_SUFFIX[x["unit"]]
        assert not suf or name.endswith("_" + suf), name


# No hand-typed copies of critical numbers in the new implementation (legacy/, design/, generated/ and docs/ excepted).
CRITICAL = [r"\b89400(\.0)?\b", r"\b84000(\.0)?\b", r"\b5400(\.0)?\b", r"\b417e-6\b", r"\b458e-6\b", r"\b2\.53e-3\b",
            r"\b42\.57\d*e6\b", r"\b25000(\.0)?\b", r"\b100000(\.0)?\b"]
SCAN = ["physics", "pulse", "firmware", "simulator", "tools", "electronics", "mechanical", "validation"]
SKIP_PARTS = {"generated", "legacy", "build", "node_modules", ".pio", "__pycache__", "golden", "data", "legacy-audit"}


def test_no_duplicated_critical_constants():
    hits = []
    for top in SCAN:
        base = ROOT / top
        if not base.exists():
            continue
        for f in base.rglob("*"):
            if not f.is_file() or SKIP_PARTS & set(f.relative_to(ROOT).parts):
                continue
            if f.suffix not in {".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts", ".html", ".scad", ".yaml", ".json", ".ini"}:
                continue
            if f.name == "gen_config.py":
                continue
            text = f.read_text(encoding="utf-8", errors="ignore")
            for pat in CRITICAL:
                for m in re.finditer(pat, text):
                    line = text.count("\n", 0, m.start()) + 1
                    hits.append(f"{f.relative_to(ROOT)}:{line}: {m.group(0)}")
    assert not hits, "critical constants typed by hand (use generated/):\n" + "\n".join(hits)
