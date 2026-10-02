// The probe, its cradle and the B0 coil pair as flat parts, the way the article builds its coil:
// a bought plastic tube with laser-cut end pieces (two 3 mm layers glued = 6 mm). Every number
// comes from probe.scad, so the flat parts and the 3D model cannot disagree.
//
// piece = one part (export it with layer = "cut" / "engrave"), or "sheets" to see them laid out.
// export-cut.sh writes every piece; CUT-LIST.md says how many of each, from what, on which machine.
//
// Bought, not cut: the former tube (40 x 2 mm acrylic or PVC, 125 mm long; the article's is 110),
// the sleeve tube (35 x 2 mm acrylic, 125 mm), nylon M3 / M4 / M6 screws and threaded rod, a brass
// 1/4"-20 nut. No steel anywhere near the coil.
include <probe.scad>
include <cut-common.scad>
part  = "none";                // probe.scad's own output off
piece = "sheets";

b0_process = "jet";            // "jet": whole rings, 6 mm polycarbonate, water-jet service
                               // "laser": rings in 4 segments, 6 mm acrylic, brick-bonded

// ---------------------------------------------------------------- probe: cheeks and sleeve flange (laser, 3 mm)
tube_od  = wind_d;             // the bought former tube
l_cheek  = 50;                 // laser cheeks are larger than the printed 46: room round the lead holes
l_lead_r = tube_od / 2 + 1.8;  // lead holes just outside the tube and the 0.9 mm winding
sl_od    = 35;                 // the bought sleeve tube; its ID 31 takes the 30 mm sample tube

module cheek2d(lead) as_cut() difference() {
    circle(d = l_cheek, $fn = 120);
    circle(d = tube_od + cut_fit, $fn = 120);
    if (lead) for (a = [0, 20]) rotate(a) translate([l_lead_r, 0]) circle(d = lead_d, $fn = 16);
}
// on the band between the tube and the rim, top and bottom (the lead holes are at 0 and 20 deg)
module cheek_text(lead) as_engrave() for (a = [0, 180])
    rotate(a) translate([0, (tube_od / 2 + l_cheek / 2) / 2])
        text(a == 0 ? (lead ? "LEADS" : "SAMPLE") : "400T AWG26", size = 1.8, font = label_font,
             halign = "center", valign = "center");
module sleeve_flange2d() as_cut() difference() {
    circle(d = l_cheek, $fn = 120);
    circle(d = sl_od + cut_fit, $fn = 120);
    translate([sl_od / 2, -3]) square([l_cheek, 6]);                    // fingernail notch
}

// ---------------------------------------------------------------- probe: cradle (laser, 6 mm)
// A base with two saddles and a cable anchor standing in slots, glued; a nut plate under the base
// holds the brass tripod nut. Saddles sit under the cheek pairs, as in probe.scad.
l_arc   = l_cheek / 2 + cut_fit;
l_under = 8;                              // saddle material under the cheek
l_horn  = 8;                              // saddle beside the cheek (carries the zip-tie slot)
sad_w   = 2 * (l_arc + l_horn);
sad_h   = l_under + l_arc;                // base top to the coil axis
sad_tab = 12;
l_axis  = t6 + sad_h;                     // coil axis above the cradle's underside
anc     = [22, 16];                       // anchor plate: width (y), height above the base
anc_x   = base_x[1] - 8;
l_base_w = max(base_w, sad_w + 4);           // the saddles are wider than the printed base
l_base  = [base_x[1] - base_x[0], l_base_w];

module saddle2d() as_cut() difference() {
    union() {
        translate([-sad_w / 2, 0]) square([sad_w, sad_h]);
        for (u = [-1, 1]) translate([u * sad_w / 4 - sad_tab / 2, -t6]) square([sad_tab, t6 + 0.01]);
    }
    translate([0, sad_h]) circle(r = l_arc, $fn = 120);
    for (u = [-1, 1]) translate([u * (l_arc + l_horn / 2) - 1.25, sad_h - 14]) square([2.5, 6]);   // zip tie
}
module anchor2d() as_cut() difference() {
    union() {
        translate([-anc[0] / 2, 0]) square([anc[0], anc[1]]);
        translate([-5, -t6]) square([10, t6 + 0.01]);
    }
    translate([0, anc[1]]) circle(d = 6, $fn = 32);                     // the cable lies in this
    for (u = [-1, 1]) translate([u * 7 - 1.25, anc[1] - 9]) square([2.5, 5]);                    // zip tie
}
// base, in the assembly frame (x along the coil axis, y across)
module cradle_base2d() {
    as_cut() difference() {
        translate([base_x[0], -l_base_w / 2]) square(l_base);
        for (x = saddle_x, u = [-1, 1]) translate([x, u * sad_w / 4]) rotate(90) slots([0], sad_tab, t6);
        translate([anc_x, 0]) rotate(90) slots([0], 10, t6);
        for (sx = [0, 1], sy = [-1, 1])
            translate([sx == 0 ? base_x[0] + 3.5 : base_x[1] - 3.5, sy * (l_base_w / 2 - 6)]) circle(d = mount_d, $fn = 32);
        circle(d = tri_hole, $fn = 32);
    }
    as_engrave() for (s = [-1, 1]) translate([0, s * (l_base_w / 2 - 4)]) rotate(s < 0 ? 0 : 180) {
        square([0.8, 5], center = true);
        translate([-3, 0]) text("AXIS 90° TO B0", size = 3, font = label_font, halign = "right", valign = "center");
        translate([3, 0])  text("EARTH FIELD: E–W", size = 3, font = label_font, halign = "left", valign = "center");
    }
}
module nutplate2d() as_cut() difference() {
    square([30, 30], center = true);
    rotate(30) circle(d = (tri_af - fit + cut_fit) / cos(30), $fn = 6);   // 7/16" across flats
}

// ---------------------------------------------------------------- B0 pair (water-jet or laser, 6 mm)
// Each coil: a stack of spacer rings (the winding surface) between two flanges, clamped by nylon
// threaded rods; the stack sits in notches of two rails that fix the Helmholtz spacing (= R).
jet     = b0_process == "jet";
b_kerf  = jet ? jet_kerf : laser_kerf;
segs    = jet ? 1 : 4;
band    = 15;                                   // spacer ring width under the winding
r_w     = hh_R - hh_h / 2;                      // winding surface radius
r_fl    = hh_R + hh_h / 2 + hh_rim;             // flange outer radius
r_rod   = r_w - band / 2;
rod_d   = jet ? 6.4 : 4.3;                      // nylon M6 (water-jet minimum hole) or M4
n_sp    = ceil(hh_w / t6);                      // spacers per coil
stack   = (n_sp + 2) * t6;                      // flange + spacers + flange
rail_x  = 100;                                  // rails at x = +-rail_x, along y
z_cross = sqrt(r_fl * r_fl - rail_x * rail_x);  // ring's lowest point in a rail's plane
rail_v  = r_fl + 10 - z_cross;                  // notch bottom above the table (rings 10 mm clear)
rail_h  = rail_v + 20;
rail_l  = hh_R + stack + 120;
brace_l = 2 * rail_x + 40;

module rods() for (k = [0:7]) rotate(22.5 + 45 * k) translate([r_rod, 0]) circle(d = rod_d, $fn = 32);
// one segment (or the whole ring with segs = 1); joints at 0/90/180/270, rods between them
module b0_flange2d(lead) as_cut(b_kerf) difference() {
    ring2d(r_w - band, r_fl, 0, 360 / segs);
    rods();
    if (lead) rotate(10) translate([r_w + 3, 0]) circle(d = jet ? jet_min_hole : 2.5, $fn = 32);   // start lead
}
module b0_spacer2d() as_cut(b_kerf) difference() { ring2d(r_w - band, r_w, 0, 360 / segs); rods(); }
module b0_rail2d() as_cut(b_kerf) difference() {
    square([rail_l, rail_h]);
    for (s = [-1, 1]) translate([rail_l / 2 + s * hh_R / 2 - (stack + 1) / 2, rail_v]) square([stack + 1, 30]);
    for (y = [20, rail_l - 20]) translate([y - (t6 + cut_fit) / 2, -1]) square([t6 + cut_fit, rail_v / 2 + 1]);
}
module b0_brace2d() as_cut(b_kerf) difference() {
    square([brace_l, rail_v]);
    for (x = [brace_l / 2 - rail_x, brace_l / 2 + rail_x]) translate([x - (t6 + cut_fit) / 2, rail_v / 2]) square([t6 + cut_fit, rail_v]);
}
module b0_text() as_engrave() if (!jet) translate([0, r_w - band / 2 + 1])
    text(str("B0 ", round(nmr_B0 * 1e4) / 10, " mT  ", hh_N, " T"), size = 4, font = label_font, halign = "center", valign = "center");

// ---------------------------------------------------------------- pieces
// [name, material, machine, qty]
pieces = [
    ["cheek",          "3 mm acrylic", "laser", 2],
    ["cheek_lead",     "3 mm acrylic", "laser", 2],
    ["sleeve_flange",  "3 mm acrylic", "laser", 1],
    ["cradle_base",    "6 mm acrylic", "laser", 1],
    ["cradle_saddle",  "6 mm acrylic", "laser", 2],
    ["cradle_anchor",  "6 mm acrylic", "laser", 1],
    ["cradle_nutplate","6 mm acrylic", "laser", 1],
    ["b0_flange",      jet ? "6 mm polycarbonate" : "6 mm acrylic", jet ? "water-jet" : "laser", 3 * segs],
    ["b0_flange_lead", jet ? "6 mm polycarbonate" : "6 mm acrylic", jet ? "water-jet" : "laser", 1 * segs],
    ["b0_spacer",      jet ? "6 mm polycarbonate" : "6 mm acrylic", jet ? "water-jet" : "laser", 2 * n_sp * segs],
    ["b0_rail",        "6 mm acrylic", "laser", 2],
    ["b0_brace",       "6 mm acrylic", "laser", 2],
];
// for export_cut.py: one line per piece
if (piece == "list") for (p = pieces) echo(str("PIECE|", p[0], "|", p[1], "|", p[2], "|", p[3]));
module one(p) {
    if (p == "cheek")          { cheek2d(false); cheek_text(false); }
    if (p == "cheek_lead")     { cheek2d(true);  cheek_text(true); }
    if (p == "sleeve_flange")  sleeve_flange2d();
    if (p == "cradle_base")    cradle_base2d();
    if (p == "cradle_saddle")  saddle2d();
    if (p == "cradle_anchor")  anchor2d();
    if (p == "cradle_nutplate") nutplate2d();
    if (p == "b0_flange")      { b0_flange2d(false); b0_text(); }
    if (p == "b0_flange_lead") b0_flange2d(true);
    if (p == "b0_spacer")      b0_spacer2d();
    if (p == "b0_rail")        b0_rail2d();
    if (p == "b0_brace")       b0_brace2d();
}
if (piece != "sheets" && piece != "list") one(piece);

// laid out on the laser bed (3 mm and 6 mm sheets) and, for the water-jet, the ring parts
if (piece == "sheets") {
    sheet(laser_bed);
    for (i = [0:3]) translate([35 + 55 * i, 35]) one(i < 2 ? "cheek" : "cheek_lead");
    translate([35 + 55 * 4, 35]) one("sleeve_flange");
    translate([0, 420]) {
        sheet(laser_bed);
        translate([-base_x[0] + 10, l_base_w / 2 + 10]) one("cradle_base");
        for (i = [0:1]) translate([45 + 75 * i, l_base_w + 30]) one("cradle_saddle");
        translate([30, l_base_w + 90]) one("cradle_anchor");
        translate([80, l_base_w + 100]) one("cradle_nutplate");
        for (i = [0:1]) translate([200, 10 + (rail_h + 8) * i]) one("b0_rail");
        for (i = [0:1]) translate([200, 2 * (rail_h + 8) + 10 + (rail_v + 8) * i]) one("b0_brace");
    }
    translate([700, 0]) {
        sheet(jet ? [2 * r_fl + 20, 2 * r_fl + 20] : laser_bed);
        translate(jet ? [r_fl + 10, r_fl + 10] : [300, -r_w + band + 20]) one("b0_flange");
    }
}

// ---------------------------------------------------------------- checks
echo(str("cheeks: ", l_cheek, " mm, bore ", tube_od + cut_fit, ", lead holes at r ", l_lead_r, " (", l_cheek / 2 - l_lead_r - lead_d / 2,
         " mm of acrylic outside them); glue 2 + 2 on the tube at 0..6 and ", z_cheek2, "..", z_cheek2 + cheek_t, " mm"));
echo(str("cradle: base ", l_base, " x 6, saddles ", sad_w, " x ", sad_h, " mm, coil axis ", l_axis, " mm above its underside"));
echo(str("B0 coil: ", n_sp, " spacers + 2 flanges = ", stack, " mm stack, channel ", n_sp * t6, " x ", r_fl - hh_rim - r_w,
         " mm for ", hh_w, " x ", hh_h, "; rings r ", r_w - band, "..", r_fl, " mm, ", segs, jet ? " piece (water-jet)" : " segments (laser)",
         ", segment fits the laser bed: ", jet || (r_fl - (r_w - band) * cos(45) < laser_bed[1] && 2 * r_fl * sin(45) < laser_bed[0])));
echo(str("B0 rails: ", rail_l, " x ", rail_h, " mm, notches ", hh_R, " mm apart (Helmholtz spacing = R), rings ", 10, " mm off the table"));
if (jet && rod_d < jet_min_hole) echo("WARNING: a hole below the water-jet minimum");
