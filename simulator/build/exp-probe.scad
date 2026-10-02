// one component of the bench setup per run, in instrument.scad's world frame
include <../../mechanical/probe.scad>
use <../../mechanical/housing.scad>
part = "none";
comp = "cradle";
$fn = 64;
housing_gap = 450;
T  = z_table;
tx = housing_port_tx();  rx = housing_port_rx();  hb = housing_port_hb();
HH = [housing_gap - tx[0][1], 90, T - housing_bottom_z()];
function hw(p) = [p[1] + HH[0], -p[0] + HH[1], p[2] + HH[2]];
function hd(d) = [d[1], -d[0], d[2]];
module run(p, d) for (i = [0:len(p) - 2]) hull() { translate(p[i]) sphere(d = d, $fn = 12); translate(p[i + 1]) sphere(d = d, $fn = 12); }
function from_port(port, out) = let(p = hw(port[0]), q = p + out * hd(port[1])) [p, q, [q[0], q[1], T + 3]];
E  = [base_x[1] + 40, 0, anchor[2] + 2.5];
L  = [0, hh_R / 2 + hh_w / 2, axis_z - hh_R - hh_h / 2];
drop = [E, [E[0] + 40, E[1], E[2] - 30], [E[0] + 80, E[1], T + 3]];
tx_route = concat(drop, [for (i = [2:-1:0]) from_port(tx, 30)[i]]);
rx_route = concat([for (p = drop) p + [0, 6, 0]],
                  [[hw(rx[0])[0] - 40, hw(rx[0])[1], T + 3], hw(rx[0]) + [-40, 0, 40], hw(rx[0]) + [0, 0, 40], hw(rx[0])]);
hb_route = concat([L, [L[0], L[1] + 20, T + 3], [hw(hb[0])[0] + 60, L[1] + 20, T + 3]],
                  [for (i = [2:-1:0]) from_port(hb, 60)[i]]);
module onprobe() translate([-z_c, 0, axis_z]) rotate([0, 90, 0]) children();
if (comp == "cradle")  cradle();
if (comp == "former")  onprobe() former();
if (comp == "winding") onprobe() winding();
if (comp == "sleeve")  onprobe() translate([0, 0, -fl_t]) sleeve();
if (comp == "sample")  onprobe() translate([0, 0, -fl_t - cap_h]) sample_tube();
if (comp == "ties")    { onprobe() stub_tie(); translate([base_x[1] - anchor[0] / 2 - 2, 0, anchor[2] - 5]) cube([3, anchor[1] + 2, 9], center = true); }
if (comp == "probe_cable") cable();
if (comp == "b0_rings")    for (s = [-1, 1]) translate([0, s * hh_R / 2, axis_z]) b0_ring();
if (comp == "b0_windings") for (s = [-1, 1]) translate([0, s * hh_R / 2, axis_z]) b0_winding();
if (comp == "tripod")  translate([0, 0, T]) cylinder(d = 25, h = -T);
if (comp == "cable_tx") run(tx_route, 4);
if (comp == "cable_rx") run(rx_route, 2.8);
if (comp == "cable_hb") run(hb_route, 6);
if (comp == "info") echo(str("INFO|", T, "|", HH[0], "|", HH[1], "|", HH[2]));
