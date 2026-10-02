// The spectrum model (spectrum.scad, data from spectrum.py) cut from sheet, in two styles:
//
//   style = "fins" (default)  one fin per row (one spectrum per B0 current, or per time window),
//       standing in slots in a base plate that carries the axes - a stacked (waterfall) plot. Every
//       fin is a whole spectrum, so nothing is thinner than the sheet; it works on the laser and the
//       water-jet. Laser: the fin's row value and a half-height mark (its FWHM) engraved on it, an
//       answer card. Decay data adds a gauge piece with the 1, 1/e, 1/e^2 heights (look from the left).
//       Water-jet (spec_process = "jet", 3 mm aluminium): it cannot engrave, so the axes are V-notches
//       in the plate's edges (0 Hz deeper) and the gauge heights notches in its edges; convex corners
//       rounded r = 1 (the jet's own inside radius is ~0.45), relief at the tab roots.
//   style = "contours"  the height map sliced into 3 mm layers, glued in a stack like a topographic
//       map; layer 0 is the base with the axes engraved, every layer carries the outline of the one
//       above, engraved, so the stack glues up in register. Laser only; the top layers can be slivers
//       (see the warning).
//
// piece = "list" (for export_cut.py), "sheets" (the layout, layer = "preview"), "assembly" (3D, fins),
// or one piece: "fin" / "layer" with idx, "plate", "gauge", "card". Machine and sheet: cut-common.scad.
include <spectrum.scad>
include <cut-common.scad>
part  = "none";                // spectrum.scad's own output off
piece = "sheets";
idx   = 0;
style = "fins";
spec_process = "laser";        // "laser" (3 mm acrylic) or "jet" (3 mm aluminium, water-jet service)

jet    = spec_process == "jet";
kerf   = jet ? jet_kerf : laser_kerf;
st     = t3;                   // sheet: fin thickness, slot width
machine = jet ? "water-jet" : "laser";
stock  = jet ? "3 mm aluminium" : "3 mm acrylic";

// ================================================================ fins
fin_n    = min(8, nr);                          // fins, spread evenly over the rows
fin_base = 6;                                   // solid strip under the data (and the fin's label)
tab_w = 12;  tab_x = [plot_x * 0.2, plot_x * 0.8];   // fin tabs, through the plate
slot_w = st + cut_fit;
plate_x = [-rim_left, plot_x + (has_gauge ? 9 : 4)];
plate_y = [-rim_front, plot_y + 8];
gauge_cx = plot_x + 4.5;                        // gauge piece centre line, x
gauge_h  = fin_base + plot_h + 2;
gauge_tabs = [plot_y * 0.25, plot_y * 0.75];
fin_rows = [for (k = [0:fin_n-1]) round(k * (nr - 1) / (fin_n - 1))];
function fin_top(z) = fin_base + plot_h * z;

// one fin, in its own plane: x along frequency, y up; y = 0 is the plate's top, tabs below it
module fin_shape(i) difference() {
    union() {
        polygon(concat([[0, 0]], [for (j = [0:nc-1]) [j * dx, fin_top(spec_z[i][j])]], [[plot_x, 0]]));
        for (x = tab_x) translate([x - tab_w / 2, -st]) square([tab_w, st + 0.01]);
    }
    // relief at the tab roots: the jet leaves a round inside corner that would hold the fin up
    if (jet) for (x = tab_x, s = [-1, 1]) translate([x + s * tab_w / 2 - 0.6, -0.01]) square([1.2, 0.8]);
}
module fin_engrave(i) {
    translate([3, fin_base / 2]) txt(str(r(row_val(i), 3), " ", spec_row_unit), 3.5, "left");
    // half-height mark, cut to the fin's own outline: its length is this fin's FWHM - only over the
    // peak's own columns (on a weak fin, noise bumps reach the same band)
    h = fin_top(row_max(i) / 2);
    above = [for (j = [0:nc-1]) if (spec_z[i][j] >= row_max(i) / 2) j];
    x0 = (min(above) - 1) * dx;  x1 = (max(above) + 1) * dx;
    if (len(above) <= nc / 4)
        intersection() { fin_shape(i); translate([x0, h - 0.3]) square([x1 - x0, 0.6]); }
}

module vnotch(d) polygon([[-d / 2, 0.01], [0, -d], [d / 2, 0.01]]);   // V into an edge, 45 deg
// the base plate, data frame (x 0..plot_x = frequency, y 0..plot_y = rows)
module plate_shape() difference() {
    translate([plate_x[0] + 2, plate_y[0] + 2]) offset(r = 2)            // rounded corners
        square([plate_x[1] - plate_x[0] - 4, plate_y[1] - plate_y[0] - 4]);
    for (i = fin_rows, x = tab_x) translate([x - tab_w / 2 - 0.2, i * dy - slot_w / 2]) square([tab_w + 0.4, slot_w]);
    if (has_gauge) for (y = gauge_tabs) translate([gauge_cx - slot_w / 2, y - tab_w / 2 - 0.2]) square([slot_w, tab_w + 0.4]);
    if (jet) {      // the axes as notches: frequency in the front edge (0 Hz deeper), rows in the left edge
        for (f = ticks(spec_f[0], spec_f[1], tick_f)) translate([fx(f), plate_y[0]]) rotate(180) vnotch(f == 0 ? 6 : 3.5);
        for (t = ticks(spec_row[0], spec_row[1], spec_row_tick)) translate([plate_x[0], ty(t)]) rotate(90) vnotch(3.5);
    }
}
// the gauge (decay data only): x along the row axis (0..plot_y), y up from the plate's top
module gauge_shape() difference() {
    union() {
        square([plot_y, gauge_h]);
        for (y = gauge_tabs) translate([y - tab_w / 2, -st]) square([tab_w, st + 0.01]);
    }
    if (jet) for (l = spec_gauge, e = [0, plot_y]) translate([e, fin_top(l[0])]) rotate(e == 0 ? -90 : 90) vnotch(3);
    if (jet) for (y = gauge_tabs, s = [-1, 1]) translate([y + s * tab_w / 2 - 0.6, -0.01]) square([1.2, 0.8]);
}
module gauge_engrave() for (l = spec_gauge) {
    translate([0, fin_top(l[0]) - 0.3]) square([plot_y, 0.6]);
    translate([2, fin_top(l[0]) + (l[0] == 1 ? -2.6 : 2.6)]) txt(l[1], 3.0, "left");
}
card = [150, 8 + 7 * len(spec_answers)];
module card_engrave() for (k = [0:len(spec_answers)-1])
    translate([card[0] / 2, card[1] - 6 - 7 * k]) txt(spec_answers[k], k < len(spec_answers) - 1 ? 2.8 : 2.4);

// convex corners rounded for the jet (tab corners, the ridge tip); the plate keeps its 2 mm corners
// only - a rounding pass would erase the strip beside the gauge slots
module jet_round() if (jet) offset(r = 1) offset(delta = -1) children(); else children();

// ================================================================ contours
lt       = st;
n_layers = ceil((base_t + plot_h) / lt);
// normalised height a layer stands for (its middle), and the contour a layer is cut to
function lvl(k) = (k * lt + lt / 2 - base_t) / plot_h;
module slice(k) projection(cut = true) translate([0, 0, -(k * lt + lt / 2)]) data_solid();
base_rect = [-rim_left, -rim_front, plot_x + rim_left + rim_right, plot_y + rim_front + rim_other];
module layer_cut(k) {
    if (k == 0) translate([base_rect[0], base_rect[1]]) square([base_rect[2], base_rect[3]]);
    else slice(k);
}
module layer_engrave(k) {
    if (k == 0) axis_marks();
    if (k < n_layers - 1) difference() { slice(k + 1); offset(delta = -0.4) slice(k + 1); }   // outline of the next layer
}
// widest row above a layer's level: a layer thinner than ~3 mm is a sliver
function width(k) = max([for (i = [0:nr - 1]) len([for (j = [0:nc - 1]) if (spec_z[i][j] >= lvl(k)) j])]) * dx;

// ================================================================ pieces
module one(p, k = 0) {
    if (p == "fin")   { as_cut(kerf) jet_round() fin_shape(fin_rows[k]); if (!jet) as_engrave() fin_engrave(fin_rows[k]); }
    if (p == "plate") { as_cut(kerf) plate_shape(); if (!jet) as_engrave() axis_marks(); }
    if (p == "gauge") { as_cut(kerf) jet_round() gauge_shape(); if (!jet) as_engrave() gauge_engrave(); }
    if (p == "card")  { as_cut(kerf) square(card); as_engrave() card_engrave(); }
    if (p == "layer") { as_cut(kerf) layer_cut(k); as_engrave() layer_engrave(k); }
}
if (piece == "fin" || piece == "layer") one(piece, idx);
if (piece == "plate" || piece == "gauge" || piece == "card") one(piece);

// for export_cut.py: PIECE|name|material|machine|qty|note (a name ending in digits = piece + idx)
if (piece == "list") {
    if (style == "fins") {
        for (k = [0:fin_n-1])
            echo(str("PIECE|fin", k, "|", stock, "|", machine, "|1|", r(row_val(fin_rows[k]), 3), " ", spec_row_unit));
        echo(str("PIECE|plate|", stock, "|", machine, "|1"));
        if (has_gauge) echo(str("PIECE|gauge|", stock, "|", machine, "|1"));
        if (!jet) echo(str("PIECE|card|", stock, "|", machine, "|1|answers"));
    } else for (k = [0:n_layers - 1])
        echo(str("PIECE|layer", k, "|3 mm acrylic|laser|1|", k == 0 ? "base" : str(r(lvl(k), 2), " of the peak")));
}

// the layout on one sheet (fins in two columns; plate, gauge and card in a third)
fin_pitch = fin_base + plot_h + st + 5;
col3 = 2 * (plot_x + 5);
if (piece == "sheets") {
    sheet(jet ? [500, 300] : laser_bed);
    if (style == "fins") {
        for (k = [0:fin_n-1]) translate([5 + floor(k / 4) * (plot_x + 5), 5 + (k % 4) * fin_pitch + st]) one("fin", k);
        translate([5 + col3 - plate_x[0], 5 - plate_y[0]]) one("plate");
        g0 = 5 + plate_y[1] - plate_y[0] + 5 + st;
        if (has_gauge) translate([5 + col3, g0]) one("gauge");
        if (!jet) translate([5 + col3, g0 + (has_gauge ? gauge_h + 5 : 0)]) one("card");
    } else {
        translate([rim_left + 5, rim_front + 5]) one("layer", 0);
        for (k = [1:n_layers - 1]) translate([base_rect[2] + 15 + (k - 1) * 40 - fx(0) + 15, rim_front + 5]) one("layer", k);
    }
}
// the fins put together, to check the slots in 3D
if (piece == "assembly") {
    color("lightblue", 0.8) linear_extrude(st) plate_shape();
    for (i = fin_rows) color("orange") translate([0, i * dy + st / 2, st]) rotate([90, 0, 0]) linear_extrude(st) fin_shape(i);
    if (has_gauge) color("ivory") translate([gauge_cx - st / 2, 0, st]) rotate([90, 0, 90]) linear_extrude(st) gauge_shape();
}

// ================================================================ checks
if (style == "fins") {
    echo(str("spectrum fins (", machine, ", ", stock, ", kerf ", kerf, "): ", fin_n, " fins at rows ", fin_rows,
             ", ", r(min([for (k = [1:fin_n-1]) (fin_rows[k] - fin_rows[k-1]) * dy]) - st, 1), " mm clear between fins; slots ",
             slot_w, " mm; plate ", plate_x[1] - plate_x[0], " x ", plate_y[1] - plate_y[0], " mm"));
    if (jet && slot_w < st) echo("WARNING: a slot narrower than the sheet - below what the water-jet cuts cleanly");
} else {
    for (k = [1:n_layers - 1]) if (width(k) < 3)
        echo(str("WARNING layer ", k, " (", r(lvl(k), 2), " of the peak) is ", r(width(k), 1),
                 " mm wide at its widest: cut it from 1.5 mm stock or leave it off"));
    echo(str("spectrum contours: ", n_layers, " layers of ", lt, " mm; levels ", [for (k = [1:n_layers - 1]) r(lvl(k), 2)], " of the peak"));
}
