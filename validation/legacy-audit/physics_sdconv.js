// Pulse-and-FID physics for the class-board NMR setup. Pure functions, no DOM: the page and a
// Node test both load this file.
//
// Model: the water is split into voxels (isochromats). Each sits in the B0 pair's real field
// (Helmholtz, 4th-order error term) plus the Earth's field along B0, and in the class coil's real
// B1 (finite solenoid on-axis profile, rotating component = half the linear field). The pulse is
// an exact rotation about the effective field in the frame rotating at f_tx (off-resonance
// included); after it each voxel precesses at its own offset and decays with the intrinsic T2.
// The coil picks the signal up by reciprocity (same profile), the tuned tank multiplies it by Q
// (and filters it), the receiver mixes it to the IF and adds the tank + amplifier noise.
(function (root) {
  const GAMMA_BAR = 42.577e6;                 // Hz/T
  const GAMMA = 2 * Math.PI * GAMMA_BAR;      // rad/(s T)
  const MU0 = 4e-7 * Math.PI;
  const HBAR = 1.054571817e-34, KB = 1.380649e-23;
  const N_H = 6.69e28;                        // protons per m^3 in water
  const P = {                                 // the design (nmr-params.scad, workbook, design-decisions)
    turns: 400, coilLen: 0.100, coilRad: 0.02045, coilL: 2.53e-3, coilR: 6.7, Q: 10,
    txV: 7.95,                                // V pk, OPA564 into the coil
    hhR: 0.200, hhI0: 1.5, B0nom: 89400 / GAMMA_BAR,      // pair: 2.0997 mT at 1.5 A
    fTank: 88469,                             // Hz, C710 only + 0.78 m RG174 (instrument.scad)
    fIF: 5400, rate: 25000, tDead: 1.2e-3, tAcq: 0.2,     // nmr_t_acq_start; 200 ms of the 2 s record
    T1: 2.5, T2: 2.0, TR: 3.0, temp: 293,
    en: 15.6e-9, bw: 15000,                   // V/rtHz at the tank (tank + OPA1656), receiver bandwidth
    gain: 1.02 / 40e-6,                       // tank volts -> ADC volts (D-36: 40 uV -> 1.02 V)
  };

  function rng(seed) { let s = seed >>> 0; return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296); }
  function gauss(r) { let u = 0, v = 0; while (u === 0) u = r(); v = r(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }

  // finite solenoid, on axis, as a fraction of mu0 N I / L
  function coilProfile(x) {
    const h = P.coilLen / 2, a = P.coilRad;
    return 0.5 * ((x + h) / Math.hypot(x + h, a) - (x - h) / Math.hypot(x - h, a));
  }

  // sample geometry (m), coil-centred frame: x along the coil axis, y along B0, z the third axis
  const SAMPLES = {
    tube:   { label: "50 mL tube in the sleeve", r: 0.0140, x0: -0.058, x1: 0.025, cone: 0.020 },
    bottle: { label: "bottle, full bore",        r: 0.0175, x0: -0.062, x1: 0.062, cone: 0 },
  };
  function voxels(kind, n = 3000, seed = 7) {
    const s = SAMPLES[kind], r = rng(seed), out = [];
    const xa = s.x0, xb = s.x1 + s.cone;
    while (out.length < n) {
      const x = xa + (xb - xa) * r(), y = (2 * r() - 1) * s.r, z = (2 * r() - 1) * s.r;
      const rad = x <= s.x1 ? s.r : s.r * (1 - (x - s.x1) / s.cone) + 0.002 * (x - s.x1) / s.cone;
      if (y * y + z * z <= rad * rad) out.push([x, y, z]);
    }
    // the volume, by the same sampling
    let vol = Math.PI * s.r * s.r * (s.x1 - s.x0);
    if (s.cone) vol += Math.PI / 3 * s.cone * (s.r * s.r + s.r * 0.002 + 0.002 * 0.002);
    return { pts: out, vol };
  }

  // B0 pair along y: Helmholtz with its 4th-order error, plus the Earth's field (along and across)
  function bField(p, I, bEpar, bEperp) {
    const [x, y, z] = p, R = P.hhR, rho2 = x * x + z * z;
    const err = -1.152 * (y ** 4 - 3 * y * y * rho2 + 0.375 * rho2 * rho2) / R ** 4;
    const bpar = P.B0nom * (I / P.hhI0) * (1 + err) + bEpar;
    return Math.hypot(bpar, bEperp);
  }

  // rotate v about unit axis n by angle th (Rodrigues)
  function rot(v, n, th) {
    const c = Math.cos(th), s = Math.sin(th), d = n[0] * v[0] + n[1] * v[1] + n[2] * v[2];
    return [v[0] * c + (n[1] * v[2] - n[2] * v[1]) * s + n[0] * d * (1 - c),
            v[1] * c + (n[2] * v[0] - n[0] * v[2]) * s + n[1] * d * (1 - c),
            v[2] * c + (n[0] * v[1] - n[1] * v[0]) * s + n[2] * d * (1 - c)];
  }

  // Exact field maps (simulator/build/nmr_exact.wls, Mathematica: Biot-Savart for the B0 pair's real
  // winding cross-section and every turn of the class coil; checked against SpinDynamica). Per voxel:
  // position (mm), line offset from 89.4 kHz at 1.5 A with no Earth field (Hz), the coil's field
  // perpendicular to B0 per amp (T/A) and its direction in the transverse plane (rad).
  const MAPS = {};
  function loadFieldMaps(base) {
    const get = k => fetch(base + "fieldmap-" + k + ".json").then(r => r.ok ? r.json() : null).then(m => { if (m) MAPS[k] = m; }).catch(() => {});
    return Promise.all(["tube", "bottle"].map(get));
  }
  function mapVoxels(m) {
    return { pts: m.p.map(q => [q[0] / 1000, q[1] / 1000, q[2] / 1000]), vol: m.p.length * (m.grid_mm / 1000) ** 3,
             df: m.df, b1: m.b1, phi: m.phi, centre: m.centre };
  }

  // set up one experiment; returns everything the page needs
  function setup(o) {
    const map = MAPS[o.sample] && o.fields !== "approx" ? mapVoxels(MAPS[o.sample]) : null;
    const { pts, vol } = map || voxels(o.sample);
    const fTx = o.fTx, I = o.b0I, tau = o.tau * 1e-6;
    const iTx = P.txV / Math.hypot(P.coilR, 2 * Math.PI * fTx * P.coilL);   // A pk in the coil
    const bPerA = MU0 * P.turns / P.coilLen;                                 // T/A, long-solenoid
    const dV = vol / pts.length;
    const vox = pts.map((p, i) => {
      let B, b1, phi;
      if (map) {                                                             // exact: the field scales with the current
        B = Math.hypot((89400 + map.df[i]) / GAMMA_BAR * (I / P.hhI0) + o.earthPar * 1e-6, o.earthPerp * 1e-6);
        b1 = map.b1[i]; phi = map.phi[i];
      } else {                                                               // approximate: 4th-order Helmholtz, on-axis solenoid
        B = bField(p, I, o.earthPar * 1e-6, o.earthPerp * 1e-6);
        b1 = bPerA * coilProfile(p[0]); phi = 0;
      }
      const dw = 2 * Math.PI * (GAMMA_BAR * B - fTx);                       // rad/s, in the frame at f_tx
      const w1 = GAMMA * 0.5 * b1 * iTx;                                    // rad/s, rotating component
      const W = Math.hypot(w1, dw);
      const n = [w1 * Math.cos(phi) / W, w1 * Math.sin(phi) / W, dw / W]; // M rotates about -B_eff
      const M0 = N_H * GAMMA * GAMMA * HBAR * HBAR * B / (4 * KB * P.temp);  // A/m
      return { p, b1, phi, dw, n, W, M0, m: rot([0, 0, 1], n, W * tau) };
    });
    // Mz before a scan in the averaged steady state, for this flip at the centre
    const centre = vox.reduce((a, v) => Math.abs(v.p[0]) < Math.abs(a.p[0]) ? v : a);
    const E1 = Math.exp(-P.TR / P.T1), cth = centre.m[2];
    const mzSS = (1 - E1) / (1 - E1 * cth);
    const avgFactor = (1 + (o.nAvg - 1) * mzSS) / o.nAvg;

    // receiver: tank-referred complex signal, then IF and ADC
    const fLo = fTx - P.fIF;
    const dt = 1 / P.rate, ns = Math.round(P.tAcq / dt);
    const wL = 2 * Math.PI * fTx;
    const amp = vox.map(v => wL * v.b1 * v.M0 * dV);                         // V of emf per voxel (reciprocity)
    const S = new Float64Array(2 * ns);                                       // complex baseband, noise-free
    for (let i = 0; i < vox.length; i++) {
      const v = vox[i];
      const cp = Math.cos(v.phi), sp = Math.sin(v.phi);                   // received with the coil's phase, e^{-i phi}
      let re = (v.m[0] * cp + v.m[1] * sp) * amp[i], im = (v.m[1] * cp - v.m[0] * sp) * amp[i];
      const c = Math.cos(-v.dw * dt), s = Math.sin(-v.dw * dt), d = Math.exp(-dt / P.T2);
      for (let k = 0; k < ns; k++) {
        S[2 * k] += re; S[2 * k + 1] += im;
        const r2 = (re * c - im * s) * d; im = (re * s + im * c) * d; re = r2;
      }
    }
    // the tank: Q at resonance, a single-pole response around it (the line offset sets the gain)
    let peak = 0, kp = 0;
    for (let k = 0; k < ns; k++) { const a = Math.hypot(S[2 * k], S[2 * k + 1]); if (a > peak) { peak = a; kp = k; } }
    const fLine = fTx + lineOffset(S, dt, ns);
    const tankGain = P.Q / Math.sqrt(1 + (2 * P.Q * (fLine - P.fTank) / P.fTank) ** 2);
    const sigma = P.en * Math.sqrt(P.bw) / Math.sqrt(o.nAvg);                 // tank V rms per channel
    const r = rng(o.seed || 1);
    const I_ = new Float64Array(ns), Q_ = new Float64Array(ns), env = new Float64Array(ns);
    for (let k = 0; k < ns; k++) {
      const t = k * dt, ph = 2 * Math.PI * P.fIF * t;
      const sr = S[2 * k] * tankGain * avgFactor, si = -S[2 * k + 1] * tankGain * avgFactor;   // conj: line above f_tx -> IF above 5.4 kHz
      const ir = sr * Math.cos(ph) - si * Math.sin(ph), qi = sr * Math.sin(ph) + si * Math.cos(ph);
      env[k] = Math.hypot(sr, si) * P.gain;
      const blank = k < Math.round(P.tDead / dt) ? 0 : 1;                   // DG419 holds stage 2 at AGND
      I_[k] = blank * (ir + sigma * gauss(r)) * P.gain;
      Q_[k] = blank * (qi + sigma * gauss(r)) * P.gain;
    }
    // T2*: envelope to 1/e of its value at the end of the pulse
    let t2s = null;
    for (let k = 1; k < ns; k++) if (env[k] < env[0] / Math.E) { t2s = k * dt; break; }
    // linewidth: FWHM of the noise-free spectrum of the acquired part, 1 Hz steps round the line
    // (S turns as e^{-i dw t}, so the line sits where S e^{+i 2 pi f t} adds up)
    const k0 = Math.round(P.tDead / dt), off = fLine - fTx;
    let best = 0; const spec = [];
    for (let f = off - 400; f <= off + 400; f += 1) {
      let re = 0, im = 0;
      for (let k = k0; k < ns; k += 2) { const ph = 2 * Math.PI * f * k * dt; re += S[2 * k] * Math.cos(ph) - S[2 * k + 1] * Math.sin(ph); im += S[2 * k + 1] * Math.cos(ph) + S[2 * k] * Math.sin(ph); }
      const a = Math.hypot(re, im); spec.push([f, a]); if (a > best) best = a;
    }
    const above = spec.filter(q => q[1] >= best / 2).map(q => q[0]);
    const fwhm = above.length ? above[above.length - 1] - above[0] + 1 : null;
    const fPeak = fTx + spec.reduce((a, q) => q[1] > a[1] ? q : a)[0];
    const mTot = vox.reduce((a, v) => a + v.M0 * dV, 0);
    const emfUntuned = Math.hypot(S[0], S[1]);
    return {
      vox, tau, dt, ns, I: I_, Q: Q_, env, fLine: fPeak, fwhm, fLo, fTx, iTx, mzSS, avgFactor, tankGain, t2s,
      sigmaAdc: sigma * P.gain, vol, mTot, emfUntuned, tankSignal: emfUntuned * tankGain * avgFactor,
      flipCentre: Math.acos(Math.max(-1, Math.min(1, centre.m[2]))) * 180 / Math.PI,
      t90Centre: (Math.PI / 2) / (GAMMA * 0.5 * iTx * (map ? map.centre.b1 : bPerA * coilProfile(0))),
      b1Centre: 0.5 * iTx * (map ? map.centre.b1 : bPerA * coilProfile(0)),
      fields: map ? "exact" : "approx",
      firstSample: Math.round(P.tDead / dt),
    };
  }
  // the line's offset from f_tx: phase slope of the noise-free signal over the first ms
  function lineOffset(S, dt, ns) {
    const k1 = Math.min(ns - 1, Math.round(1e-3 / dt));
    let acc = 0, n = 0;
    for (let k = 0; k < k1; k++) {
      const a = Math.atan2(S[2 * k + 3] * S[2 * k] - S[2 * k + 2] * S[2 * k + 1], S[2 * k + 2] * S[2 * k] + S[2 * k + 3] * S[2 * k + 1]);
      acc += a; n++;
    }
    return -(acc / n) / (2 * Math.PI * dt);
  }
  // M of every voxel at time t after the start of the pulse (for the 3D view)
  function state(sim, t, out) {
    for (let i = 0; i < sim.vox.length; i++) {
      const v = sim.vox[i];
      let m;
      if (t <= 0) m = [0, 0, 1];
      else if (t < sim.tau) m = rot([0, 0, 1], v.n, v.W * t);
      else {
        const s = t - sim.tau, ph = -v.dw * s, d = Math.exp(-s / P.T2);
        const c = Math.cos(ph), sn = Math.sin(ph);
        m = [(v.m[0] * c - v.m[1] * sn) * d, (v.m[0] * sn + v.m[1] * c) * d, 1 - (1 - v.m[2]) * Math.exp(-s / P.T1)];
      }
      out[i] = m;
    }
    return out;
  }
  // magnitude spectrum of the acquired part (after the dead time), zero-filled, around the IF
  function spectrum(sim, fMin, fMax, nOut = 400) {
    const k0 = sim.firstSample, out = new Float64Array(nOut);
    for (let j = 0; j < nOut; j++) {
      const f = fMin + (fMax - fMin) * j / (nOut - 1);
      let re = 0, im = 0;
      for (let k = k0; k < sim.ns; k++) {
        const ph = -2 * Math.PI * f * k * sim.dt;
        re += sim.I[k] * Math.cos(ph) - sim.Q[k] * Math.sin(ph);
        im += sim.I[k] * Math.sin(ph) + sim.Q[k] * Math.cos(ph);
      }
      out[j] = Math.hypot(re, im) / (sim.ns - k0);
    }
    return out;
  }
  root.NMR = { P, SAMPLES, setup, state, spectrum, coilProfile, GAMMA_BAR, loadFieldMaps, MAPS };
  if (typeof module !== "undefined") module.exports = root.NMR;
})(typeof window !== "undefined" ? window : globalThis);
