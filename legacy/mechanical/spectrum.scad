// A hand-held model of the instrument's own data, printed. After Jones et al., J. Chem. Educ. 2021,
// 98, 1024 (3D-printed 2D NMR spectra and chromatograms for the classroom).
//
// The data (spectrum-data.scad, from `py spectrum.py`) is a surface: frequency across, one row per
// record. Which experiment the rows are, and what to read off them, comes with the data:
//   sweep - one spectrum per B0 coil current: the ridge runs diagonally; its slope is the B0 pair's
//           calibration (Hz/A, divided by gamma: mT/A) and its width the field's homogeneity (ppm).
//           Under the board's voltage drive the ridge bends: the coils warm and B0 sinks.
//   decay - sliding-window spectra of one record: the ridge's height along time is the FID; a
//           gauge wall with grooves at 1, 1/e, 1/e^2 of the peak reads T2* (look from the left).
// spectrum.py says which of the paper's data steps it applies; this file adds the rest:
//   - a solid under the surface and a 3 mm base (the paper's prints broke at thin base-peak joints),
//   - a fixed footprint and a height cap (their raw data printed metres tall),
//   - axis ticks and labels on a rim round the data (their models had none), engraved to be felt,
//   - a groove across the front face at half height: its length is the FWHM, read against the
//     frequency ticks straight below it,
//   - the answers (spec_answers) engraved under the base, to check a reading against.
// The same data cut from sheet (laser, water-jet): cut-spectrum.scad.
//
// part = "model" (print orientation, base on the bed; no supports: a height map never overhangs)
part = "model";

include <spectrum-data.scad>

// ---------------------------------------------------------------- size
plot_x = 120;           // frequency axis, mm
plot_y = 80;            // row axis, mm
plot_h = 30;            // the tallest point (normalised 1) above the base
base_t = 3;             // solid under the lowest data point
rim_front = 16;         // label rim in front of the data (frequency axis)
rim_left  = 22;         // label rim left of the data (row axis)
rim_other = 3;          // plain rim behind
has_gauge = len(spec_gauge) > 0;
rim_right = has_gauge ? 8 : 3;   // carries the gauge wall
gauge_gap = 2.5;        // data edge to the wall's inner face (the grooves stay clear of the data)
gauge_t   = 2.5;        // wall thickness
groove    = 0.8;        // V-groove depth; 45 deg flanks, so a groove on a wall prints
engrave = 0.8;          // label and tick depth (the housing's labels are 0.6; this is to be felt)
font = "Liberation Sans:style=Bold";
grow = 0.05;            // stroke thickening, as in housing.scad

nr = len(spec_z);  nc = len(spec_z[0]);
dx = plot_x / (nc - 1);  dy = plot_y / (nr - 1);
function fx(f) = (f - spec_f[0]) / (spec_f[1] - spec_f[0]) * plot_x;              // Hz -> mm
function ty(t) = (t - spec_row[0]) / (spec_row[1] - spec_row[0]) * plot_y;        // row value -> mm
function row_val(i) = spec_row[0] + i * (spec_row[1] - spec_row[0]) / (nr - 1);
function row_max(i) = max(spec_z[i]);
function r(v, n) = round(v * pow(10, n)) / pow(10, n);
// a frequency tick step giving 4..8 ticks over the span
steps = [5, 10, 20, 50, 100, 200, 250, 500, 1000, 2000];
tick_f = [for (s = steps) if ((spec_f[1] - spec_f[0]) / s <= 8) s][0];

// ---------------------------------------------------------------- the data as a solid
// The matrix as a closed polyhedron from z = 0 to base_t + plot_h * z (MATLAB's surf2solid in the
// paper): top grid, bottom grid, four side walls; every face clockwise seen from outside.
module data_solid() {
    top = [for (i = [0:nr-1], j = [0:nc-1]) [j * dx, i * dy, base_t + plot_h * spec_z[i][j]]];
    bot = [for (i = [0:nr-1], j = [0:nc-1]) [j * dx, i * dy, 0]];
    function T(i, j) = i * nc + j;
    function B(i, j) = nr * nc + i * nc + j;
    faces = concat(
        [for (i = [0:nr-2], j = [0:nc-2]) each [[T(i,j), T(i+1,j), T(i+1,j+1)], [T(i,j), T(i+1,j+1), T(i,j+1)]]],
        [for (i = [0:nr-2], j = [0:nc-2]) each [[B(i,j), B(i,j+1), B(i+1,j+1)], [B(i,j), B(i+1,j+1), B(i+1,j)]]],
        [for (j = [0:nc-2]) [T(0,j), T(0,j+1), B(0,j+1), B(0,j)]],                       // front
        [for (j = [0:nc-2]) [T(nr-1,j+1), T(nr-1,j), B(nr-1,j), B(nr-1,j+1)]],           // back
        [for (i = [0:nr-2]) [T(i+1,0), T(i,0), B(i,0), B(i+1,0)]],                       // left
        [for (i = [0:nr-2]) [T(i,nc-1), T(i+1,nc-1), B(i+1,nc-1), B(i,nc-1)]]            // right
    );
    polyhedron(concat(top, bot), faces);
}

module txt(s, size, h = "center", v = "center")
    offset(delta = grow) text(s, size = size, font = font, halign = h, valign = v);

// ---------------------------------------------------------------- labels (2D, plot frame)
function ticks(a, b, step) = [for (k = [ceil(a / step - 1e-6) : floor(b / step + 1e-6)]) k * step];
module axis_marks() {
    // frequency: ticks reaching into the rim, values under them, axis title under those
    for (f = ticks(spec_f[0], spec_f[1], tick_f)) {
        translate([fx(f) - 0.5, -6]) square([1.0, 5]);
        translate([fx(f), -9]) txt(f == 0 ? "0" : str(f > 0 ? "+" : "−", abs(f)), 3.2);
    }
    translate([plot_x / 2, -13.5]) txt(str("Hz from ", spec_larmor_hz / 1000, " kHz"), 3.2);
    // rows: ticks into the left rim, values left of them, title turned along the axis
    for (t = ticks(spec_row[0], spec_row[1], spec_row_tick)) {
        translate([-6, ty(t) - 0.5]) square([5, 1.0]);
        translate([-7, ty(t)]) txt(str(r(t, 3)), 3.2, "right");
    }
    translate([-rim_left + 3, plot_y / 2]) rotate(90) txt(spec_row_name, 3.0);
}

// ---------------------------------------------------------------- reading aids
gx = plot_x + gauge_gap;                    // the gauge wall's inner face
function zh(h) = base_t + plot_h * h;       // normalised height -> mm
module gauge_wall() translate([gx, 0, 0]) cube([gauge_t, plot_y, zh(1) + 1.5]);
// V-groove along y in the wall's inner face, at height z
module groove_y(z) translate([gx, -1, z]) rotate([-90, 0, 0])
    linear_extrude(plot_y + 2) polygon([[-0.01, groove], [groove, 0], [-0.01, -groove]]);
// V-groove along x in the data's front face (y = 0), at height z
module groove_x(z) translate([-1, 0, z]) rotate([0, 90, 0])
    linear_extrude(plot_x + 2) polygon([[groove, -0.01], [0, groove], [-groove, -0.01]]);
// text engraved in the wall's inner face, read from the left (from -x)
module wall_text(s, y, z) translate([gx + engrave, y, z]) rotate([90, 0, -90])
    linear_extrude(engrave + 1) txt(s, 3.0, "left");
// on the bed face, mirrored to read from below
module answer_key() for (k = [0:len(spec_answers)-1])
    translate([plot_x / 2 - (rim_left - rim_right) / 2, plot_y - 8 - 7 * k, -1]) linear_extrude(0.6 + 1)
        mirror([1, 0]) txt(spec_answers[k], k < len(spec_answers) - 1 ? 2.8 : 2.4);

module model() {
    difference() {
        union() {
            data_solid();
            translate([-rim_left, -rim_front, 0])
                cube([plot_x + rim_left + rim_right, plot_y + rim_front + rim_other, base_t]);
            if (has_gauge) gauge_wall();
        }
        translate([0, 0, base_t - engrave]) linear_extrude(engrave + 1) axis_marks();
        if (has_gauge) for (l = spec_gauge) {
            groove_y(zh(l[0]));
            wall_text(l[1], plot_y - 2, zh(l[0]) + (l[0] == 1 ? -2.6 : 2.6));
        }
        groove_x(zh(row_max(0) / 2));
        answer_key();
    }
}

// ---------------------------------------------------------------- output
if (part == "model") model();

echo(str("model ", plot_x + rim_left + rim_right, " x ", plot_y + rim_front + rim_other, " x ",
         zh(1) + (has_gauge ? 1.5 : 0), " mm; data ", nr, " x ", nc, " (", r(dx, 2), " mm per frequency column, ",
         r(dy, 2), " mm per row); ", spec_mode, ": ", spec_row_name, " ", spec_row[0], "..", spec_row[1]));
// ridge width at half height in the front row: the print's thinnest feature (paper: needles break)
half = [for (j = [0:nc-1]) if (spec_z[0][j] >= row_max(0) / 2) j];
echo(str("ridge at half height: ", r(len(half) * dx, 1), " mm wide (keep it above ~4 mm for PLA)"));
