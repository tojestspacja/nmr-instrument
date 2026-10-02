include <../../mechanical/cut-housing.scad>
use <../../mechanical/probe.scad>
use <../../mechanical/housing.scad>
part = "none";
piece = "none";
comp = "box";
housing_gap = 450;
T  = probe_table_z();
txp = housing_port_tx();
HH = [housing_gap - txp[0][1], 90, T - housing_bottom_z()];
module placed() translate(HH) rotate(-90) children();
if (comp == "box")   placed() box();
if (comp == "shell") placed() shell();
if (comp == "hood")  placed() hood();
if (comp == "panel") placed() import("../../../class-board-2026/hardware/release/front-panel.stl");
if (comp == "main")  placed() translate([board_x, 0, -gap]) rotate([0, 180, 0]) import("../../../class-board-2026/hardware/release/class-board.stl");
if (comp == "dev")   placed() pbox(dev_x[0], dev_x[1], dev_py[0], dev_py[1], z_dev_low, z_dev_low + dev_parts + pcb);
