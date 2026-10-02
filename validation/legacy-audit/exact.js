const N = require("C:/Users/Liang/tigp-2026/nmr-instrument/simulator/physics.js");
const fs = require("fs");
for (const k of ["tube","bottle"]) N.MAPS[k] = JSON.parse(fs.readFileSync(`C:/Users/Liang/tigp-2026/nmr-instrument/simulator/m/fieldmap-${k}.json`));
const base = { fTx: 89400, b0I: 1.5, tau: 417, earthPar: 0, earthPerp: 0, nAvg: 1, seed: 3 };
for (const [smp, fields] of [["tube","exact"],["bottle","exact"],["tube","approx"],["bottle","approx"]]) {
  let t0=Date.now(); const s = N.setup({...base, sample: smp, fields}); const tS=Date.now()-t0;
  t0=Date.now(); const sp = N.spectrum(s, 4000, 7000, 400); const tF=Date.now()-t0;
  t0=Date.now(); for(let i=0;i<10;i++){const out=[]; N.state(s, 0.05, out);} const tState=(Date.now()-t0)/10;
  console.log(smp, fields, "nvox", s.vox.length, "vol mL", (s.vol*1e6).toFixed(2), "emf uV", (s.emfUntuned*1e6).toFixed(4), "tank uV", (s.tankSignal*1e6).toFixed(3),
   "line", s.fLine, "fwhm", s.fwhm, "T2* ms", s.t2s && (s.t2s*1e3).toFixed(2), "t90c", (s.t90Centre*1e6).toFixed(2), "flipc", s.flipCentre.toFixed(2),
   "iTx mA", (s.iTx*1e3).toFixed(4), "b1rot uT", (s.b1Centre*1e6).toFixed(3), "tankGain", s.tankGain.toFixed(4), "sigmaADC", s.sigmaAdc.toFixed(4),
   "| setup ms", tS, "spectrum ms", tF, "state ms", tState);
}
