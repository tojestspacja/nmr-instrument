"""Generate language-specific instrument configs from design/instrument.yaml (the only hand-edited copy).

    py tools/gen_config.py            # write generated/
    py tools/gen_config.py --check    # exit 1 if generated/ is stale (CI)

What it does:
  1. validates design/instrument.yaml against design/schema/instrument.schema.json;
  2. flattens it to unit-carrying identifiers (tx.frequency [Hz] -> TX_FREQUENCY_HZ);
  3. computes the derived quantities (each with its formula) so nothing derived is ever typed twice;
  4. checks physical consistency (Nyquist, ADC rate, t90 vs. B1, ...) and fails on violations;
  5. writes generated/instrument_config.{json,hpp,py} with a SHA-256 of the canonical content.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "design" / "instrument.yaml"
SCHEMA = ROOT / "design" / "schema" / "instrument.schema.json"
OUT = ROOT / "generated"

UNIT_SUFFIX = {
    "1": "", "m": "M", "s": "S", "Hz": "HZ", "A": "A", "V": "V", "ohm": "OHM", "H": "H", "F": "F", "T": "T", "K": "K",
    "J*s": "J_S", "J/K": "J_PER_K", "H/m": "H_PER_M", "ohm*m": "OHM_M", "1/K": "PER_K", "J/(kg*K)": "J_PER_KG_K",
    "kg/m^3": "KG_PER_M3", "Hz/T": "HZ_PER_T", "1/m^3": "PER_M3", "T/A": "T_PER_A", "V/sqrt(Hz)": "V_PER_RTHZ",
    "W": "W", "kg": "KG", "A/m": "A_PER_M", "rad": "RAD", "Hz/A": "HZ_PER_A", "K/s": "K_PER_S", "Hz/s": "HZ_PER_S",
}


def ident(path: str, unit: str) -> str:
    base = path.replace(".", "_").upper()
    suf = UNIT_SUFFIX[unit]
    return f"{base}_{suf}" if suf else base


def flatten(doc: dict) -> tuple[dict, dict]:
    """-> (quantities {path: {value, unit, kind, source}}, strings {path: str})"""
    q, s = {}, {}
    for group, body in doc.items():
        if group in ("schema_version",):
            continue
        for key, v in body.items():
            path = f"{group}.{key}"
            if isinstance(v, dict):
                q[path] = v
            else:
                s[path] = v
    return q, s


def derive(q: dict) -> dict:
    """Derived quantities. Each: (value, unit, formula). Inputs by path."""
    v = {p: x["value"] for p, x in q.items()}
    d = {}

    def put(path, value, unit, formula):
        d[path] = {"value": value, "unit": unit, "kind": "calculated", "source": formula}

    gbar = v["nucleus.gamma_bar"]
    pi = math.pi
    mu0 = v["constants.mu0"]
    put("rf.if_frequency", v["tx.frequency"] - v["lo.frequency"], "Hz", "tx.frequency - lo.frequency")
    put("lo.clk1_frequency", v["lo.frequency"] * v["lo.multiplier"], "Hz", "lo.frequency * lo.multiplier")
    put("acquisition.decimated_rate", v["acquisition.raw_rate"] / v["acquisition.decimation"], "Hz",
        "acquisition.raw_rate / acquisition.decimation")
    put("acquisition.samples", round(v["acquisition.duration"] * v["acquisition.raw_rate"] / v["acquisition.decimation"]),
        "1", "round(duration * raw_rate / decimation) complex samples")
    # B0 pair (thin-wire Helmholtz; the finite-section value comes from the physics core / field map)
    b0_thin = (4 / 5) ** 1.5 * mu0 * v["b0.turns_per_coil"] * v["b0.current"] / v["b0.radius"]
    put("b0.field_thin_wire", b0_thin, "T", "(4/5)^1.5 mu0 N I / R")
    put("b0.larmor_thin_wire", gbar * b0_thin, "Hz", "gamma_bar * b0.field_thin_wire")
    put("b0.field_per_amp_thin_wire", b0_thin / v["b0.current"], "T/A", "(4/5)^1.5 mu0 N / R")
    wire_len = 2 * v["b0.turns_per_coil"] * 2 * pi * v["b0.radius"]
    area = pi * (v["b0.wire_copper_diameter"] / 2) ** 2
    r_b0 = v["constants.copper_resistivity_20c"] * wire_len / area
    mass = wire_len * area * v["constants.copper_density"]
    put("b0.resistance_20c", r_b0, "ohm", "rho_Cu * 2 N 2 pi R / (pi d^2/4)")
    put("b0.copper_mass", mass, "kg", "2 N 2 pi R * pi d^2/4 * rho_mass")
    put("b0.voltage_20c", r_b0 * v["b0.current"], "V", "I * R_20C")
    put("b0.power_20c", r_b0 * v["b0.current"] ** 2, "W", "I^2 R_20C")
    heat = r_b0 * v["b0.current"] ** 2 / (mass * v["constants.copper_heat_capacity"])
    put("b0.heating_rate_adiabatic", heat, "K/s", "P / (m c), no cooling: an upper bound")
    put("b0.voltage_drive_drift_rate", -gbar * b0_thin * v["constants.copper_tempco"] * heat, "Hz/s",
        "voltage drive: df/dt = -f alpha_Cu dT/dt")
    put("b0.current_drive_drift_rate", -gbar * b0_thin * v["constants.copper_expansion"] * heat, "Hz/s",
        "constant current: B ~ 1/R_geometric -> df/dt = -f alpha_expansion dT/dt")
    # RF coil and tank
    w = 2 * pi * v["tx.frequency"]
    put("rf_coil.reactance", w * v["rf_coil.inductance"], "ohm", "2 pi f_tx L")
    put("rf_coil.tank_capacitance", 1 / (w ** 2 * v["rf_coil.inductance"]), "F", "1 / ((2 pi f_tx)^2 L)")
    put("rf_coil.tank_parallel_resistance", v["rf_coil.quality_factor"] * w * v["rf_coil.inductance"], "ohm", "Q omega L")
    put("rf_coil.ringdown_time_constant", v["rf_coil.quality_factor"] / (pi * v["tx.frequency"]), "s",
        "tau = 2Q/omega = Q/(pi f) (energy decays at 2/tau)")
    i_tx = (v["tx.amplitude"] - v["tx.series_diode_drop"]) / math.hypot(v["rf_coil.resistance"], w * v["rf_coil.inductance"])
    put("tx.coil_current", i_tx, "A", "(V_tx - V_diode) / |R + j omega L| (untuned coil during TX, ADR-0007)")
    b1rot = 0.5 * v["rf_coil.b1_per_amp_centre"] * i_tx
    put("tx.b1_rotating_centre", b1rot, "T", "1/2 * b1_per_amp_centre * coil_current")
    put("tx.t90_calculated", 1 / (4 * gbar * b1rot), "s", "pi/2 = 2 pi gamma_bar B1rot t  ->  t = 1/(4 gamma_bar B1rot)")
    put("tx.excitation_bandwidth", 1 / (4 * v["tx.t90"]), "Hz", "~1/(4 t90): +-half-width of a hard 90 pulse's flat region")
    # Curie magnetization at the thin-wire field
    m0 = (v["sample.proton_density"] * (2 * pi * gbar) ** 2 * v["constants.hbar"] ** 2 * b0_thin
          / (4 * v["constants.k_boltzmann"] * v["sample.temperature"]))
    put("sample.curie_magnetization", m0, "A/m", "N gamma^2 hbar^2 B0 / (4 k T)")
    put("timing.tick_period", 1 / v["timing.tick"], "s", "1 / timing.tick")
    put("timing.capture_resolution", 1 / v["timing.capture_clock"], "s", "1 / timing.capture_clock")
    return d


def check(q: dict, d: dict) -> list[str]:
    """Physical/engineering consistency. Returns a list of violations (errors)."""
    v = {p: x["value"] for p, x in {**q, **d}.items()}
    errs = []
    if v["acquisition.decimated_rate"] <= 2 * abs(v["rf.if_frequency"]):
        errs.append("decimated rate must exceed 2|IF| (complex IF would still fold for real sampling)")
    if 2 * v["acquisition.raw_rate"] > v["receiver.adc_max_aggregate_rate"]:
        errs.append("two channels at raw_rate exceed the ADC's aggregate maximum")
    if abs(v["tx.t90_calculated"] / v["tx.t90"] - 1) > 0.02:
        errs.append(f"t90 target {v['tx.t90']:.6g} s disagrees with the B1 calculation {v['tx.t90_calculated']:.6g} s by >2 %")
    if abs(v["tx.t180"] / v["tx.t90"] - 2) > 1e-9:
        errs.append("t180 must be 2 t90")
    if v["acquisition.start_delay"] < v["receiver.dead_time"]:
        errs.append("acquisition must not start before the receiver dead time has elapsed")
    if v["receiver.dead_time"] < 10 * v["rf_coil.ringdown_time_constant"]:
        errs.append("dead time shorter than 10 ring-down time constants")
    if v["b0.current"] > v["limits.b0_current_max"]:
        errs.append("B0 current above its limit")
    if v["b0.voltage_20c"] * (1 + v["constants.copper_tempco"] * 20) > v["b0.supply_compliance"] - 1.0:
        errs.append("B0 compliance: hot coil (+20 K) plus 1 V regulator headroom exceeds the supply")
    if v["tx.t90"] > v["limits.tx_max_pulse"] or v["tx.t180"] > v["limits.tx_max_pulse"]:
        errs.append("pulse longer than the TX limit")
    if v["limits.vext_max"] > v["limits.opa564_supply_abs_max"]:
        errs.append("VEXT max above OPA564 absolute maximum")
    if v["tx.frequency"] > v["dds.max_output_frequency"]:
        errs.append("TX frequency above the DDS limit")
    if int(v["acquisition.averages"]) % 4:
        errs.append("averages must be a multiple of 4 (CYCLOPS)")
    return errs


class Loader(yaml.SafeLoader):
    """PyYAML follows YAML 1.1, which reads 417e-6 (no decimal point) as a string; accept it as a float."""


Loader.add_implicit_resolver(
    "tag:yaml.org,2002:float",
    __import__("re").compile(r"^[-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)$"),
    list("-+0123456789."))


def canonical(obj) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def build() -> dict[str, str]:
    doc = yaml.load(SRC.read_text(encoding="utf-8"), Loader=Loader)
    jsonschema.validate(doc, json.loads(SCHEMA.read_text(encoding="utf-8")))
    q, s = flatten(doc)
    d = derive(q)
    errs = check(q, d)
    if errs:
        raise SystemExit("instrument.yaml is inconsistent:\n  " + "\n  ".join(errs))
    params = {ident(p, x["unit"]): {"path": p, **x} for p, x in {**q, **d}.items()}
    if len(params) != len(q) + len(d):
        raise SystemExit("identifier collision between parameters")
    strings = {p.replace(".", "_").upper(): x for p, x in s.items()}
    strings.update({"INSTRUMENT_" + k.upper(): x for k, x in doc["instrument"].items()})
    body = {"params": params, "strings": strings}
    digest = hashlib.sha256(canonical(body).encode()).hexdigest()
    meta = {"generator": "tools/gen_config.py", "source": "design/instrument.yaml", "config_sha256": digest}

    out = {}
    out["instrument_config.json"] = json.dumps({"meta": meta, **body}, indent=1, sort_keys=True, ensure_ascii=False) + "\n"

    def cnum(x):
        if isinstance(x, int) or float(x).is_integer() and abs(x) < 2 ** 53 and not isinstance(x, float):
            return f"{x}"
        return repr(float(x))

    h = ["// GENERATED by tools/gen_config.py from design/instrument.yaml - do not edit.",
         f"// config sha256 {digest}", "#pragma once", "", "namespace nmr::cfg {", "",
         f'inline constexpr const char* CONFIG_SHA256 = "{digest}";']
    for k, x in sorted(strings.items()):
        h.append(f'inline constexpr const char* {k} = "{x}";')
    h.append("")
    for k, x in sorted(params.items()):
        h.append(f"/// {x['path']} [{x['unit']}] ({x['kind']}) - {x['source'].replace('*/', '* /')}")
        h.append(f"inline constexpr double {k} = {cnum(x['value'])};")
    h += ["", "}  // namespace nmr::cfg", ""]
    out["instrument_config.hpp"] = "\n".join(h)

    p = ['"""GENERATED by tools/gen_config.py from design/instrument.yaml - do not edit."""', "",
         f'CONFIG_SHA256 = "{digest}"', ""]
    for k, x in sorted(strings.items()):
        p.append(f"{k} = {x!r}")
    p.append("")
    for k, x in sorted(params.items()):
        p.append(f"{k} = {cnum(x['value'])}  # {x['path']} [{x['unit']}] ({x['kind']})")
    p += ["", "META = " + repr({k: {kk: vv for kk, vv in x.items() if kk != 'value'} for k, x in sorted(params.items())}), ""]
    out["instrument_config.py"] = "\n".join(p)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if generated/ differs from a fresh generation")
    a = ap.parse_args()
    files = build()
    if a.check:
        stale = [n for n, c in files.items() if not (OUT / n).exists() or (OUT / n).read_text(encoding="utf-8") != c]
        if stale:
            print("generated/ is stale: " + ", ".join(stale) + "  (run py tools/gen_config.py)")
            return 1
        print("generated/ is up to date")
        return 0
    OUT.mkdir(exist_ok=True)
    for n, c in files.items():
        (OUT / n).write_text(c, encoding="utf-8", newline="\n")
    print(f"wrote {len(files)} files to generated/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
