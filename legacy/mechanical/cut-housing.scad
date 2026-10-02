// The housing as a laser-cut box in 3 mm acrylic: a cover with the walls glued into it by tabs
// (the printed shell's job), and the bottom plate screwed on with M3 nuts in T-slots (the printed
// version's nut pockets). Every hole, window, notch, opening and label is housing.scad's own list,
// so the printed and the cut housing fit the same boards.
//
// What changes against the printed shell: the walls are 3 mm, not 2 (the box is 1 mm larger
// each side, the board gap stays 0.5 mm); the cover is 3 mm, not 2 (the SMA thread above it is
// 3.8 mm, enough for the nut and washer); the optocoupler pockets are engraved into the cover's
// underside (engrave_back, 1.2 mm deep); the OLED hood is three glued frames on the cover; the
// stand-off posts are four 3 mm acrylic washers.
//
// piece = one part, or "sheets"; layer as in cut-common.scad. Assembly: cover face down, glue the
// walls' tabs into its slots (solvent cement), sides first; washers on the stand-offs, shell on,
// M3x12 from the top; plate on with M3x10 into the T-slot nuts.
include <housing.scad>
include <cut-common.scad>
part  = "none";                // housing.scad's own output off
piece = "sheets";

t   = t3;
bx  = [in_x[0] - t, in_x[1] + t];            // box outside, x
by  = [in_py[0] - t, in_py[1] + t];          // box outside, KiCad-style y (positive)
zc  = z_cov_u;                               // cover underside = wall tops (as in the printed shell)
zt  = zc + t;                                // cover top
hw  = zc - z_rim;                            // wall height
corner_v = [z_rim + 0.3 * hw, z_rim + 0.75 * hw];   // side-to-front/rear joints (tab centres, z)
corner_h = 10;

// tabs on the wall tops into the cover: [x or py, width]; clear of the front notches
tabs_front = [[20, 15], [74, 8], [170, 15]];
tabs_rear  = [[20, 15], [90, 15], [170, 15]];
tabs_side  = [[40, 15], [90, 15]];
// T-slots in the wall bottoms for the plate screws; the rear avoids the terminal opening
ts_front = [30, 100, 160];
ts_rear  = [30, 70, 160];
ts_side  = [65];

// ---------------------------------------------------------------- cover (in housing x, -py)
module cover_cut() difference() {
    translate([bx[0], -by[1]]) square([bx[1] - bx[0], by[1] - by[0]]);
    for (h = holes) translate([h[0], -h[1]]) circle(d = m3_hole, $fn = 32);
    for (p = sma) translate([p[0], -p[1]]) circle(d = sma_d, $fn = 48);
    translate([hood_in[0], -hood_in[3]]) square([hood_in[1] - hood_in[0], hood_in[3] - hood_in[2]]);  // OLED
    for (w = [for (i = [1:len(windows) - 1]) windows[i]]) translate([w[0], -w[3]]) square([w[1] - w[0], w[3] - w[2]]);
    for (e = edge_terms) translate([e[0], -by[1] - 1]) square([e[1] - e[0], by[1] + 1 - e[2]]);       // open to the front edge
    // slots for the wall tabs, on each wall's mid-plane
    for (a = tabs_front) translate([a[0], -(by[1] - t / 2)]) slots([0], a[1], t);
    for (a = tabs_rear)  translate([a[0], -(by[0] + t / 2)]) slots([0], a[1], t);
    for (a = tabs_side, x = [bx[0] + t / 2, bx[1] - t / 2]) translate([x, -a[0]]) rotate(90) slots([0], a[1], t);
}
module cover_engrave() {
    for (n = sma_names) translate([n[1], -(n[2] + sma_label_dy)])
        label(n[0], len([for (m = nmr_names) if (m == n[0]) 1]) > 0 ? 3.4 : 2.6);
    for (l = cover_labels) translate([l[1], -l[2]]) label(l[0], l[3], l[4]);
    translate([title_at[0], -title_at[1]]) { label(title[0], 4.2); translate([0, -7]) label(title[1], 3.0); }
    badge2d();
}
// optocouplers stand 3.55 mm, the cover's underside 3.0 above the face: 1.05 + 0.5 clearance -> 1.2 deep
module cover_back() for (c = chips) translate([c[0] - chip_clr, -(c[3] + chip_clr)])
    square([c[1] - c[0] + 2 * chip_clr, c[3] - c[2] + 2 * chip_clr]);

// ---------------------------------------------------------------- walls (u along the wall, v = z, seen from outside)
module wall_base(u0, u1, tb, ts) difference() {
    union() {
        translate([u0, z_rim]) square([u1 - u0, hw]);
        for (a = tb) translate([a[0], zc - 0.01]) tabs([0], a[1], t + 0.01);
    }
    for (u = ts) translate([u, z_rim]) tslot();
}
module corner_slots(us) for (u = us, v = corner_v) translate([u - (t + cut_fit) / 2, v - (corner_h + cut_fit) / 2]) square([t + cut_fit, corner_h + cut_fit]);

module front_cut() difference() {                       // u = x, seen from the front
    wall_base(bx[0], bx[1], tabs_front, ts_front);
    for (e = edge_terms) translate([e[0], z_notch]) square([e[1] - e[0], zt]);         // wire entries, open to the top
    corner_slots([bx[0] + t / 2, bx[1] - t / 2]);
}
module front_engrave() for (l = front_labels) translate([l[1], l[2]]) label(l[0], l[3], l[4]);

module rear_cut() mirror([1, 0]) difference() {        // drawn in x, mirrored: seen from behind
    wall_base(bx[0], bx[1], tabs_rear, ts_rear);
    for (o = rear_v) translate([o[0], o[2]]) square([o[1] - o[0], o[3] - o[2]]);
    translate([rear_open[0], z_rim - 1]) square([rear_open[1] - rear_open[0], rear_open_top - z_rim + 1]);
    corner_slots([bx[0] + t / 2, bx[1] - t / 2]);
}
module rear_engrave() for (l = rear_labels) translate([-l[1], l[2]]) label(l[0], l[3]);

module side_cut() difference() {                        // u = py, between the front and rear walls
    union() {
        wall_base(in_py[0], in_py[1], tabs_side, ts_side);
        for (u = [in_py[0] - t, in_py[1]], v = corner_v) translate([u - 0.01, v - corner_h / 2]) square([t + 0.02, corner_h]);
    }
}

// ---------------------------------------------------------------- small parts and the plate
module hood_frame() difference() {
    translate([hood_out[0], -hood_out[3]]) square([hood_out[1] - hood_out[0], hood_out[3] - hood_out[2]]);
    translate([hood_in[0], -hood_in[3]]) square([hood_in[1] - hood_in[0], hood_in[3] - hood_in[2]]);
}
module washer() difference() { circle(d = 7, $fn = 32); circle(d = m3_hole, $fn = 32); }
module plate_cut() difference() {
    translate([bx[0], -by[1]]) square([bx[1] - bx[0], by[1] - by[0]]);
    for (x = ts_front) translate([x, -(by[1] - t / 2)]) circle(d = m3_hole, $fn = 32);
    for (x = ts_rear)  translate([x, -(by[0] + t / 2)]) circle(d = m3_hole, $fn = 32);
    for (p = ts_side, x = [bx[0] + t / 2, bx[1] - t / 2]) translate([x, -p]) circle(d = m3_hole, $fn = 32);
    for (m = term_screws) translate([board_x - m, -19.7]) circle(d = 5, $fn = 32);      // screwdriver holes
}

// ---------------------------------------------------------------- pieces
// [name, material, machine, qty]
pieces = [
    ["cover",      "3 mm acrylic", "laser", 1],
    ["front",      "3 mm acrylic", "laser", 1],
    ["rear",       "3 mm acrylic", "laser", 1],
    ["side",       "3 mm acrylic", "laser", 2],
    ["plate",      "3 mm acrylic", "laser", 1],
    ["hood_frame", "3 mm acrylic", "laser", 3],
    ["washer",     "3 mm acrylic", "laser", 4],
];
// for export_cut.py: one line per piece
if (piece == "list") for (p = pieces) echo(str("PIECE|", p[0], "|", p[1], "|", p[2], "|", p[3]));
module one(p) {
    if (p == "cover") { as_cut() cover_cut(); as_engrave() cover_engrave(); as_engrave_back() cover_back(); }
    if (p == "front") { as_cut() front_cut(); as_engrave() front_engrave(); }
    if (p == "rear")  { as_cut() rear_cut();  as_engrave() rear_engrave(); }
    if (p == "side")  as_cut() side_cut();
    if (p == "plate") as_cut() plate_cut();
    if (p == "hood_frame") as_cut() hood_frame();
    if (p == "washer") as_cut() washer();
}
if (piece != "sheets" && piece != "box" && piece != "list") one(piece);

// the flat parts folded up round the boards, to check every tab, notch and opening in 3D
module box() {
    color("lightblue", 0.5) {
        translate([0, 0, zc]) linear_extrude(t) cover_cut();
        translate([0, -(by[1] - t), 0]) rotate([90, 0, 0]) linear_extrude(t) front_cut();
        translate([0, -by[0], 0]) rotate([90, 0, 0]) linear_extrude(t) mirror([1, 0]) rear_cut();
        for (x0 = [bx[0], bx[1] - t]) multmatrix([[0, 0, 1, x0], [-1, 0, 0, 0], [0, 1, 0, 0]]) linear_extrude(t) side_cut();
        translate([0, 0, z_rim - t]) linear_extrude(t) plate_cut();
        for (i = [0:2]) translate([0, 0, zt + i * t]) linear_extrude(t) hood_frame();
        for (h = holes) translate([h[0], -h[1], z_face]) linear_extrude(t) washer();
    }
}
if (piece == "box") { boards(); box(); }

// one 600 x 400 sheet of 3 mm acrylic
if (piece == "sheets") {
    sheet(laser_bed);
    translate([10 - bx[0], 10 + by[1]]) one("cover");
    translate([207 - bx[0], 10 + by[1]]) one("plate");
    translate([10 - bx[0], 140 - z_rim]) one("front");
    translate([207 + bx[1], 140 - z_rim]) one("rear");
    for (i = [0:1]) translate([410 - in_py[0] + t + 5 + 0 * i, 10 - z_rim + 60 * i]) one("side");
    for (i = [0:2]) translate([410 - hood_out[0] + 0 * i, 140 + hood_out[3] + 40 * i]) one("hood_frame");
    for (i = [0:3]) translate([20 + 12 * i, 210]) one("washer");
}

// ---------------------------------------------------------------- checks
echo(str("box ", bx[1] - bx[0], " x ", by[1] - by[0], " x ", zt - z_rim + plate_t, " mm; walls ", hw, " mm + ", t, " mm tabs; ",
         "cover top ", zt, " -> SMA thread above it ", 9.8 - (zt - z_face), " mm"));
echo(str("board to wall: x ", -bx[0] - t, " / ", bx[1] - t - board_x, ", y ", board_py0 - by[0] - t, " / ", by[1] - t - board_py1, " mm"));
echo(str("OLED hood: 3 frames on the cover, top ", zt + 3 * t, " vs module top ", z_face + 13.82));
echo(str("screws: M3x12 from the top (cover 3 + washer 3 + panel 1.6 = 7.6, 4.4 into the stand-off); M3x10 + nut, ",
         len(ts_front) + len(ts_rear) + 2 * len(ts_side), " pcs, for the plate"));
echo(str("sheet: everything on one ", laser_bed[0], " x ", laser_bed[1], " sheet of 3 mm acrylic"));
