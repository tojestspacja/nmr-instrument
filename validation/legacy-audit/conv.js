const fs=require("fs");
for (const f of ["C:/Users/Liang/tigp-2026/nmr-instrument/simulator/physics.js","./physics_sdconv.js"]) {
  delete require.cache[require.resolve(f)]; const N=require(f);
  for (const k of ["tube","bottle"]) N.MAPS[k]=JSON.parse(fs.readFileSync(`C:/Users/Liang/tigp-2026/nmr-instrument/simulator/m/fieldmap-${k}.json`));
  for (const k of ["tube","bottle"]) { const s=N.setup({sample:k,fTx:89400,b0I:1.5,tau:417,earthPar:0,earthPerp:0,nAvg:1,seed:3});
    console.log(f.split("/").pop(),k,"emf uV",(s.emfUntuned*1e6).toFixed(5),"T2* ms",(s.t2s*1e3).toFixed(2),"line",s.fLine.toFixed(1)); }
}
