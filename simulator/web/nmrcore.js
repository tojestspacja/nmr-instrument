// JavaScript access to the C++ NMR core compiled to WebAssembly (simulator/wasm/build.py).
// Thin marshalling only: every number is computed by the same C++ the native tests and the Python tools use.
// Works in the browser and in Node (tests/wasm_parity.mjs).

const LAYOUT = {          // byte sizes, checked against nmr_abi_layout() at load time
  timing: 40, sequence: 72, limits: 64, model: 200, record: 64, pipeline: 64,
};
const F = {               // field offsets (wasm32: double 8-aligned)
  timing: { tick_hz: 0, tx_freq_hz: 8, pre_blank_s: 16, dead_time_s: 24, repetition_s: 32 },
  sequence: { kind: [0, 'i32'], t90: 8, t180: 16, tau: 24, ti: 32, acq_start: 40, acq_len: 48,
              n_echoes: [56, 'i32'], n_avg: [60, 'i32'], cyclops: [64, 'i32'] },
  model: { gamma_bar: 0, b0_current: 8, t1: 16, t2: 24, proton_density: 32, temperature: 40, earth_x: 48,
           earth_y: 56, earth_z: 64, tx_coil_current: 72, tank_f0: 80, tank_q: 88, rx_gain: 96, if_pole_hz: 104,
           noise_density: 112, lo_hz: 120, adc_full_scale: 128, adc_rate: 136, adc_t0: 144, offset_i: 152,
           offset_q: 160, adc_bits: [168, 'i32'], lo_coherent: [172, 'i32'], interp: [176, 'i32'],
           beat_measurement_sigma: 184, seed: [192, 'u64'] },
  record: { scan: [0, 'u32'], window: [4, 'u32'], t_first: 8, t_excitation: 16, rx_phase_turns: 24,
            beat_phase_true: 32, beat_phase_measured: 40, f_tx: 48, n: [56, 'i32'] },
  pipeline: { adc_rate: 0, adc_full_scale: 8, fir_cutoff_hz: 16, offset_tail: 24, lo_hz: 32, adc_bits: [40, 'i32'],
              decimation: [44, 'i32'], fir_taps: [48, 'i32'], correct_beat: [52, 'i32'], window_id: [56, 'i32'] },
};

function wasiStubs(getMem, log) {
  const ENOSYS = 52, EBADF = 8;
  return {
    environ_sizes_get: (pc, ps) => { const v = new DataView(getMem().buffer); v.setUint32(pc, 0, true); v.setUint32(ps, 0, true); return 0; },
    environ_get: () => 0,
    fd_prestat_get: () => EBADF,
    fd_prestat_dir_name: () => EBADF,
    fd_close: () => 0,
    fd_seek: () => ENOSYS,
    fd_write: (fd, iovs, n, pw) => {
      const v = new DataView(getMem().buffer); let w = 0, s = '';
      for (let k = 0; k < n; k++) {
        const p = v.getUint32(iovs + 8 * k, true), l = v.getUint32(iovs + 8 * k + 4, true);
        s += new TextDecoder().decode(new Uint8Array(getMem().buffer, p, l)); w += l;
      }
      v.setUint32(pw, w, true); log(s); return 0;
    },
    proc_exit: (c) => { throw new Error(`nmrcore exited with ${c}`); },
  };
}

export async function loadCore(source, { log = () => {} } = {}) {
  let mem;
  const imports = { wasi_snapshot_preview1: wasiStubs(() => mem, log) };
  const bytes = source instanceof ArrayBuffer || ArrayBuffer.isView(source) ? source : await (await fetch(source)).arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  const X = instance.exports;
  mem = X.memory;
  X._initialize();
  return new Core(X);
}

class Core {
  constructor(X) {
    this.X = X;
    const p = X.malloc(24);
    X.nmr_abi_layout(p);
    const s = new Int32Array(X.memory.buffer, p, 6);
    const want = [LAYOUT.timing, LAYOUT.sequence, LAYOUT.limits, LAYOUT.model, LAYOUT.record, LAYOUT.pipeline];
    if (want.some((w, k) => w !== s[k])) throw new Error(`ABI layout mismatch: wasm ${[...s]} vs js ${want}`);
    X.free(p);
  }
  get dv() { return new DataView(this.X.memory.buffer); }
  str(p) { const b = new Uint8Array(this.X.memory.buffer, p); let n = 0; while (b[n]) n++; return new TextDecoder().decode(b.subarray(0, n)); }
  error() { return this.str(this.X.nmr_last_error()); }
  configSha256() { return this.str(this.X.nmr_config_sha256()); }

  // struct <-> object
  read(kind, p) {
    const o = {}, v = this.dv;
    for (const [k, d] of Object.entries(F[kind])) {
      const [off, t] = Array.isArray(d) ? d : [d, 'f64'];
      o[k] = t === 'i32' ? v.getInt32(p + off, true) : t === 'u32' ? v.getUint32(p + off, true)
        : t === 'u64' ? Number(v.getBigUint64(p + off, true)) : v.getFloat64(p + off, true);
    }
    return o;
  }
  write(kind, p, o) {
    const v = this.dv;
    for (const [k, d] of Object.entries(F[kind])) {
      if (!(k in o)) continue;
      const [off, t] = Array.isArray(d) ? d : [d, 'f64'];
      if (t === 'i32') v.setInt32(p + off, o[k], true);
      else if (t === 'u32') v.setUint32(p + off, o[k], true);
      else if (t === 'u64') v.setBigUint64(p + off, BigInt(o[k]), true);
      else v.setFloat64(p + off, o[k], true);
    }
  }
  withStruct(kind, init, fn) {
    const p = this.X.malloc(LAYOUT[kind]);
    try { new Uint8Array(this.X.memory.buffer, p, LAYOUT[kind]).fill(0); init(p); return fn(p); } finally { this.X.free(p); }
  }
  defaults(kind, fnName, arg) {
    return this.withStruct(kind, (p) => (arg === undefined ? this.X[fnName](p) : this.X[fnName](p, arg)), (p) => this.read(kind, p));
  }
  defaultTiming() { return this.defaults('timing', 'nmr_default_timing'); }
  defaultSequence(kind) { return this.defaults('sequence', 'nmr_default_sequence', kind); }
  defaultModel() { return this.defaults('model', 'nmr_default_model'); }
  defaultPipeline() { return this.defaults('pipeline', 'nmr_default_pipeline'); }

  compile(sequence, timing = this.defaultTiming()) {
    const X = this.X, cap = 8 * 8192 + 64, out = X.malloc(cap);
    try {
      const n = this.withStruct('timing', (pt) => this.write('timing', pt, timing),
        (pt) => this.withStruct('sequence', (ps) => this.write('sequence', ps, sequence), (ps) => X.nmr_compile(pt, ps, out, cap)));
      if (n < 0) throw new Error(this.error());
      return new Uint8Array(X.memory.buffer, out, n).slice();
    } finally { X.free(out); }
  }
  withBytes(bytes, fn) {
    const p = this.X.malloc(bytes.length);
    try { new Uint8Array(this.X.memory.buffer, p, bytes.length).set(bytes); return fn(p); } finally { this.X.free(p); }
  }
  disassemble(prog) {
    const cap = 1 << 20, out = this.X.malloc(cap);
    try { return this.withBytes(prog, (p) => (this.X.nmr_disassemble(p, prog.length, out, cap) < 0 ? this.error() : this.str(out))); }
    finally { this.X.free(out); }
  }
  voxelize(grid) { return this.X.nmr_voxelize(grid); }
  voxels() {
    const cap = 20000, p = this.X.malloc(cap * 56);
    try { const k = this.X.nmr_voxels(p, cap); return new Float64Array(this.X.memory.buffer, p, 7 * k).slice(); } finally { this.X.free(p); }
  }
  simulate(prog, model = this.defaultModel()) {
    const n = this.withBytes(prog, (pp) => this.withStruct('model', (pm) => this.write('model', pm, model), (pm) => this.X.nmr_simulate(pp, prog.length, pm)));
    if (n < 0) throw new Error(this.error());
    const recs = [];
    for (let k = 0; k < n; k++) recs.push(this.record(k));
    return recs;
  }
  record(k) {
    return this.withStruct('record', () => {}, (pm) => {
      const n = this.X.nmr_record(k, pm, 0, 0, 0);
      const pi = this.X.malloc(2 * n), pq = this.X.malloc(2 * n);
      try {
        this.X.nmr_record(k, pm, pi, pq, n);
        const meta = this.read('record', pm);
        meta.i = new Int16Array(this.X.memory.buffer, pi, n).slice();
        meta.q = new Int16Array(this.X.memory.buffer, pq, n).slice();
        return meta;
      } finally { this.X.free(pi); this.X.free(pq); }
    });
  }
  loadRecords(recs) {   // measured data, same format as simulate()'s output
    this.X.nmr_clear_records();
    for (const r of recs) {
      this.withStruct('record', (pm) => this.write('record', pm, { ...r, n: r.i.length }), (pm) => {
        const pi = this.X.malloc(2 * r.i.length), pq = this.X.malloc(2 * r.q.length);
        try {
          new Int16Array(this.X.memory.buffer, pi, r.i.length).set(r.i);
          new Int16Array(this.X.memory.buffer, pq, r.q.length).set(r.q);
          this.X.nmr_load_record(pm, pi, pq);
        } finally { this.X.free(pi); this.X.free(pq); }
      });
    }
  }
  process(pipeline = this.defaultPipeline(), window = 0) {
    const cap = 1 << 20, out = this.X.malloc(16 * cap), pf = this.X.malloc(16);
    try {
      const n = this.withStruct('pipeline', (pp) => this.write('pipeline', pp, { ...pipeline, window_id: window }),
        (pp) => this.X.nmr_process(pp, out, cap, pf, pf + 8));
      if (n < 0) throw new Error(this.error());
      const re = new Float64Array(this.X.memory.buffer, out, 2 * n).slice();
      return { reim: re, fs: this.dv.getFloat64(pf, true), t0: this.dv.getFloat64(pf + 8, true) };
    } finally { this.X.free(out); this.X.free(pf); }
  }
  spectrum(reim, fs, window = 0, tc = 0, pad = 2) {
    const n = reim.length / 2, cap = 1 << 21;
    const px = this.X.malloc(8 * reim.length), pf = this.X.malloc(8 * cap), pv = this.X.malloc(16 * cap);
    try {
      new Float64Array(this.X.memory.buffer, px, reim.length).set(reim);
      const m = this.X.nmr_spectrum(px, n, fs, window, tc, pad, pf, pv, cap);
      if (m < 0) throw new Error(this.error());
      return { freq: new Float64Array(this.X.memory.buffer, pf, m).slice(), reim: new Float64Array(this.X.memory.buffer, pv, 2 * m).slice() };
    } finally { this.X.free(px); this.X.free(pf); this.X.free(pv); }
  }
  peak(spec, fMin, fMax, guard) {
    const m = spec.freq.length, pf = this.X.malloc(8 * m), pv = this.X.malloc(16 * m), po = this.X.malloc(32);
    try {
      new Float64Array(this.X.memory.buffer, pf, m).set(spec.freq);
      new Float64Array(this.X.memory.buffer, pv, 2 * m).set(spec.reim);
      this.X.nmr_peak(pf, pv, m, fMin, fMax, guard, po);
      const o = new Float64Array(this.X.memory.buffer, po, 4);
      return { freq_hz: o[0], amplitude: o[1], noise_rms: o[2], snr: o[3] };
    } finally { this.X.free(pf); this.X.free(pv); this.X.free(po); }
  }
}
