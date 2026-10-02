"""Turn NMR records from the class board into a 3D-printable data matrix - from the physics of
this probe, not from an ideal line.

After Jones et al., "Using 3D Printing to Visualize 2D Chromatograms and NMR Spectra for the
Classroom", J. Chem. Educ. 2021, 98, 1024-1030 (doi 10.1021/acs.jchemed.0c01130): the records
become a surface, and the paper's print lessons are applied before it reaches OpenSCAD: crop to
the region of interest, average over blocks so peaks are ridges and not needles, normalise to 1;
the axis ranges and the answers go along with the matrix, so the model carries its own labels.

Two experiments (rows of the surface):
  sweep (default)  one spectrum per B0 coil current: the ridge runs diagonally, its slope is the
                   pair's calibration (Hz/A -> mT/A) and its width the field's homogeneity (ppm) -
                   what this probe can show, since its line is about 150 Hz wide;
  decay            sliding-window spectra of one record: the ridge's height is the FID - only
                   readable in a field good enough for T2* >> the window (--field ideal shows it).

Every number comes from nmr-params.scad (firmware defaults, water, copper) and probe.scad (the
B0 pair, the coil, the 50 mL tube), parsed here, so the simulator and the models cannot disagree:
  - B0 over the sample: the Helmholtz field to 4th order, B = B0 [1 - 144/125 (y^4 - 3 y^2 rho^2
    + 3/8 rho^4) / R^4] (y along the pair's axis), sampled over the tube's water where the winding
    sees it (the tube is off-centre in the winding: its body ends before the far cheek);
  - the record starts nmr_t_acq_start after the pulse (dead time) and lasts nmr_t_acq;
  - amplitude ~ f^2 (magnetisation ~ B0, induced voltage ~ omega) x the RX tank's response (Q);
  - voltage drive (--drive voltage, as the board's H-bridge does it): the coils warm at
    P / (m c), their resistance rises cu_alpha per K, the current and B0 fall with it - no cooling,
    so an upper bound - and scans averaged over that time smear the line.

    py spectrum.py                                   # SIM field sweep, this probe
    py spectrum.py --drive current                   # the same with a constant-current B0 drive
    py spectrum.py --experiment decay --field ideal  # the FID in a perfect field (T2 of water)
    py spectrum.py --csv i1.csv i2.csv ... --currents 1.485 1.49 ...   # measured sweep
    py spectrum.py --experiment decay --csv fid.csv  # measured record (instrument nmr --csv)

Writes spectrum-data.scad next to this file; spectrum.scad and cut-spectrum.scad include it.
"""

import argparse
import csv
import math
import re
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
MU0 = 4e-7 * math.pi


def scad_numbers(path):
    """Top-level `name = <number>;` assignments of a .scad file (literals only)."""
    num = r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?"
    out = {}
    for m in re.finditer(rf"^\s*([A-Za-z_]\w*)\s*=\s*({num})\s*;", path.read_text(encoding="utf-8"), re.M):
        out.setdefault(m.group(1), float(m.group(2)))
    return out


class Probe:
    """The setup as nmr-params.scad and probe.scad describe it (same formulas as probe.scad)."""

    def __init__(self, f_tank=None):
        p = scad_numbers(HERE / "nmr-params.scad")
        q = scad_numbers(HERE / "probe.scad")
        self.p, self.q = p, q
        self.f_tx, self.f_lo, self.gamma = p["nmr_f_tx"], p["nmr_f_lo"], p["nmr_gamma"]
        self.B0 = self.f_tx / self.gamma
        self.rate, self.t_acq, self.t0 = p["nmr_rate"], p["nmr_t_acq"], p["nmr_t_acq_start"]
        self.t_repeat, self.T2 = p["nmr_t_repeat"], p["water_T2"]
        self.Q = p["coil_Q"]
        self.f_tank = f_tank or self.f_tx
        # the B0 pair (probe.scad: hh_N = ceil(B0 R / ((4/5)^1.5 mu0 I)))
        self.R, self.I0 = q["hh_R"], q["hh_I"]
        k = (4 / 5) ** 1.5 * MU0
        self.N = math.ceil(self.B0 * self.R / 1000 / (k * self.I0))
        self.b_per_a = k * self.N / (self.R / 1000)                        # T/A
        cu_d = q["hh_cu"] / 1000
        length = 2 * self.N * 2 * math.pi * self.R / 1000                  # m, both coils
        self.ohm = length * 1.724e-8 / (math.pi * (cu_d / 2) ** 2)
        self.cu_kg = length * math.pi * (cu_d / 2) ** 2 * 8960
        self.alpha, self.cu_c = p["cu_alpha"], p["cu_c"]
        # the water the winding sees, along the coil axis x from the winding centre (probe.scad's
        # former frame: tube body from -fl_t to -fl_t - cap_h + smp_l - cone_l, winding cheek_t..+wind_l)
        body = (-q["fl_t"], -q["fl_t"] - q["cap_h"] + q["smp_l"] - q["cone_l"])
        wind = (q["cheek_t"], q["cheek_t"] + q["wind_l"])
        zc = (wind[0] + wind[1]) / 2
        self.x_span = (max(body[0], wind[0]) - zc, min(body[1], wind[1]) - zc)
        self.r_smp = q["smp_d"] / 2

    def field_offsets(self, n=40000, ideal=False, seed=3):
        """Relative field B/B_centre - 1 at n points spread evenly through the water."""
        if ideal:
            return np.zeros(1)
        rng = np.random.default_rng(seed)
        x = rng.uniform(*self.x_span, n)
        r = self.r_smp * np.sqrt(rng.uniform(0, 1, n))
        a = rng.uniform(0, 2 * np.pi, n)
        y, zz = r * np.cos(a), r * np.sin(a)          # y along the pair's axis, z up
        rho2 = (x * x + zz * zz) / self.R ** 2
        y2 = y * y / self.R ** 2
        return -144 / 125 * (y2 * y2 - 3 * y2 * rho2 + 3 / 8 * rho2 * rho2)

    def tank(self, f):
        return 1 / math.sqrt(1 + (2 * self.Q * (f - self.f_tank) / self.f_tank) ** 2)


def sim_record(pr, f_line, offsets, amp, n_avg, rng):
    """One averaged record: the line at f_line (Hz, absolute) spread by the field offsets."""
    n = int(pr.rate * pr.t_acq)
    df = pr.rate / n
    f_if = f_line - pr.f_lo
    H = np.zeros(n, complex)
    k = np.round((f_if + f_line * offsets) / df).astype(int) % n
    np.add.at(H, k, 1.0 / offsets.size)
    fk = np.fft.fftfreq(n, 1 / pr.rate)
    H *= np.exp(2j * np.pi * fk * pr.t0)               # the record starts t0 after the pulse
    t = np.arange(n) / pr.rate
    sig = n * np.fft.ifft(H) * np.exp(-(t + pr.t0) / pr.T2) * amp
    hum = 0.002 * np.exp(2j * np.pi * 50 * t + 1j * rng.uniform(0, 2 * np.pi)) / math.sqrt(n_avg)
    noise = 0.003 / math.sqrt(n_avg) * (rng.standard_normal(n) + 1j * rng.standard_normal(n)) / math.sqrt(2)
    return sig + hum + noise + (0.005 - 0.004j)


def sim_averaged(pr, I_set, offsets, n_avg, drive, dT, rng):
    """n_avg scans at the set current, t_repeat apart, averaged; returns (record, coil warming after).

    Voltage drive: the H-bridge holds the voltage, so the current is I_set / (1 + alpha dT) and the
    line walks down while the coils warm - scans at different frequencies average to a record whose
    FID dies early. Constant current: every scan at the same frequency."""
    zs = []
    for _ in range(n_avg):
        I = I_set if drive == "current" else I_set / (1 + pr.alpha * dT)
        f_line = pr.gamma * pr.b_per_a * I
        amp = 0.018 * (f_line / pr.f_tx) ** 2 * pr.tank(f_line)
        zs.append(sim_record(pr, f_line, offsets, amp, 1, rng))
        dT += I * I * pr.ohm * (1 + pr.alpha * dT) * pr.t_repeat / (pr.cu_kg * pr.cu_c)
    return np.mean(zs, axis=0), dT


def read_csv(path):
    """t_ms, I_V, Q_V rows as written by `instrument nmr --csv`."""
    with open(path, newline="") as f:
        a = np.array(list(csv.reader(f))[1:], dtype=float)
    return 1000.0 / (a[1, 0] - a[0, 0]), a[:, 1] + 1j * a[:, 2]


def spectrum(z, rate, df=5.0):
    """Magnitude spectrum of a record whose signal may last only milliseconds.

    A Hann window over the whole record (instrument.py does that) is ~0 at its start, exactly where
    an inhomogeneous FID lives, so here the record is cut where the envelope reaches the noise and
    tapered with the right half of a Hann window: the signal keeps its weight, the noise is left out.
    """
    z = z - z[-z.size // 10:].mean()
    m = max(1, int(rate / 1000))
    env = np.convolve(np.abs(z), np.ones(m) / m, mode="same")
    noise = np.median(env[z.size // 2:])
    below = np.nonzero(env[m:] < 2 * noise)[0]
    n_use = int(np.clip((below[0] + m) if below.size else z.size, rate * 0.005, z.size))
    w = 0.5 * (1 + np.cos(np.pi * np.arange(n_use) / n_use))
    n_fft = max(int(round(rate / df)), z.size)          # one length for every record: same bins
    s = np.abs(np.fft.fftshift(np.fft.fft(z[:n_use] * w, n_fft))) / w.sum()
    f = np.fft.fftshift(np.fft.fftfreq(n_fft, 1 / rate))
    return f, s, n_use / rate


def fwhm(f, s):
    s = s / s.max()
    i = int(np.argmax(s))
    lo = i
    while lo > 0 and s[lo] >= 0.5:
        lo -= 1
    hi = i
    while hi < s.size - 1 and s[hi] >= 0.5:
        hi += 1
    fl = np.interp(0.5, [s[lo], s[lo + 1]], [f[lo], f[lo + 1]])
    fh = np.interp(0.5, [s[hi], s[hi - 1]], [f[hi], f[hi - 1]])
    return fh - fl


def block_cols(a, bx):
    c = (a.shape[-1] // bx) * bx
    return a[..., :c].reshape(*a.shape[:-1], c // bx, bx).mean(axis=-1)


def widen(spec, dfc, w_true, min_cols=5):
    """The paper's print lesson: a ridge narrower than a few columns prints as a needle, and one that
    moves sideways from row to row prints with a sawtooth top (its sampled height jumps). Convolve the
    rows with a Gaussian so the printed FWHM is min_cols columns; returns (spec, printed FWHM, Hz)."""
    w_min = min_cols * dfc
    if w_true >= w_min:
        return spec, w_true
    sig = math.sqrt(w_min ** 2 - w_true ** 2) / 2.355 / dfc
    k = np.exp(-0.5 * (np.arange(-4 * int(sig + 1), 4 * int(sig + 1) + 1) / sig) ** 2)
    k /= k.sum()
    return np.array([np.convolve(r, k, mode="same") for r in spec]), w_min


def centroid(f, s):
    s = np.clip(s - 0.3 * s.max(), 0, None)
    return float((f * s).sum() / s.sum())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--experiment", choices=["sweep", "decay"], default="sweep")
    ap.add_argument("--csv", nargs="+", help="measured records (sweep: one per current, with --currents)")
    ap.add_argument("--currents", type=float, nargs="+", help="B0 coil current of each --csv record, A")
    ap.add_argument("--field", choices=["probe", "ideal"], default="probe", help="SIM: this probe's B0 pair or a perfect field")
    ap.add_argument("--drive", choices=["voltage", "current"], default="voltage", help="SIM: how the H-bridge drives B0")
    ap.add_argument("--sweep", type=float, default=1.0, help="SIM sweep: +- this percent of the B0 current")
    ap.add_argument("--steps", type=int, default=29, help="SIM sweep: currents (about one column of ridge shift per row)")
    ap.add_argument("--n-avg", type=int, default=4, help="SIM: scans averaged per record")
    ap.add_argument("--f-tank", type=float, help="RX tank resonance, Hz (instrument.scad: ~88470 with the coax)")
    ap.add_argument("--window", type=float, default=0.25, help="decay: spectrum window, s")
    ap.add_argument("--hop", type=float, default=0.05, help="decay: step between windows, s")
    ap.add_argument("--t-max", type=float, default=1.5, help="decay: last window start, s")
    ap.add_argument("--cols", type=int, default=60, help="frequency columns after the block average")
    ap.add_argument("--out", default=str(HERE / "spectrum-data.scad"))
    a = ap.parse_args()

    pr = Probe(a.f_tank)
    rng = np.random.default_rng(1)
    offsets = pr.field_offsets(ideal=a.field == "ideal")
    spread = pr.f_tx * (offsets.max() - offsets.min())
    measured = bool(a.csv)
    src = ", ".join(Path(c).name for c in a.csv) if measured else \
        f"SIM: {a.field} field, {a.drive} drive, {a.n_avg} scans/record (spectrum.py)"
    answers, gauge = [], []

    if a.experiment == "sweep":
        if measured:
            if not a.currents or len(a.currents) != len(a.csv):
                raise SystemExit("a measured sweep needs one --currents value per --csv record")
            cur = np.array(a.currents)
            recs = [read_csv(c) for c in a.csv]
            heat = None
        else:
            cur = pr.I0 * (1 + np.linspace(-a.sweep, a.sweep, a.steps) / 100)
            recs, heat, dT = [], [], 0.0
            for I_set in cur:
                z, dT = sim_averaged(pr, I_set, offsets, a.n_avg, a.drive, dT, rng)
                recs.append((pr.rate, z))
                heat.append(dT)
        specs, used = [], []
        for rate, z in recs:
            f, s, t_use = spectrum(z, rate)
            specs.append(s)
            used.append(t_use)
        f_abs = pr.f_lo + f
        f_lines = np.array([centroid(f_abs, s) for s in specs])
        widths = [fwhm(f_abs, s) for s in specs]
        w_mid = float(np.median(widths))
        margin = max(3 * w_mid, 200)
        keep = (f_abs > f_lines.min() - margin) & (f_abs < f_lines.max() + margin)
        bx = max(1, int(keep.sum() // a.cols))
        spec = block_cols(np.array(specs)[:, keep], bx)
        f_rel = block_cols(f_abs[keep], bx) - pr.f_tx
        slope = np.polyfit(cur, f_lines, 1)[0]
        # the field's own lineshape: the offsets as a 1 Hz histogram (no drift, no window)
        hf, he = np.histogram(offsets * pr.f_tx, bins=np.arange(offsets.min() * pr.f_tx - 5, offsets.max() * pr.f_tx + 6, 1.0))
        w_field = fwhm((he[:-1] + he[1:]) / 2, hf.astype(float)) if offsets.size > 1 else 0.0
        spec, w_print = widen(spec, f_rel[1] - f_rel[0], w_mid)
        rows, row_name, row_unit = cur, "B0 COIL CURRENT (A)", "A"
        row_tick = 0.01 if np.ptp(cur) < 0.1 else 0.05
        ppm = w_mid / pr.f_tx * 1e6
        answers = [
            f"slope df/dI = {slope / 1000:.2f} kHz/A (straight line through the ridge)",
            f"B0 pair: slope / gamma = {slope / pr.gamma * 1e3:.3f} mT/A (design {pr.b_per_a * 1e3:.3f})",
            f"FWHM {w_mid:.0f} Hz = {ppm:.0f} ppm of B0 over the sample (a cusp with a tail, not a Lorentzian)"
            if measured or a.drive == "current" else
            f"FWHM {w_mid:.0f} Hz, mostly drift within each record; the field alone: {w_field:.0f} Hz = "
            f"{w_field / pr.f_tx * 1e6:.0f} ppm",
        ]
        if w_print > w_mid:
            answers.append(f"ridge widened to {w_print:.0f} Hz for printing; the true FWHM is {w_mid:.0f} Hz")
        if heat:
            drift = pr.alpha * heat[-1] * pr.f_tx
            answers.append(f"coils +{heat[-1]:.1f} K over the sweep: line {drift:.0f} Hz low at the end ({a.drive} drive)"
                           if a.drive == "voltage" else "constant-current drive: no thermal drift")
        print(f"sweep {cur[0]:.4f}..{cur[-1]:.4f} A: lines {f_lines[0]:.0f}..{f_lines[-1]:.0f} Hz, slope {slope:.0f} Hz/A "
              f"(design {pr.gamma * pr.b_per_a:.0f}), FWHM {w_mid:.0f} Hz ({ppm:.0f} ppm; field spread over the "
              f"sample {spread:.0f} Hz), signal used {min(used) * 1e3:.1f}..{max(used) * 1e3:.1f} ms of each record")
    else:
        if measured:
            rate, z = read_csv(a.csv[0])
        else:
            rate = pr.rate
            z, dT = sim_averaged(pr, pr.I0, offsets, a.n_avg, a.drive, 0.0, rng)
        z = z - z[-z.size // 10:].mean()
        n_win, n_fft = int(a.window * rate), int(rate)
        w = np.hanning(n_win)
        starts = np.arange(0, int(a.t_max * rate) + 1, int(a.hop * rate))
        starts = starts[starts + n_win <= z.size]
        f = np.fft.fftshift(np.fft.fftfreq(n_fft, 1 / rate))
        spec_all = np.array([np.abs(np.fft.fftshift(np.fft.fft(z[s:s + n_win] * w, n_fft))) for s in starts])
        f_abs = pr.f_lo + f
        f_c = centroid(f_abs, spec_all[0])
        w0 = fwhm(f_abs, spec_all[0])
        margin = max(3 * w0, 50)
        keep = np.abs(f_abs - f_c) < margin
        bx = max(1, int(keep.sum() // a.cols))
        spec = block_cols(spec_all[:, keep], bx)
        f_rel = block_cols(f_abs[keep], bx) - pr.f_tx
        rows = pr.t0 + (starts + n_win / 2) / rate
        ridge = spec.max(axis=1) / spec.max()
        ok = ridge > 0.05
        t2 = -1 / np.polyfit(rows[ok], np.log(ridge[ok]), 1)[0] if ok.sum() > 2 else float("nan")
        row_name, row_unit, row_tick = "TIME AFTER PULSE (s)", "s", 0.5
        gauge = [[1, "1"], [math.exp(-1), "1/e"], [math.exp(-2), "1/e²"]]
        answers = [f"T2* = {t2:.2f} s (fit of the ridge height, exp(-t/T2*))",
                   f"FWHM = {w0:.1f} Hz: set by the {a.window} s window,",
                   f"the line itself is 1/(pi T2*) = {1 / (math.pi * t2):.2f} Hz"]
        if ok.sum() <= 2:
            print(f"WARNING: the signal is gone within the first window - this field spreads the line over {spread:.0f} Hz "
                  f"(T2* ~ {1e3 / (math.pi * max(spread, 1e-9)):.1f} ms); use --experiment sweep, or --field ideal to see the FID")
        print(f"decay: {len(rows)} windows, T2* {t2:.3f} s, FWHM {w0:.1f} Hz")

    zz = np.clip(spec / spec.max(), 0, 1)
    answers.append(f"1H {pr.f_tx:.0f} Hz = {pr.B0 * 1e3:.3f} mT; gamma = {pr.gamma / 1e6:.3f} MHz/T")
    answers.append(src)

    def row(v):
        return "[" + ",".join(f"{x:.4f}" for x in v) + "]"

    def s(t):
        return '"' + t.replace('"', "'") + '"'

    with open(a.out, "w", newline="\n", encoding="utf-8") as f:
        f.write("// Generated by spectrum.py - do not edit; re-run it with new records instead.\n")
        f.write(f"spec_mode = {s(a.experiment)};\n")
        f.write(f"spec_source = {s(src)};\n")
        f.write(f"spec_larmor_hz = {pr.f_tx:.1f};   // frequency axis origin: the firmware's f_tx (nmr-params.scad)\n")
        f.write(f"spec_f = [{f_rel[0]:.2f}, {f_rel[-1]:.2f}];   // columns, Hz from spec_larmor_hz\n")
        f.write(f"spec_row = [{rows[0]:.5f}, {rows[-1]:.5f}];   // rows: {row_name}\n")
        f.write(f"spec_row_name = {s(row_name)};\nspec_row_unit = {s(row_unit)};\nspec_row_tick = {row_tick};\n")
        f.write("spec_gauge = [" + ", ".join(f"[{g[0]:.5f}, {s(g[1])}]" for g in gauge) + "];   // gauge heights, x peak\n")
        f.write("spec_answers = [\n" + ",\n".join("    " + s(t) for t in answers) + "\n];\n")
        f.write("// rows x frequency columns, normalised 0..1\n")
        f.write("spec_z = [\n" + ",\n".join(row(r) for r in zz) + "\n];\n")
    print(f"{zz.shape[0]} x {zz.shape[1]} matrix, f {f_rel[0]:+.0f}..{f_rel[-1]:+.0f} Hz -> {a.out}")
    for t in answers:
        print("  " + t)


if __name__ == "__main__":
    main()
