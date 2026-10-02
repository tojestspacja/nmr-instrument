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
pts = {{-0.05675,-0.01275,-0.00525},{-0.04425,-0.01025,0.00475},{-0.01175,-0.00775,-0.00775},{0.00575,-0.00025,0.00475},{0.04325,0.00225,-0.00025}};
{t, r} = AbsoluteTiming[Table[Module[{b0 = zb0At[p], b1 = zb1At[p], n, bp, ex, ey},
   n = b0/Norm[b0]; ex = Normalize[{1, 0, 0} - ({1, 0, 0}.n) n]; ey = Cross[n, ex]; bp = b1 - (b1.n) n;
   {p 1000, zgbar Norm[b0] - zfTx, Norm[bp], ArcTan[bp.ex, bp.ey + 10^-30]}], {p, pts}]];
Print[NumberForm[#, 8] & /@ r];
Print["B0 centre mT ", NumberForm[1000 Norm[zb0At[{0, 0, 0}]], 10], "  seconds per 5 voxels ", t];
