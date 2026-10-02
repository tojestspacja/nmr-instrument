const N = require("../physics.js");
const base = { sample: "tube", fTx: 89400, b0I: 1.5, tau: 417, earthPar: 0, earthPerp: 0, nAvg: 1, seed: 3 };
function show(name, o) {
  const t0 = Date.now(), s = N.setup(Object.assign({}, base, o));
  const sp = N.spectrum(s, s.fLine - s.fLo - 1500, s.fLine - s.fLo + 1500, 300); let jm = 0; sp.forEach((v, j) => { if (v > sp[jm]) jm = j; });
  const fpk = s.fLine - s.fLo - 1500 + 3000 * jm / 299;
  console.log(`${name.padEnd(28)} vol ${(s.vol*1e6).toFixed(1)} mL  m ${s.mTot.toExponential(2)} A m2  emf ${(s.emfUntuned*1e6).toFixed(2)} uV  tank ${(s.tankSignal*1e6).toFixed(2)} uV  ADC pk ${(s.env[0]).toFixed(3)} V  noise ${s.sigmaAdc.toFixed(3)} V rms  SNR0 ${(s.env[s.firstSample]/s.sigmaAdc).toFixed(1)}`);
  console.log(`${"".padEnd(28)} line ${s.fLine.toFixed(0)} Hz  spec pk IF ${fpk.toFixed(0)}  B1rot ${(s.b1Centre*1e6).toFixed(2)} uT  t90c ${(s.t90Centre*1e6).toFixed(0)} us  flip ${s.flipCentre.toFixed(1)} deg  T2* ${s.t2s ? (s.t2s*1e3).toFixed(1)+' ms' : '>200 ms'}  FWHM ${s.fwhm} Hz  env@dead ${(s.env[s.firstSample]/s.env[0]).toFixed(2)}  avgF ${s.avgFactor.toFixed(2)}  ${Date.now()-t0} ms`);
}
show("default", {});
show("bottle", { sample: "bottle" });
show("earth +40 uT along B0", { earthPar: 40 });
show("tau 900 us", { tau: 900 });
show("64 averages", { nAvg: 64 });
show("B0 1.484 A on tank", { b0I: 1.484, fTx: 88469 });
