(* Pulse-acquire on the class-board NMR setup with exact fields and SpinDynamica.

   Fields: Biot-Savart for circular current loops (complete elliptic integrals).
     - B0 pair: two coils on the y axis at y = +-R/2, mean radius R = 200 mm, winding
       cross-section 28 mm (axial) x 27 mm (radial), 312 turns each, as 5 x 5 filaments.
     - Class coil: every one of the 400 turns, two layers of AWG26 (0.45 mm) on the 40 mm
       former, axis along x, from x = -50 to +50 mm.
   Spins: SpinDynamica, one spin-1/2 per voxel, in the frame rotating at f_tx. The pulse is
   TransformationAmplitude[Iz -> {Ix, Iy, Iz}] under omega1 (cos phi Ix + sin phi Iy) + Omega Iz,
   with omega1 from the component of the coil's field perpendicular to the local B0 (half of it
   rotates with the spins) and phi its direction in the transverse plane.
   Detection: reciprocity with the same perpendicular field; T2 = 2 s; tank Q = 10.

   Usage: wolframscript -file nmr_exact.wls [grid_mm] [voxel_limit] [tube|bottle]
   SpinDynamica (v3.7.x for Mathematica 13) is looked for in the folder named by the environment
   variable SPINDYNAMICA, else in ./sd371 next to this file.                       *)

zsd = If[StringQ[Environment["SPINDYNAMICA"]], Environment["SPINDYNAMICA"], FileNameJoin[{DirectoryName[$InputFileName], "sd371"}]];
PrependTo[$Path, zsd]; SetDirectory[FileNameJoin[{zsd, "SpinDynamica"}]];
Quiet[Needs["SpinDynamica`"]];
SetDirectory[DirectoryName[$InputFileName]];
zargs = Rest[$ScriptCommandLine];
zgrid = If[Length[zargs] >= 1, ToExpression[zargs[[1]]], 2.5]*10^-3;     (* voxel pitch, m *)
zlim  = If[Length[zargs] >= 2, ToExpression[zargs[[2]]], Infinity];
zsample = If[Length[zargs] >= 3, zargs[[3]], "tube"];

(* ---------------- constants and the design (nmr-params.scad, probe.scad, workbook) *)
zmu0 = 4 Pi 10^-7; zgbar = 42.577*10^6; zgam = 2 Pi zgbar; zhbar = 1.054571817*10^-34; zkB = 1.380649*10^-23;
znH = 6.69*10^28; ztemp = 293.;
zfTx = 89400.; zfLo = 84000.; ztau = 417.*10^-6; zT2 = 2.0; zQ = 10; zfTank = 88469.;
ztxV = 7.95; zcoilL = 2.53*10^-3; zcoilR = 6.7;
ziTx = ztxV/Sqrt[zcoilR^2 + (2 Pi zfTx zcoilL)^2];                       (* A pk in the coil *)
zhhR = 0.200; zhhN = 312; zhhI = 1.5; zhhW = 0.028; zhhH = 0.027;        (* B0 pair *)
zearth = {0., 0., 0.};                                                (* T, add the room's field here *)

(* ---------------- field of one circular loop: centre c, unit axis n, radius a, current I *)
zloopB[p_, c_, n_, a_, i_] := Module[{d = p - c, u, rv, r, rhat, m, den, kk, ee, bu, br},
  u = d.n; rv = d - u n; r = Norm[rv]; rhat = If[r > 10^-12, rv/r, {0, 0, 0}];
  den = (a + r)^2 + u^2; m = 4 a r/den;
  kk = EllipticK[m]; ee = EllipticE[m];
  bu = zmu0 i/(2 Pi Sqrt[den]) (kk + (a^2 - r^2 - u^2)/((a - r)^2 + u^2) ee);
  br = If[r > 10^-12, zmu0 i u/(2 Pi r Sqrt[den]) (-kk + (a^2 + r^2 + u^2)/((a - r)^2 + u^2) ee), 0];
  bu n + br rhat];

(* B0 pair, 5 x 5 filaments per coil, each with 1/25 of the 312 turns *)
zfil = Flatten[Table[{s zhhR/2 + (j - 3)/5 zhhW, zhhR + (k - 3)/5 zhhH}, {s, {-1, 1}}, {j, 5}, {k, 5}], 2];
zb0At[p_] := Total[zloopB[p, {0, #[[1]], 0}, {0, 1, 0}, #[[2]], zhhI zhhN/25] & /@ zfil] + zearth;

(* class coil: 222 turns at r 20.225 mm, 178 at 20.675 mm, 0.45 mm pitch from x = -50 mm; per amp *)
zturns = Join[Table[{-0.050 + 0.000225 + 0.00045 k, 0.020225}, {k, 0, 221}], Table[{-0.050 + 0.000225 + 0.00045 k, 0.020675}, {k, 0, 177}]];
zb1At[p_] := Total[zloopB[p, {#[[1]], 0, 0}, {1, 0, 0}, #[[2]], 1.] & /@ zturns];

(* ---------------- the sample: water in the 50 mL tube (probe.scad: body x -58..25 mm, r 14 mm, cone to 45) *)
(* the bottle: full bore, r 17.5 mm, x -62..62 mm (probe.scad: 35.7 mm bottle in the 36 mm bore) *)
zinTube[{x_, y_, z_}] := If[zsample === "bottle", -0.062 <= x <= 0.062 && y^2 + z^2 <= 0.0175^2,
  Module[{r = If[x <= 0.025, 0.014, 0.014 (1 - (x - 0.025)/0.020) + 0.002 (x - 0.025)/0.020]},
  -0.058 <= x <= 0.045 && y^2 + z^2 <= r^2]];
zR = If[zsample === "bottle", 0.0175, 0.014]; zx1 = If[zsample === "bottle", 0.062, 0.045]; zx0 = If[zsample === "bottle", -0.062, -0.058];
zpts = Select[Flatten[Table[{x, y, z}, {x, zx0 + zgrid/2, zx1, zgrid}, {y, -zR + zgrid/2, zR, zgrid}, {z, -zR + zgrid/2, zR, zgrid}], 2], zinTube];
If[zlim < Length[zpts], zpts = RandomSample[SeedRandom[1]; zpts, zlim]];
zdV = zgrid^3; znv = Length[zpts];
Print["voxels: ", znv, " at ", zgrid 1000, " mm, water ", Round[znv zdV 10^6, 0.1], " mL"];

(* ---------------- fields at every voxel *)
{ztF, zfields} = AbsoluteTiming[Table[{zb0At[p], zb1At[p]}, {p, zpts}]];
zb0c = Norm[zb0At[{0, 0, 0}]];
Print["fields in ", Round[ztF, 0.1], " s; B0 at the coil centre ", ToString[N[Round[zb0c 1000, 10^-6]]], " mT at ", zhhI, " A (Helmholtz formula: 2.09973)"];

(* transverse frame: x' = coil axis (x), y' = -z, z' = local B0 direction *)
zvox = Table[Module[{b0 = zfields[[i, 1]], b1 = zfields[[i, 2]], n, bp, ex, ey, bmag},
    bmag = Norm[b0]; n = b0/bmag;
    ex = Normalize[{1, 0, 0} - ({1, 0, 0}.n) n]; ey = Cross[n, ex];
    bp = b1 - (b1.n) n;                                        (* coil field per amp, perpendicular to B0 *)
    <|"p" -> zpts[[i]], "B" -> bmag, "Om" -> 2 Pi (zgbar bmag - zfTx),
      "w1" -> zgam Norm[bp] ziTx/2, "phi" -> ArcTan[bp.ex, bp.ey + 10^-30], "bperp" -> Norm[bp],
      "M0" -> znH zgam^2 zhbar^2 bmag/(4 zkB ztemp)|>], {i, znv}];

(* ---------------- the pulse, with SpinDynamica *)
SetSpinSystem[1];
zpulseM[w1_, phi_, om_] := TransformationAmplitude[opI["z"] -> {opI["x"], opI["y"], opI["z"]},
   {w1 (Cos[phi] opI["x"] + Sin[phi] opI["y"]), ztau}, BackgroundGenerator -> om opI["z"]];
{ztP, zmAfter} = AbsoluteTiming[zpulseM[#w1, #phi, #Om] & /@ zvox];
Print["SpinDynamica pulse on ", znv, " voxels in ", Round[ztP, 0.1], " s"];

(* the centre: nutation, 90 deg time *)
zcen = First@MinimalBy[Range[znv], Norm[zvox[[#, "p"]]] &];
zw1c = zgam Norm[zb1At[{0, 0, 0}]] ziTx/2;
zmc = zpulseM[zw1c, 0, 0];
Print["centre: B1 ", N[Norm[zb1At[{0, 0, 0}]] ziTx/2 10^6, 4], " uT rotating, 90 deg in ",
  ToString[N[Round[(Pi/2)/zw1c 10^6, 10^-4]]], " us, flip at ", ztau 10^6, " us = ", N[ArcCos[zmc[[3]]]/Degree, 4], " deg (SpinDynamica)"];

(* ---------------- the FID: reciprocity, free precession, T2; 25 kS/s for 200 ms after the pulse *)
zdt = 1/25000.; zns = 5000; ztt = Range[0, zns - 1] zdt;
zamp = Table[2 Pi zfTx zvox[[i, "bperp"]] zvox[[i, "M0"]] zdV, {i, znv}];     (* V of emf per unit transverse M *)
zmp = Table[(zmAfter[[i, 1]] + I zmAfter[[i, 2]]) Exp[-I zvox[[i, "phi"]]], {i, znv}];
zoms = zvox[[All, "Om"]];
zsig = Exp[-ztt/zT2] Total[Table[zamp[[i]] zmp[[i]] Exp[-I zoms[[i]] ztt], {i, znv}]];
zenv = Abs[zsig];
zt2s = Module[{k = FirstPosition[zenv, _?(# < zenv[[1]]/E &)]}, If[MissingQ[k], Missing[], (k[[1]] - 1) zdt]];
(* line and its width: spectrum of the record after the 1.2 ms dead time, 1 Hz steps *)
zk0 = Round[1.2*10^-3/zdt] + 1;
zoffs = Range[-300, 300, 1] + Round[Mean[zoms]/(2 Pi)];
zspec = Table[Abs[Total[zsig[[zk0 ;;]] Exp[I 2 Pi f ztt[[zk0 ;;]]]]], {f, zoffs}];
zpk = zoffs[[First@Ordering[zspec, -1]]];
zabove = Pick[zoffs, Thread[zspec >= Max[zspec]/2]];
zfwhm = Max[zabove] - Min[zabove] + 1;
ztankGain = zQ/Sqrt[1 + (2 zQ (zfTx + zpk - zfTank)/zfTank)^2];
Print["coil emf ", N[zenv[[1]] 10^6, 3], " uV pk, at the tank ", N[zenv[[1]] ztankGain 10^6, 3], " uV pk"];
Print["line ", zfTx + zpk, " Hz, FWHM ", zfwhm, " Hz, T2* (envelope to 1/e) ", If[MissingQ[zt2s], "over 200 ms", ToString[Round[zt2s 1000, 0.1]] <> " ms"]];
Print["offset spread over the water: ", ToString[N[Round[(Max[zoms] - Min[zoms])/(2 Pi), 10^-4]]], " Hz (", ToString[N[Round[(Max[zoms] - Min[zoms])/(2 Pi zfTx) 10^6, 10^-4]]], " ppm)"];

(* ---------------- results for the page and for the record *)
Export["exact-" <> zsample <> "-summary.json", <|"voxels" -> znv, "grid_mm" -> zgrid 1000, "water_mL" -> znv zdV 10^6,
   "B0_centre_mT" -> zb0c 1000, "B1_centre_uT" -> Norm[zb1At[{0, 0, 0}]] ziTx/2 10^6, "t90_centre_us" -> (Pi/2)/zw1c 10^6,
   "flip_centre_deg" -> ArcCos[zmc[[3]]]/Degree, "emf_uV" -> zenv[[1]] 10^6, "tank_uV" -> zenv[[1]] ztankGain 10^6,
   "line_Hz" -> zfTx + zpk, "fwhm_Hz" -> zfwhm, "T2star_ms" -> If[MissingQ[zt2s], Null, zt2s 1000],
   "spread_Hz" -> (Max[zoms] - Min[zoms])/(2 Pi), "seconds_fields" -> ztF, "seconds_spindynamica" -> ztP|>, "JSON"];
Export["exact-" <> zsample <> "-fid.png", ListLinePlot[Transpose[{ztt 1000, zenv 10^6}], PlotRange -> All, Frame -> True,
   FrameLabel -> {"t after the pulse (ms)", "|FID| at the coil (\[Micro]V)"}, ImageSize -> 600]];
Export["exact-" <> zsample <> "-spectrum.png", ListLinePlot[Transpose[{zfTx + zoffs, zspec/Max[zspec]}], PlotRange -> All, Frame -> True,
   FrameLabel -> {"frequency (Hz)", "spectrum (normalised)"}, ImageSize -> 600]];
(* per-voxel field map for the web page: position (mm), offset from 89.4 kHz (Hz) and B1 per amp *)
Export["exact-" <> zsample <> "-fieldmap.json", Table[<|"p" -> Round[1000 zvox[[i, "p"]], 0.01], "df" -> Round[zvox[[i, "Om"]]/(2 Pi), 0.001],
   "b1" -> zvox[[i, "bperp"]], "phi" -> Round[zvox[[i, "phi"]], 0.0001]|>, {i, znv}], "JSON"];
Print["written exact-", zsample, "-{summary,fieldmap}.json and the two plots"];
