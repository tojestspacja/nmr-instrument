// Runs one experiment on the WASM core and prints JSON for tests/test_wasm_parity.py to compare with native.
import { readFileSync } from 'node:fs';
import { loadCore } from '../simulator/web/nmrcore.js';

const core = await loadCore(readFileSync(new URL('../simulator/web/nmrcore.wasm', import.meta.url)));
const nvox = core.voxelize(0.005);
const seq = { ...core.defaultSequence(0), n_avg: 4, acq_len: 0.2 };
const prog = core.compile(seq);
const recs = core.simulate(prog, core.defaultModel());
const p = core.process();
const spec = core.spectrum(p.reim, p.fs, 0, 0, 4);
const pk = core.peak(spec, -400, 400, 10);
console.log(JSON.stringify({
  sha: core.configSha256(), nvox, prog: Array.from(prog), nrec: recs.length,
  i0: Array.from(recs[0].i.slice(0, 2000)), q3: Array.from(recs[3].q.slice(0, 2000)),
  t_first: recs.map((r) => r.t_first), proc: Array.from(p.reim.slice(0, 200)), fs: p.fs, t0: p.t0, peak: pk,
}));
