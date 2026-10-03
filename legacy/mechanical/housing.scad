// Housing for the class board: a printed shell (cover + walls, open at the bottom)
// and a laser-cut bottom plate screwed on with M3 screws into nuts in side-open pockets.
//
// Frame: the front panel's KiCad frame, panel face up. x 0..180, y -115..-15 (KiCad y points
// down, so y = -(KiCad y)); the rear edge (USB-C, DC jack, terminals) is y = -15, the far
// edge with the screw terminals is y = -115. z = 0 is the panel's back surface.
//
// part = "assembly" (boards drawn transparent), "shell" (print orientation, cover on the bed),
// "hood" (OLED surround, flange on the bed), "plate" (2D, export as DXF), "plate3d", "coupon";
// "shell_in_place" / "hood_in_place" are in the board frame, for collision checks.
part = "assembly";

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- boards (from the models)
board_x = 180;  board_py0 = 15;  board_py1 = 115;   // KiCad y range of both boards
pcb   = 1.6;
gap   = 11;                       // facing surfaces: 8.5 mm socket on a 2.5 mm insulator
z_face   = pcb;                   // panel outer face
z_main_b = -gap;                  // main board back (link sockets, faces up)
z_main_f = -gap - pcb;            // main board front (dev-board side, faces down) = -12.6
holes = [[4, 19], [176, 19], [4, 111], [176, 111]];   // M3, both boards (KiCad x, y)

// Dev board (not in the models): Jinhua #40729, 28 x 57 mm listed, centred on sockets J1/J2
// (main x 52..108.5, KiCad y 56.3 / 81.3 -> housing x 71.5..128). Brief: hangs 17 mm below
// the main board; its parts (module, USB-C, buttons) face down, about 3.5 mm.
dev_drop  = 17;
dev_parts = 3.5;
dev_x = [99.75 - 28.5, 99.75 + 28.5];
dev_py = [68.8 - 14, 68.8 + 14];
z_dev_low = z_main_f - dev_drop - pcb - dev_parts;   // lowest point of the dev board

// ---------------------------------------------------------------- housing parameters
wall       = 2;
board_gap  = 0.5;    // board edge to the inner wall face
fit        = 0.3;    // every hole 0.3 mm larger than the part through it
cover_t    = 2;      // cover thickness
cover_clr  = 3.0;    // panel face to the cover's underside (link-header tails 1.31 mm)
floor_clr  = 3.0;    // dev board's lowest part to the top of the bottom plate
plate_t    = 3;      // laser-cut plate (3 mm acrylic)
r_out      = 3;      // outer corner radius

z_cov_u = z_face + cover_clr;        // cover underside
z_top   = z_cov_u + cover_t;         // cover top surface
z_rim   = z_dev_low - floor_clr;     // bottom edge of the shell = top of the plate

in_x  = [-board_gap, board_x + board_gap];
in_py = [board_py0 - board_gap, board_py1 + board_gap];
out_x  = [in_x[0] - wall, in_x[1] + wall];
out_py = [in_py[0] - wall, in_py[1] + wall];

// M3 nut pocket rule: 5.8 mm across flats, 2.5 mm deep, open to the side
m3_hole = 3.0 + fit;
nut_af  = 5.8;
nut_h   = 2.5;

// ---------------------------------------------------------------- helpers
// box in housing coordinates given KiCad-style py (positive) ranges
module pbox(x0, x1, py0, py1, z0, z1) {
    translate([x0, -py1, z0]) cube([x1 - x0, py1 - py0, z1 - z0]);
}
module rbox(x0, x1, py0, py1, z0, z1, r) {
    hull() for (x = [x0 + r, x1 - r], py = [py0 + r, py1 - r])
        translate([x, -py, z0]) cylinder(r = r, h = z1 - z0);
}
module rrect(x0, x1, py0, py1, r) {
    hull() for (x = [x0 + r, x1 - r], py = [py0 + r, py1 - r]) translate([x, -py]) circle(r = r);
}
module hex(af) rotate(30) circle(d = af / cos(30), $fn = 6);   // flats facing +-x

// ---------------------------------------------------------------- cover cut-outs (panel frame)
sma_cols = [for (n = [0:5]) 75.65 + 18 * n];
sma = concat([for (x = sma_cols) [x, 25.67]], [for (x = sma_cols) [x, 43.67]],
             [for (n = [0:3]) [sma_cols[n], 61.67]]);
sma_d = 6.35 + fit;                  // 1/4-36 thread

// OLED module outline from the mesh (z > 12): x 137.55..164.55, KiCad y 72.65..101.65
oled = [137.55, 164.55, 72.65, 101.65];
hood_clr = 0.5;  hood_wall = 1.6;  hood_flange = 2.0;  hood_flange_t = cover_clr - 0.3;
hood_h = 13.82 + 0.5;                // module top + 0.5, above the panel face
hood_in  = [oled[0] - hood_clr, oled[1] + hood_clr, oled[2] - hood_clr, oled[3] + hood_clr];
hood_out = [hood_in[0] - hood_wall, hood_in[1] + hood_wall, hood_in[2] - hood_wall, hood_in[3] + hood_wall];
hood_fl  = [hood_out[0] - hood_flange, hood_out[1] + hood_flange, hood_out[2] - hood_flange, hood_out[3] + hood_flange];

// windows through the cover only: [x0, x1, py0, py1] (mesh extents + margin)
windows = [
    [hood_out[0] - fit/2, hood_out[1] + fit/2, hood_out[2] - fit/2, hood_out[3] + fit/2],  // OLED hood
    [114.7, 118.3, 85.7, 93.8],        // three LEDs (x 115..118, y 86..93.5), one light slot
    [117.5, 124.5, 73.0, 82.5],        // Qwiic J33 (x 119..123, y 74.5..81) + finger room
    [23.3, 46.2, 102.9, 114.0],        // module header J40 (x 24.3..45.2, y 103.9..113)
];
// screw terminals on the far edge: window through the cover AND a notch through the front
// wall, so the wire comes in from the side and the screwdriver from the top
edge_terms = [
    [130.4, 157.8, 107.0],             // TTL strip J31 (x 131.4..156.8, y 108..114.3)
    [104.55, 116.85, 103.05],          // TX terminal J32 (x 105.55..115.85, y 104.05..)
    [80.85, 95.45, 102.45],            // isolated input J411 (x 81.85..94.45, y 102.75..); rear edge
    [53.38, 67.13, 102.2],             // isolated input J412 (x 54.38..66.13, y 102.5..)   only + fit
];

// ---------------------------------------------------------------- chips under the cover
// From the full panel model (kicad-cli export with every component): everything under the
// cover is at most 1.32 mm above the face (header tails, SMD passives, U410 1.03) except the
// two optocouplers U401/U402 (wide SOP-8, 3.55 mm). They get pockets in the cover's underside.
chip_clr = 0.5;
chips = [                              // [x0, x1, KiCad y0, y1, height above the face]
    [78.0, 86.5, 86.0, 96.5, 3.55],    // U401, isolated input 1
    [50.5, 59.0, 86.0, 96.5, 3.55],    // U402, isolated input 2
];
module chip_pockets()
    for (c = chips) pbox(c[0] - chip_clr, c[1] + chip_clr, c[2] - chip_clr, c[3] + chip_clr,
                         z_cov_u - 1, z_face + c[4] + chip_clr);
z_notch = z_face + 0.6;                // the notch's sides start 0.6 mm above the panel face

// ---------------------------------------------------------------- rear-wall openings
// Main board turned over: housing x = 180 - main x; its rear parts hang down from z = -12.6.
// [x0, x1, z0 (bottom), z1 (top)] with plug/wire room; the lower edge is a 45 deg V so the
// shell prints upside down without supports.
rear_v = [
    [155.25, 168.75, -16.6, -7.6],     // USB-C J201 (main x 13.5..22.5): 12.5 x 7 plug overmold + 1
    [141.45, 153.95, -24.9, -11.1],    // DC jack J202 (main x 27.55..37.05, 10.8 tall)
];
// Coil and supply terminals J905, J903, J901 (main x 72.0..87.65, 58.7..69.3, 43.4..55.3) overlap
// once given room, so they share one opening, open to the bottom edge; the plate closes it.
// J905's model is 10 mm tall below the board; J901/J903's models are shifted 8.5 mm into the
// board (z -6.85..6.55, impossible for a real part), so they are sized like J905.
rear_open = [90.85, 138.15, -24.1];    // x0, x1, top z = board + 1.5 ... (bottom = rim)
rear_open_top = z_main_f + 1.5;

// ---------------------------------------------------------------- labels (engraved)
// The cover hides the panel silkscreen, so the connectors are named on the housing: SMAs on
// the cover; the edge terminals by name on the front wall, under their notches, and pin by pin
// on the cover beside the wire entries; the rear terminals by name and pin on the rear wall.
// Names and pin roles from the nets in front-panel.kicad_pcb and class-board.kicad_pcb.
// Text is 2.0-4.2 mm tall, its strokes thickened by label_grow so they print with a 0.4 mm
// nozzle; + and - are drawn as 0.6 mm bars. 2 mm is at the printer's limit: the coupon has
// samples at 2.0 / 2.5 / 3.0 mm to check before printing the shell.
label_depth = 0.6;
label_grow  = 0.05;
label_font  = "Liberation Sans:style=Bold";
sma_names = [["AI1", 75.65, 25.67], ["AI2", 93.65, 25.67], ["AI3", 111.65, 25.67], ["AI4", 129.65, 25.67],
             ["AUX", 147.65, 25.67], ["FAST1", 165.65, 25.67],
             ["AI5", 75.65, 43.67], ["AI6", 93.65, 43.67], ["AI7", 111.65, 43.67], ["AI8", 129.65, 43.67],
             ["RX", 147.65, 43.67], ["FAST2", 165.65, 43.67],
             ["AO1", 75.65, 61.67], ["AO2", 93.65, 61.67], ["TX", 111.65, 61.67], ["TRIG", 129.65, 61.67]];
nmr_names = ["TX", "RX", "TRIG"];
sma_label_dy = 7.2;                    // label centre in front of the jack
title = ["NMR SPECTROMETER", "TIGP 2026"];   // on the free left part of the cover
title_at = [34, 45];                           // x, KiCad y

// Under the title, a working scale instead of a picture: field B0 (mT, above) against the proton
// Larmor frequency (kHz, below), f = 42.577 kHz/mT x B0, for setting f_tx to the field coil and
// f_lo = f_tx - IF to match. Engraved like the labels and read with the instrument on the bench.
// The model that shows the data itself is spectrum.scad (after Jones et al., J. Chem. Educ. 2021,
// 98, 1024). The default printed here is the firmware's (nmr-params.scad), not a record's line
// centre: spectrum-data.scad changes with every measured record, the engraved default must not.
// x 10..60 = 1.8..2.3 mT; axis at KiCad y 69.5; everything above y 84, clear of the U402 pocket.
include <nmr-params.scad>
gamma_khz_mt = nmr_gamma / 1e6;      // 1H gyromagnetic ratio / 2 pi, kHz/mT
nmr_if_khz   = nmr_if / 1000;        // f_tx - f_lo of the defaults
scale_b   = [1.8, 2.3];  scale_x = [10, 60];  scale_y = 69.5;
function sx(b) = scale_x[0] + (b - scale_b[0]) / (scale_b[1] - scale_b[0]) * (scale_x[1] - scale_x[0]);
module badge2d() {
    y = scale_y;
    translate([scale_x[0] - 0.5, -y - 0.6]) square([scale_x[1] - scale_x[0] + 1, 0.6]);       // axis
    // ticks overlap the axis by 0.1 mm and the loop has no let(): the first version (ticks only
    // touching the axis, let() in the loop) previewed but rendered with no engraving in 2021.01
    for (k = [0:10]) {                                                                          // mT, up
        b = scale_b[0] + 0.05 * k;
        translate([sx(b) - 0.3, -y - 0.1]) square([0.6, k % 2 == 0 ? 4.1 : 2.1]);
        if (k % 2 == 0) translate([sx(b), -(y - 5.7)]) label(b == round(b) ? str(b, ".0") : str(b), 2.2);
    }
    for (f = [ceil(scale_b[0] * gamma_khz_mt) : floor(scale_b[1] * gamma_khz_mt)])            // kHz, down
        translate([sx(f / gamma_khz_mt) - 0.3, -y - 0.5 - (f % 5 == 0 ? 4 : 2)])
            square([0.6, f % 5 == 0 ? 4 : 2]);
    for (f = [80 : 5 : 95]) translate([sx(f / gamma_khz_mt), -(y + 6.5)]) label(str(f), 2.2);
    translate([scale_x[0], -(y - 10)]) label("B0 (mT)", 2.2, "left");
    translate([scale_x[0], -(y + 10.1)]) label("1H f (kHz) = 42.58 × B0 (mT)", 2.0, "left");
    translate([scale_x[0], -(y + 13.4)])
        label(str("DEFAULT ", nmr_f_tx / 1000, " kHz, f_LO = f − ", nmr_if_khz, " kHz"), 2.0, "left");
}

// cover: [text, x, KiCad y, size, halign]
ttl_pins = [for (i = [0:9]) 144.1 - 11.43 + 2.54 * i];          // J31 pads 1..10
cover_labels = concat(
    [for (i = [0:9]) [i < 8 ? str(i + 1) : "G", ttl_pins[i], 105.55, 2.0, "center"]],  // TTL1..8, GND
    [["TX", 108.35, 101.6, 2.0, "center"], ["G", 113.35, 101.6, 2.0, "center"],      // J32 TX, GND
     ["+", 84.35, 100.5, 2.6, "center"], ["−", 89.35, 100.5, 2.6, "center"],    // J411 ISO1
     ["+", 56.875, 100.5, 2.6, "center"], ["−", 61.875, 100.5, 2.6, "center"],  // J412 ISO2
     ["MODULE OUT", 34.75, 100.4, 2.5, "center"],                                    // J40
     ["QWIIC", 116.6, 77.75, 2.2, "right"],
     ["PWR", 113.6, 86.95, 2.0, "right"], ["WIFI", 113.6, 89.9, 2.0, "right"],       // D1..D3
     ["ACT", 113.6, 92.85, 2.0, "right"]]);
// front wall, read from the front: [text, x, z, size, halign]
front_labels = [
    ["TTL 1–8", 138.5, -9.0, 2.5, "right"],     // clear of the deep TTL notch
    ["TX COIL", 110.7, -9.0, 2.5, "center"],
    ["ISO IN 1", 88.15, -9.0, 2.5, "center"],
    ["ISO IN 2", 60.25, -9.0, 2.5, "center"],
];
// rear wall, read from behind: names over the openings, pin roles under them (housing x of
// each pad = 180 - main x, the footprints are turned 180 deg)
rear_labels = [
    ["USB-C", 162.0, -4.4, 2.5],                                                      // widths measured:
    ["5 V DC", 147.7, -4.4, 2.5],                                                     // 10.8 mm
    ["7–18 V", 130.1, -1.3, 2.0],                                                // 8.5
    ["VEXT", 130.1, -4.4, 2.2],                                                       // 7.8
    ["+", 127.625, -8.4, 2.6], ["−", 132.625, -8.4, 2.6],                        // J901 VIN, GND
    ["H-BRIDGE", 116.5, -4.4, 2.0],                                                   // 13.2
    ["1", 113.225, -8.4, 2.2], ["2", 118.225, -8.4, 2.2],                             // J903 OUT1, OUT2
    ["POLARIZER", 100.1, -4.4, 2.0],                                                  // 15.5
    ["+", 95.1, -8.4, 2.6], ["SW", 100.1, -8.4, 2.0], ["−", 105.1, -8.4, 2.6],   // J905 VCOIL, COIL, GND
];
module label(t, s, h = "center") {
    if (t == "+" || t == "−") {                    // bars, not the font's 0.4 mm thin minus
        square([2.0, 0.6], center = true);
        if (t == "+") square([0.6, 2.0], center = true);
    } else offset(delta = label_grow) text(t, size = s, font = label_font, halign = h, valign = "center");
}
module cover_label_cuts() {
    for (n = sma_names) {
        s = len([for (m = nmr_names) if (m == n[0]) 1]) > 0 ? 3.4 : 2.6;
        translate([n[1], -(n[2] + sma_label_dy), z_top - label_depth]) linear_extrude(label_depth + 1) label(n[0], s);
    }
    for (l = cover_labels)
        translate([l[1], -l[2], z_top - label_depth]) linear_extrude(label_depth + 1) label(l[0], l[3], l[4]);
    translate([title_at[0], -title_at[1], z_top - label_depth]) linear_extrude(label_depth + 1) {
        label(title[0], 4.2);
        translate([0, -7]) label(title[1], 3.0);
    }
    translate([0, 0, z_top - label_depth]) linear_extrude(label_depth + 1) badge2d();
}
module front_label_cuts() {
    for (l = front_labels)
        translate([l[1], -out_py[1] + label_depth, l[2]]) rotate([90, 0, 0])
            linear_extrude(label_depth + 1) label(l[0], l[3], l[4]);
}
module rear_label_cuts() {
    for (l = rear_labels)
        translate([l[1], -out_py[0] - label_depth, l[2]]) rotate([90, 0, 180])
            linear_extrude(label_depth + 1) label(l[0], l[3]);
}

// ---------------------------------------------------------------- plate fastening bosses
// [wall, position along it]; wall: "rear", "front", "left", "right"
bosses = [["rear", 8], ["rear", 60], ["rear", 174.5],
          ["front", 8], ["front", 90], ["front", 174.5],
          ["left", 65], ["right", 65]];
boss_w = 9;  boss_d = 8;  boss_h = 6;           // block; above it a 45 deg ramp to the wall
boss_hole_y = 4;                                // hole centre, from the wall face
pocket_z = 2;                                   // nut pocket from the rim: z 2.0 .. 4.5

module at_boss(b) {
    w = b[0]; s = b[1];
    if (w == "rear")  translate([s, -in_py[0], z_rim]) rotate(180) children();
    if (w == "front") translate([s, -in_py[1], z_rim]) children();
    if (w == "left")  translate([in_x[0], -s, z_rim]) rotate(-90) children();
    if (w == "right") translate([in_x[1], -s, z_rim]) rotate(90) children();
}
// boss in local coordinates: wall face at y = 0, interior towards +y, rim at z = 0
module boss_solid() hull() {
    translate([-boss_w/2, -0.5, 0]) cube([boss_w, boss_d + 0.5, boss_h]);
    translate([-boss_w/2, -0.5, boss_h]) cube([boss_w, 0.5, boss_d]);
}
module boss_cut() {
    translate([0, boss_hole_y, -1]) cylinder(d = m3_hole, h = boss_h + 2);
    translate([0, 0, pocket_z]) linear_extrude(nut_h)
        hull() { translate([0, boss_hole_y]) hex(nut_af); translate([0, boss_d + 2]) hex(nut_af); }
}

// ---------------------------------------------------------------- parts
module shell() {
    difference() {
        union() {
            difference() {
                rbox(out_x[0], out_x[1], out_py[0], out_py[1], z_rim, z_top, r_out);
                rbox(in_x[0], in_x[1], in_py[0], in_py[1], z_rim - 1, z_cov_u, 1);
            }
            // posts from the cover's underside onto the panel at the four stand-off holes
            for (h = holes) translate([h[0], -h[1], z_face]) cylinder(d = 7, h = cover_clr + eps);
            for (b = bosses) at_boss(b) boss_solid();
        }
        for (h = holes) translate([h[0], -h[1], z_face - 1]) cylinder(d = m3_hole, h = 20);
        for (p = sma) translate([p[0], -p[1], z_cov_u - 1]) cylinder(d = sma_d, h = cover_t + 2);
        for (w = windows) pbox(w[0], w[1], w[2], w[3], z_cov_u - 1, z_top + 1);
        for (t = edge_terms) {
            pbox(t[0], t[1], t[2], in_py[1], z_cov_u - 1, z_top + 1);
            front_v_cut(t);
        }
        for (o = rear_v) rear_v_cut(o);
        pbox(rear_open[0], rear_open[1], out_py[0] - 1, in_py[0] + 1.5, z_rim - 1, rear_open_top);
        for (b = bosses) at_boss(b) boss_cut();
        chip_pockets();
        cover_label_cuts();
        front_label_cuts();
        rear_label_cuts();
    }
}
module rear_v_cut(o) {
    x0 = o[0]; x1 = o[1]; z0 = o[2]; z1 = o[3]; xm = (x0 + x1) / 2;
    translate([0, -(out_py[0] - 1), 0]) rotate([90, 0, 0]) linear_extrude(wall + 2.5)
        polygon([[x0, z1], [x1, z1], [x1, z0], [xm, z0 - (x1 - x0) / 2], [x0, z0]]);
}

// notch through the front wall under a far-edge terminal; its lower edge is a 45 deg V too
module front_v_cut(t) {
    x0 = t[0]; x1 = t[1]; xm = (x0 + x1) / 2;
    translate([0, -(in_py[1] - 1), 0]) rotate([90, 0, 0]) linear_extrude(wall + 2)
        polygon([[x0, z_top + 1], [x1, z_top + 1], [x1, z_notch], [xm, z_notch - (x1 - x0) / 2], [x0, z_notch]]);
}

module hood() {
    difference() {
        union() {
            pbox(hood_out[0], hood_out[1], hood_out[2], hood_out[3], z_face, z_face + hood_h);
            pbox(hood_fl[0], hood_fl[1], hood_fl[2], hood_fl[3], z_face, z_face + hood_flange_t);
        }
        pbox(hood_in[0], hood_in[1], hood_in[2], hood_in[3], z_face - 1, z_face + hood_h + 1);
    }
}

// rear terminal screws face down (the parts are on the main board's front), so the plate has
// screwdriver holes under them: pins main x, KiCad y 19.7
term_screws = [47.375, 52.375, 61.775, 66.775, 74.9, 79.9, 84.9];
module plate2d() {
    difference() {
        rrect(out_x[0], out_x[1], out_py[0], out_py[1], r_out);
        for (b = bosses) projection() at_boss(b) translate([0, boss_hole_y, 0]) cylinder(d = m3_hole, h = 1);
        for (m = term_screws) translate([board_x - m, -19.7]) circle(d = 5);
    }
}

module coupon() {
    difference() {
        translate([0, -14, 0]) cube([70, 36, 2]);
        for (i = [0:6]) translate([6 + i * 7, 7, -1]) cylinder(d = 3.0 + i * 0.1, h = 4);
        // label samples on the bed face, engraved like the cover's labels (read from below)
        for (r = [[2.0, -3.5, "2.0"], [2.5, -7.5, "2.5"], [3.0, -11.5, "3.0"]])
            translate([0, r[1], -1]) linear_extrude(label_depth + 1) mirror([1, 0]) {
                translate([-3, 0]) label(str(r[2], "  TX 12345678 G"), r[0], "right");
                translate([-62, 0]) label("+", r[0]);
                translate([-66, 0]) label("−", r[0]);
            }
    }
    translate([0, 14, 0]) cube([50, wall, 12]);                       // a wall of 2 mm
    translate([58, 12, 0]) difference() {                            // one nut pocket
        translate([-5, 0, 0]) cube([10, 10, 7]);
        translate([0, 5, -1]) cylinder(d = m3_hole, h = 9);
        translate([0, 0, pocket_z + 2]) linear_extrude(nut_h)
            hull() { translate([0, 5]) hex(nut_af); translate([0, -2]) hex(nut_af); }
    }
}

// The board meshes are exports of the boards, owned by class-board-2026 (hardware/release/); they
// are not copied here. This path assumes the two repositories are cloned side by side
// (tigp-2026/nmr-instrument and tigp-2026/class-board-2026); the reference commit is in
// ../hardware/README.md. Only previews use them - no exported part depends on them.
board_dir = "../../class-board-2026/hardware/release/";
module boards() {
    color("darkgreen") import(str(board_dir, "front-panel.stl"));
    color("seagreen") translate([board_x, 0, -gap]) rotate([0, 180, 0])
        import(str(board_dir, "class-board.stl"));
    color("steelblue") pbox(dev_x[0], dev_x[1], dev_py[0], dev_py[1], z_dev_low, z_dev_low + dev_parts + pcb);
}

// ---------------------------------------------------------------- Phase 5: bench feet + cable strain relief
// The enclosure (shell + bottom plate + cover standoffs + connector cut-outs) was already complete; these are the two
// mechanical gaps (bench stability, and keeping a cable pull off the terminals/SMA). See docs/design-closure.md.
foot_d = 14;  foot_h = 5;
module housing_feet() color("dimgray")
    for (x = [out_x[0] + 14, out_x[1] - 14], y = [out_py[0] + 14, out_py[1] - 14])
        translate([x, y, z_rim - plate_t - foot_h]) cylinder(d = foot_d, h = foot_h + 0.2, $fn = 24);
// a clamp bar just outside the front wall (TX pair + screw terminals edge): cables zip-tie to it, so a tug on the
// cable reaches the bar, not J32 or the OPA564 output. Zip-tie slots along it.
module strain_relief() color("dimgray")
    translate([60, out_py[0] - foot_d, z_rim - plate_t]) difference() {
        cube([80, 7, 16]);
        for (sx = [12, 40, 68]) translate([sx, -1, 8]) cube([3.5, 9, 5]);
    }

// ---------------------------------------------------------------- output
// assembly: the boards solid, the printed parts see-through (0 = invisible, 1 = solid)
see_through = 0.5;
module housing_assembly() {
    boards();                                  // opaque first, so they show through the shell
    color("ivory", see_through) shell();
    color("orange", min(1, 2 * see_through)) hood();
    color("lightblue", see_through) translate([0, 0, z_rim - plate_t]) linear_extrude(plate_t) plate2d();
    housing_feet();
    strain_relief();
}
if (part == "assembly") housing_assembly();

// ---------------------------------------------------------------- for instrument.scad (use <housing.scad>)
// where the cables plug in, in this file's frame: [x, y, z] and the direction the cable leaves
function housing_bottom_z() = z_rim - plate_t;
function housing_size() = [out_x[1] - out_x[0], out_py[1] - out_py[0], z_top - z_rim + plate_t];
function housing_port_tx() = [[110.7, -out_py[1], z_face + 4], [0, -1, 0]];       // TX COIL J32, front notch
function housing_port_rx() = [[147.65, -43.67, z_top + 8], [0, 0, 1]];            // RX SMA, on the cover
function housing_port_hb() = [[115.7, -out_py[0], -16], [0, 1, 0]];               // H-BRIDGE J903, rear opening
// M3 steel screws (x, y): the four stand-off screws, and the plate screws with their nuts in the bosses
// (the hole is boss_hole_y in from the wall's inner face)
function boss_xy(b) = b[0] == "rear"  ? [b[1], -(in_py[0] + boss_hole_y)]
                    : b[0] == "front" ? [b[1], -(in_py[1] - boss_hole_y)]
                    : b[0] == "left"  ? [in_x[0] + boss_hole_y, -b[1]]
                    :                   [in_x[1] - boss_hole_y, -b[1]];
function housing_steel() = concat([[4, -19], [176, -19], [4, -111], [176, -111]], [for (b = bosses) boss_xy(b)]);
if (part == "shell")   translate([0, 0, z_top]) rotate([180, 0, 0]) shell();
if (part == "shell_in_place") shell();   // board frame, for collision checks
if (part == "hood")    translate([0, 0, -z_face]) hood();
if (part == "hood_in_place") hood();
if (part == "plate")   plate2d();
if (part == "plate3d") linear_extrude(plate_t) plate2d();
if (part == "coupon")  coupon();

// ---------------------------------------------------------------- numbers to check
echo(str("outer size ", out_x[1] - out_x[0], " x ", out_py[1] - out_py[0], " x ",
         z_top - z_rim + plate_t, " mm (shell ", z_top - z_rim, " + plate ", plate_t, ")"));
echo(str("z: cover top ", z_top, ", cover underside ", z_cov_u, ", panel face ", z_face,
         ", main board front ", z_main_f, ", dev board lowest ", z_dev_low, ", rim/plate top ", z_rim));
echo(str("SMA thread above the cover: ", 9.8 - (z_top - z_face), " mm (hex base 2.0, cover underside ",
         cover_clr, " above the face)"));
echo(str("OLED hood top ", hood_h, " above the face, ", z_face + hood_h - z_top, " above the cover"));
echo(str("deepest rear part (DC jack) ", z_main_f - 10.8, ", clearance to the plate ", z_main_f - 10.8 - z_rim));
echo(str("stand-off screw from the top: cover ", cover_t, " + post ", cover_clr, " + panel ", pcb,
         " = ", cover_t + cover_clr + pcb, " -> M3x12 (5.4 into the 11 mm stand-off); from below M3x6 through the main board"));
echo(str("plate screws: plate ", plate_t, " + nut pocket top ", pocket_z + nut_h, " -> M3x8, ",
         len(bosses), " pcs"));
echo(str("fits 256 bed: ", out_x[1] - out_x[0] <= 256 && out_py[1] - out_py[0] <= 256));
echo(str("chips under the cover: tallest unpocketed part 1.32 mm, ", cover_clr - 1.32, " mm below the cover; ",
         "optocoupler pockets leave ", z_top - (z_face + chips[0][4] + chip_clr), " mm of cover over them"));
