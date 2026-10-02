// Shared by the cut-*.scad files: the machines, the cut width (kerf), what each file exports, and
// the joints every flat part uses. Change the machine or the sheet here, not in a part.
//
// layer = "cut"          the outlines to cut through, kerf-compensated (export DXF/SVG)
//         "engrave"      raster-engrave on the face that is up when cutting (labels, scales)
//         "engrave_back" raster-engrave on the other face: cut "cut", flip, align, engrave this
//         "preview"      everything, coloured, with the sheet outline (look, do not export)
// export-cut.sh runs every part through every layer it has.

layer = "preview";

// ---------------------------------------------------------------- machines (defaults; check yours)
// CO2 laser: 600 x 400 bed, 0.15 mm kerf in 3-6 mm acrylic. Never PVC (chlorine gas) or
// polycarbonate (chars, burns) or ABS (most makerspaces ban it) in a laser.
// Water-jet: a cutting service; 0.9 mm kerf, slight taper, no engraving, no hole smaller than
// about the sheet thickness, small parts need tabs - send one DXF per part, in mm.
laser_bed    = [600, 400];
laser_kerf   = 0.15;
jet_bed      = [1500, 3000];
jet_kerf     = 0.9;
jet_min_hole = 6;              // for 6 mm polycarbonate

// ---------------------------------------------------------------- sheet stock
t3 = 3;                        // acrylic (PMMA), laser
t6 = 6;                        // acrylic (PMMA), laser; or polycarbonate / HDPE for the water-jet
cut_fit = 0.2;                 // slot = tab + this (after kerf compensation)

// ---------------------------------------------------------------- layer plumbing
// kerf compensation: the beam removes kerf/2 either side of the line, so offsetting the whole
// part by +kerf/2 makes outlines larger and holes smaller by exactly what the cut takes away
module kerfed(k) offset(delta = k / 2) children();
module as_cut(k = laser_kerf) {
    if (layer == "cut") kerfed(k) children();
    if (layer == "preview") color("lightsteelblue") linear_extrude(1) children();
}
module as_engrave() {
    if (layer == "engrave") children();
    if (layer == "preview") color("darkred") translate([0, 0, 1]) linear_extrude(0.3) children();
}
module as_engrave_back() {
    if (layer == "engrave_back") mirror([1, 0]) children();     // seen from the back, as the laser sees it after the flip
    if (layer == "preview") color("orange") translate([0, 0, -0.3]) linear_extrude(0.3) children();
}
module sheet(size) if (layer == "preview") %translate([0, 0, -2]) cube([size[0], size[1], 1]);

// ---------------------------------------------------------------- joints
// tabs along the x axis from x0, height t, centred on the given positions
module tabs(xs, w, t) for (x = xs) translate([x - w / 2, 0]) square([w, t]);
// the matching slots, a hair bigger
module slots(xs, w, t) for (x = xs) translate([x - (w + cut_fit) / 2, -(t + cut_fit) / 2]) square([w + cut_fit, t + cut_fit]);
// M3 T-slot from an edge (the edge is y = 0, the slot goes in along +y): screw shank, then a
// cross slot for the nut, which lies flat in the sheet's plane
m3_shank = 3.2;  m3_nut_af = 5.6;  m3_nut_t = 2.5;  tslot_len = 10;  tslot_nut_at = 5;
module tslot() {
    translate([-m3_shank / 2, -1]) square([m3_shank, tslot_len + 1]);
    translate([-m3_nut_af / 2, tslot_nut_at]) square([m3_nut_af, m3_nut_t]);
}
// a ring, optionally only one segment of it (a, a + span degrees)
module ring2d(r0, r1, a = 0, span = 360, fn = 180) {
    if (span >= 360) difference() { circle(r = r1, $fn = fn); circle(r = r0, $fn = fn); }
    else intersection() {
        difference() { circle(r = r1, $fn = fn); circle(r = r0, $fn = fn); }
        polygon(concat([[0, 0]], [for (k = [0:16]) 2 * r1 * [cos(a + span * k / 16), sin(a + span * k / 16)]]));
    }
}
