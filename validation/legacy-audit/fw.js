const fs=require("fs"); const N=require("C:/Users/Liang/tigp-2026/nmr-instrument/simulator/physics.js");
N.MAPS.tube=JSON.parse(fs.readFileSync("C:/Users/Liang/tigp-2026/nmr-instrument/simulator/m/fieldmap-tube.json"));
for (const T of [0.1,0.2,0.4,0.8]) { N.P.tAcq=T; const t0=Date.now(); const s=N.setup({sample:"tube",fTx:89400,b0I:1.5,tau:417,earthPar:0,earthPerp:0,nAvg:1,seed:3});
 console.log("tAcq",T,"FWHM",s.fwhm,"Hz  (1.207/T =",(1.207/(T-0.0012)).toFixed(1),") setup ms",Date.now()-t0); }
