// The whole instrument on the bench: the housing with the boards, the probe on its tripod
// inside the B0 pair, and the three cables between them - and the checks that only make sense
// once the parts are put together (cable lengths, the RX tank with the coax on it, the B0 drive
// against the H-bridge, steel near the sample, the TX/RX wiring).
//
// The parts come from their own files (use <...> takes their modules and their numbers, not
// their output); the shared NMR numbers from nmr-params.scad. Nothing here is printed.
//
// Frame: the probe's - winding centre at x = y = 0, coil axis along x, B0 along y; the housing
// stands on the same table at +x, turned so its front edge (TX COIL terminal) faces the probe.
use <housing.scad>
use <probe.scad>
include <nmr-params.scad>

$fn = 32;

// ---------------------------------------------------------------- layout (change these)
housing_gap = 450;        // sample centre to the housing's front wall, mm (article: 30 cm decides)
single_coil = true;       // the class coil on both TX COIL and RX SMA (the workbook's numbers)
coax_pf_m   = 101;        // RG174 (RX); RG316 is 95, a twisted pair is ~50 but unscreened
slack       = 1.15;       // cable bought = route x slack

// ---------------------------------------------------------------- placing the housing
T  = probe_table_z();                        // table top, in the probe frame
tx = housing_port_tx();  rx = housing_port_rx();  hb = housing_port_hb();
H  = [housing_gap - tx[0][1], 90, T - housing_bottom_z()];   // turned -90: (x, y) -> (y, -x)
function hw(p) = [p[1] + H[0], -p[0] + H[1], p[2] + H[2]];  // housing point -> world
function hd(d) = [d[1], -d[0], d[2]];                        // housing direction -> world

// ---------------------------------------------------------------- cables
function seg_len(p, i = 0) = i >= len(p) - 1 ? 0 : norm(p[i + 1] - p[i]) + seg_len(p, i + 1);
module run(p, d) for (i = [0:len(p) - 2]) hull() { translate(p[i]) sphere(d = d); translate(p[i + 1]) sphere(d = d); }
// leave a port straight out for `out` mm, then down to the table
function from_port(port, out) = let(p = hw(port[0]), q = p + out * hd(port[1])) [p, q, [q[0], q[1], T + 3]];

E  = probe_cable_end();
L  = probe_b0_leads();
drop = [E, [E[0] + 40, E[1], E[2] - 30], [E[0] + 80, E[1], T + 3]];       // off the tripod to the table
tx_route = concat(drop, [for (i = [2:-1:0]) from_port(tx, 30)[i]]);
rx_route = concat([for (p = drop) p + [0, 6, 0]],
                  [[hw(rx[0])[0] - 40, hw(rx[0])[1], T + 3], hw(rx[0]) + [-40, 0, 40], hw(rx[0]) + [0, 0, 40], hw(rx[0])]);
hb_route = concat([L, [L[0], L[1] + 20, T + 3], [hw(hb[0])[0] + 60, L[1] + 20, T + 3]],
                  [for (i = [2:-1:0]) from_port(hb, 60)[i]]);

probe_assembly(b0 = true, table = false);
translate(H) rotate(-90) housing_assembly();
color("dimgray") run(tx_route, 4);          // TX: two-core, to the TX COIL screw terminal
color("black")   run(rx_route, 2.8);        // RX: RG174 coax, SMA on the cover
color("sienna")  run(hb_route, 6);          // B0: AWG18 pair, to the H-BRIDGE terminal
%translate([-350, -300, T - 5]) cube([housing_gap + 400, 600, 5]);           // the table

// ---------------------------------------------------------------- checks
len_tx = seg_len(tx_route) * slack;  len_rx = seg_len(rx_route) * slack;  len_hb = seg_len(hb_route) * slack;
echo(str("cables (route x ", slack, "): TX ", round(len_tx), " mm, RX coax ", round(len_rx), " mm, B0 ", round(len_hb), " mm"));

// RX tank: the coax hangs in parallel with the board's tank capacitors, so it is part of the tank
c_coax  = coax_pf_m * 1e-12 * len_rx / 1000;
subsets = [for (m = [0:7]) [m, (m % 2) * tank_fit[0] + (floor(m / 2) % 2) * tank_fit[1] + (floor(m / 4) % 2) * tank_fit[2]]];
need    = tank_C - c_coax;
best    = [for (s = subsets) if (abs(s[1] - need) == min([for (t = subsets) abs(t[1] - need)])) s][0];
names   = ["C710 1.2n", "C711 100p", "C712 22p"];
fitted  = [for (k = [0:2]) if (floor(best[0] / pow(2, k)) % 2 == 1) names[k]];
f_tank  = 1 / (2 * PI * sqrt(coil_L * (best[1] + c_coax)));
f_all   = 1 / (2 * PI * sqrt(coil_L * (tank_fit[0] + tank_fit[1] + tank_fit[2] + c_coax)));
gainl   = function(f) 1 / sqrt(1 + pow(2 * coil_Q * (f - nmr_f_tx) / nmr_f_tx, 2));
echo(str("RX tank: coil needs ", round(tank_C * 1e12), " pF; the coax adds ", round(c_coax * 1e12), " pF -> fit ",
         fitted, " = ", round(best[1] * 1e12), " pF, tank at ", round(f_tank), " Hz (",
         round(20 * log(gainl(f_tank)) * 100) / 100, " dB at f_tx); all three fitted: ", round(f_all), " Hz (",
         round(20 * log(gainl(f_all)) * 100) / 100, " dB)"));
b0 = probe_b0();                              // [R, I, ohm, N, build]
echo(str("or move the line to the tank: B0 current ", b0[1], " -> ", round(b0[1] * f_tank / nmr_f_tx * 1000) / 1000,
         " A puts the proton line at ", round(f_tank), " Hz (set f_tx and f_lo with it)"));

// B0 drive through the H-bridge, with the lead's resistance (AWG18, 20.9 mohm/m, there and back)
r_lead = 2 * len_hb / 1000 * 0.0209;
v_b0   = b0[1] * (b0[2] + r_lead);
echo(str("B0: ", b0[1], " A x (", round(b0[2] * 10) / 10, " + lead ", round(r_lead * 100) / 100, " ohm) = ",
         round(v_b0 * 10) / 10, " V of +VEXT ", vext_max, " V max; trip ", hb_trip, " A -> ",
         v_b0 <= vext_max && b0[1] < hb_trip ? "OK" : "NOT POSSIBLE"));

// B0 drift: the H-bridge holds a voltage, so as the copper warms its resistance rises cu_alpha per K
// and the current - and B0, and the line - fall with it. No cooling counted: the first minutes.
b0_mass = b0[3] * 2 * 2 * PI * b0[0] / 1000 * PI * pow(1.291 / 2000, 2) * 8960;   // kg, both coils (AWG16)
b0_P    = b0[1] * b0[1] * b0[2];
heat_Kmin = b0_P / (b0_mass * cu_c) * 60;
drift_Hz_K = cu_alpha * nmr_f_tx;
echo(str("B0 drift (voltage drive): ", round(b0_P), " W into ", round(b0_mass * 10) / 10, " kg of copper = +",
         round(heat_Kmin * 100) / 100, " K/min -> the line falls ", round(drift_Hz_K), " Hz per K = ",
         round(heat_Kmin * drift_Hz_K), " Hz/min; ", nmr_t_repeat, " s between scans -> ",
         round(heat_Kmin * drift_Hz_K * nmr_t_repeat / 60), " Hz between averaged scans (field-limited line: ~2 Hz). ",
         "Drive B0 at constant current (or measure the current every scan and correct f_lo) before averaging."));

// steel near the sample: the housing's M3 screws, nearest one
steel_d = min([for (s = housing_steel()) norm(hw([s[0], s[1], 0]) - probe_sample())]);
echo(str("nearest steel screw in the housing: ", round(steel_d), " mm from the sample ",
         steel_d >= 300 ? "(>= 300 mm, OK)" : "(< 300 mm: move the housing away)"));
ring_out = b0[0] + b0[4] / 2 + 3;
echo(str("B0 ring outer radius ", round(ring_out), " mm; housing front ", housing_gap, " mm -> ",
         housing_gap > ring_out + 50 ? "clear" : "TOO CLOSE"));

// TX and RX on one coil: the RX limiter sits directly on the RX SMA (sheet_nmr_rx: D703/D704 at the
// connector, before the tank), the TX drives the same coil through R813 4.7 ohm + C818
if (single_coil)
    echo(str("WARNING TX/RX: one coil on both terminals puts the crossed diodes (+-", rx_clamp, " V) across the ",
             "transmitter's +-", tx_vpk, " V: ", round((tx_vpk - rx_clamp) / 4.7 * 100) / 100,
             " A through R813 = the OPA564's 1.5 A limit -> I_FLAG, scan aborted. Needs a decision on the board side ",
             "(a series TX diode pair + RX series element, or a separate crossed receive coil) before a real scan."));
