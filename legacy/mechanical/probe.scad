// NMR probe: the coil former for the class coil, a sleeve that holds a 50 mL centrifuge tube,
// and a cradle that holds the coil next to the instrument, joined to it by cables (TX COIL
// terminal, RX SMA). Separate from the housing so the sample sits away from the boards.
//
// The class coil (workbook blocks/a.md, b.md; bring-up.md): 400 turns AWG26 on a 4 cm x 10 cm
// former, L = 2.53 mH, B/I = 5.03 mT/A; 60-250 mL of water.
// The 2 mT field coil pair is not specified in the repository; B0 must be perpendicular to
// this coil's axis (x in the assembly), so the pair's axis is y or z. The winding's centre is
// x = 0 in the assembly and is marked on the cradle, so the pair can be centred on it.
//
// Taken from "Nuclear Magnetic Resonance for Everyone" (hackaday.io/project/191192), an
// Earth's-field coil of the same size (40/36 mm tube, two layers, 50 mL centrifuge tube):
//  - 6 mm end cheeks (theirs are two 3 mm laser-cut pieces glued): stiff enough for two
//    layers wound under tension;
//  - the tube runs on past the lead-end cheek: the leads come out through the cheek and are
//    clamped to that stub with a cable tie, together with the cable, so a pull on the cable
//    never reaches the winding;
//  - the 50 mL centrifuge tube is the sample holder (signal seen with 25 mL); it is thinner
//    than the bore, so a printed sleeve centres it and its cap is the end stop;
//  - "it is crucial to be able to freely rotate the coil": for the Earth's-field stretch goal
//    (NMR-FIRMWARE.md) do not screw the cradle down; turn it until the axis points east-west
//    (then it is across the Earth's field at any inclination) and move it about - 30 cm can
//    decide between a signal and none;
//  - the coil rides on a camera tripod (their step 5: tripod hardware swapped for nylon):
//    a brass 1/4"-20 nut under the winding centre;
//  - no steel in the cable: check the cable and its plug with a phone's magnetometer or a
//    magnet before soldering it to the coil.
//
// Winding (their tips): start lead out through the cheek and cable-tied to the stub; keep the
// supply spool from running free and the wire under constant tension; a drop of
// cyanoacrylate on the first turns; wind to the far cheek, step up a layer, come back - two
// layers end where they began, so both leads leave at the stub end.
//
// No steel near the sample: the cradle has no screws of its own; fix it with brass or nylon
// screws, and keep steel tools and screws away from the coil.
//
// part = "assembly", "former" (print standing, plain cheek on the bed), "sleeve" (flange on
// the bed), "cradle".
part = "assembly";

$fn = 96;
fit = 0.3;                // every hole 0.3 mm larger than the part through it
eps = 0.01;
chamf = 1.2;              // 45 deg lead-in / edge break on mating parts (eases insertion, kills the print's sharp lip)
mu0 = 4 * PI * 1e-7;
include <nmr-params.scad>

// a 45 deg lead-in cone that widens a bore of diameter d by `c` over a depth `c`, at its mouth (open upward at z=0)
module leadin(d, c) translate([0, 0, -eps]) cylinder(d1 = d + 2 * c, d2 = d, h = c + eps);

// ---------------------------------------------------------------- coil (from the docs)
turns   = 266;            // 2 layers x 133 over the 60 mm winding (design-closure.md: 60 mm active region)
wire_d  = 0.45;           // AWG26 enamelled, overall diameter (the article's AWG24 is 0.55)
wire_cu = 0.405;          // AWG26 copper diameter, for the mass to buy
wind_d  = 40;             // winding surface (former outside diameter)
wind_l  = 60;             // winding length = B0-homogeneous active region (design-closure.md; was 100 mm)
tube_t  = 2;              // former wall
cheek_t = 6;              // end cheek thickness (article: 6 mm end pieces)
cheek_h = 3;              // cheek height above the winding surface
lead_d  = 1.2;            // holes for the start and end leads
stub_l  = 10;             // tube past the lead-end cheek: cable tie for the leads and the cable
tie_w   = 5;              // cable-tie groove in the stub
tie_dp  = 0.8;

bore     = wind_d - 2 * tube_t;              // 36: the bottle or the sleeve goes in here
cheek_d  = wind_d + 2 * cheek_h;             // 46
per_layer = floor(wind_l / wire_d);
layers    = ceil(turns / per_layer);
build     = layers * wire_d;
z_wind    = [cheek_t, cheek_t + wind_l];     // winding, along the former's axis
z_cheek2  = z_wind[1] + cheek_h;             // lead-end cheek starts (after the 45 deg cone)
z_stub    = z_cheek2 + cheek_t;              // stub starts
former_l  = z_stub + stub_l;                 // plain cheek, winding, cone, cheek, stub
z_c       = (z_wind[0] + z_wind[1]) / 2;     // winding centre

// ---------------------------------------------------------------- sample: 50 mL centrifuge tube
// Nominal 30 x 115 mm with the cap (Falcon / Corning type, conical tip). Measure yours: the
// body tapers slightly, the cap must be wider than the sleeve's hole (it is the stop).
smp_d   = 30;             // body diameter
smp_l   = 115;            // overall length with the cap
cap_d   = 34;
cap_h   = 12;
cone_l  = 20;             // conical tip
fl_t    = 2;              // sleeve flange, outside the plain cheek
sleeve_od = bore - fit;
sleeve_id = smp_d + fit;
sleeve_l  = former_l;     // supports the body along the whole bore

// ---------------------------------------------------------------- cradle
base_t    = 4;
under_arc = 6;            // material under the former, room for the zip-tie tunnel
saddle_w  = 8;
arc_r     = cheek_d / 2 + fit / 2;
axis_z    = base_t + under_arc + arc_r;
// assembly frame: x along the coil axis, winding centre at x = 0, plain cheek (sample end) at -x,
// lead end with the stub (cable end) at +x
x0        = -z_c - fl_t;                     // sleeve flange
x1        = former_l - z_c;                  // end of the stub
base_x    = [x0 - 8, x1 + 8 + 14];           // the cable end (+x) is longer: anchor
base_w    = 60;
tie       = 4.5;          // zip-tie tunnel, diamond (45 deg roof, prints without supports)
anchor    = [10, 22, 12]; // cable anchor block at the cable end, past the former
mount_d   = 3 + fit;      // M3 (brass or nylon) holes to fix the cradle (2 mT pair only)
// tripod mount under the winding centre (the article mounts its coil on a camera tripod head
// with every steel screw swapped for nylon): a BRASS 1/4"-20 nut, 7/16" across flats, pressed
// in from below; swap the tripod's own steel screw for brass or nylon too
tri_af    = 11.11 + fit;
tri_nut_h = 5.6 + fit;
tri_hole  = 6.35 + fit;
tri_boss  = [22, 10];     // diameter, height (clears the winding by about 2 mm)
saddle_x  = [cheek_t / 2 - z_c, z_cheek2 + cheek_t / 2 - z_c];   // under the two cheeks

label_depth = 0.6;
label_font  = "Liberation Sans:style=Bold";
label_y     = -(base_w / 2 + arc_r) / 2;     // between the former's shadow and the base edge

// ---------------------------------------------------------------- parts
// standing, z = coil axis, z = 0 on the bed
module former() {
    difference() {
        union() {
            cylinder(d = cheek_d, h = cheek_t);                                   // plain cheek
            cylinder(d = wind_d, h = former_l);                                   // tube + stub
            translate([0, 0, z_wind[1]]) cylinder(d1 = wind_d, d2 = cheek_d, h = cheek_h);  // 45 deg cone
            translate([0, 0, z_cheek2]) cylinder(d = cheek_d, h = cheek_t);       // lead-end cheek
        }
        translate([0, 0, -1]) cylinder(d = bore, h = former_l + 2);
        // lead-in at both bore mouths: the sleeve (or a bare bottle) slides in without catching on a sharp print lip
        leadin(bore, chamf);
        translate([0, 0, former_l]) mirror([0, 0, 1]) leadin(bore, chamf);
        // both leads leave through the lead-end cheek, onto the stub
        for (a = [0, 20]) rotate(a) translate([wind_d / 2 + lead_d / 2, 0, z_wind[1] - 1])
            cylinder(d = lead_d, h = z_stub - z_wind[1] + 1 + eps, $fn = 16);
        // cable-tie groove round the stub (1 mm ledge above it prints fine)
        translate([0, 0, z_stub + (stub_l - tie_w) / 2]) difference() {
            cylinder(d = wind_d + 1, h = tie_w);
            translate([0, 0, -1]) cylinder(d = wind_d - 2 * tie_dp, h = tie_w + 2);
        }
    }
}

// standing on the flange; the flange sits against the plain cheek, the tube's cap against it
module sleeve() {
    difference() {
        union() {
            cylinder(d = cheek_d, h = fl_t);
            cylinder(d = sleeve_od, h = fl_t + sleeve_l);
        }
        translate([0, 0, -1]) cylinder(d = sleeve_id, h = fl_t + sleeve_l + 2);
        // lead-in at the open (top) end so the 50 mL tube's conical tip finds the bore
        translate([0, 0, fl_t + sleeve_l]) mirror([0, 0, 1]) leadin(sleeve_id, chamf);
        // a notch in the flange to lever the sleeve out with a fingernail
        translate([sleeve_od / 2, -3, -1]) cube([cheek_d, 6, fl_t + 2]);
    }
}

// the 50 mL tube along +z, cap end at z = 0
module sample_tube() {
    cylinder(d = cap_d, h = cap_h);
    translate([0, 0, cap_h]) cylinder(d = smp_d, h = smp_l - cap_h - cone_l);
    translate([0, 0, smp_l - cone_l]) cylinder(d1 = smp_d, d2 = 4, h = cone_l);
}

// diamond tunnel along the y axis, centred at the origin
module tunnel_y(w, l) rotate([90, 0, 0]) linear_extrude(l, center = true) rotate(45) square(w / sqrt(2), center = true);

module cradle() {
    difference() {
        union() {
            translate([base_x[0], -base_w / 2, 0]) cube([base_x[1] - base_x[0], base_w, base_t]);
            for (x = saddle_x) translate([x - saddle_w / 2, -(arc_r + 5), 0]) cube([saddle_w, 2 * (arc_r + 5), axis_z]);
            translate([base_x[1] - anchor[0] - 2, -anchor[1] / 2, 0]) cube(anchor);
            cylinder(d = tri_boss[0], h = tri_boss[1]);                           // tripod boss
        }
        // tripod nut from below (its roof is an 11 mm bridge), screw hole above it
        translate([0, 0, -eps]) rotate(30) cylinder(d = tri_af / cos(30), h = tri_nut_h, $fn = 6);
        translate([0, 0, -1]) cylinder(d = tri_hole, h = tri_boss[1] + 2, $fn = 32);
        // the former's cheeks rest in the arcs
        translate([base_x[0] - 1, 0, axis_z]) rotate([0, 90, 0]) cylinder(r = arc_r, h = x1 + 4 - base_x[0] + 1);   // stops short of the anchor
        // a zip tie round each cheek, under the former through the saddle
        for (x = saddle_x) translate([x, 0, base_t + under_arc / 2]) tunnel_y(tie, 2 * base_w);
        // a zip tie holds the cables to the anchor
        translate([base_x[1] - anchor[0] / 2 - 2, 0, anchor[2] - 5]) tunnel_y(tie, 2 * base_w);
        // fixing holes beyond the saddles, in the corners (clear of the anchor, which is narrower)
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx < 0 ? base_x[0] + 3.5 : base_x[1] - 3.5, sy * (base_w / 2 - 6), -1])
                cylinder(d = mount_d, h = base_t + 2, $fn = 32);
        // engraved on the base, both sides: the winding centre and how to point the axis
        for (s = [-1, 1]) translate([0, s * label_y, base_t - label_depth]) linear_extrude(label_depth + 1)
            rotate(s < 0 ? 180 : 0) {
                square([0.8, 6], center = true);                                  // winding centre
                translate([-3, 0]) text("AXIS 90° TO B0", size = 3, font = label_font, halign = "right", valign = "center");
                translate([3, 0])  text("EARTH FIELD: E–W", size = 3, font = label_font, halign = "left", valign = "center");
            }
    }
}

// ---------------------------------------------------------------- what is not printed (assembly only)
// the wound coil, the cable ties and the cable, so the assembly looks like the built probe
module winding() {
    translate([0, 0, z_wind[0]]) difference() {
        cylinder(d = wind_d + 2 * build, h = wind_l);
        translate([0, 0, -1]) cylinder(d = wind_d - eps, h = wind_l + 2);
    }
    // the two leads out through the cheek and along the stub, under the cable tie
    for (a = [0, 20]) rotate(a) translate([wind_d / 2 + lead_d / 2, 0, z_wind[1]])
        cylinder(d = wire_d, h = former_l - z_wind[1] + 4, $fn = 8);
}
module stub_tie() translate([0, 0, z_stub + (stub_l - tie_w) / 2 + 0.5]) difference() {
    cylinder(d = wind_d - 2 * tie_dp + 2.6, h = tie_w - 1);
    translate([0, 0, -1]) cylinder(d = wind_d - 2 * tie_dp, h = tie_w + 1);
}
// cable (two-core, screened, no steel) from under the stub, through the anchor tie, away
module cable(d = 5) {
    pts = [[x1 - 4, 0, axis_z - wind_d / 2 - d / 2], [x1 + 6, 0, anchor[2] + d / 2],
           [base_x[1] + 40, 0, anchor[2] + d / 2]];
    for (i = [0:len(pts) - 2]) hull() { translate(pts[i]) sphere(d = d, $fn = 16); translate(pts[i + 1]) sphere(d = d, $fn = 16); }
}

// ---------------------------------------------------------------- B0 coil pair (reference, not printed)
// The repository does not specify the 2.1 mT coil. Libbrecht's apparatus (docs/references) uses a
// large coil, 2.098 mT at 1.130 A, uniform to ~25 ppm over the sample. This is the cheapest
// plausible equivalent: a Helmholtz pair (B = (4/5)^1.5 mu0 N I / R, sized the way the
// hackaday.io "Highly configurable 3D printed Helmholtz coil" does it), axis y, so B0 is across
// the probe's axis, standing on the table with the probe raised to its centre on the tripod.
// Its weak point is homogeneity: the class coil's 100 mm winding lies across the pair's axis, and
// the field falls off as 0.432 (rho/R)^4 there, which sets T2* - see the echo. R is the
// trade-off between T2* and copper mass / power; within the board's H-bridge limits
// (I_TRIP 2.0 A, +VEXT 7-18 V; D-42).
show_b0 = true;
f_L     = nmr_f_tx;         // Larmor frequency, Hz (nmr-params.scad)
gamma_p = nmr_gamma;        // Hz/T
hh_R    = 200;              // mean coil radius = spacing, mm
hh_I    = 1.5;              // A, margin under the H-bridge trip (hb_trip)
hh_cu   = 1.291;            // AWG16 copper diameter, mm
hh_wd   = 1.37;             // AWG16 enamelled, overall
hh_w    = 28;               // winding channel, axial width, mm
hh_rim  = 3;                // printed or wooden ring around the winding

B0      = f_L / gamma_p;    // T
hh_N    = ceil(B0 * hh_R / 1000 / (pow(4 / 5, 1.5) * mu0 * hh_I));
hh_h    = ceil(hh_N * hh_wd * hh_wd / 0.8 / hh_w);          // radial build at 80 % fill
hh_len  = 2 * hh_N * 2 * PI * hh_R / 1000;                  // m, both coils
hh_ohm  = hh_len * 1.724e-8 / (PI * pow(hh_cu / 2000, 2));
z_table = axis_z - (hh_R + hh_h / 2 + hh_rim) - 10;         // the rings stand on 10 mm feet

module b0_ring() rotate([90, 0, 0]) rotate_extrude($fn = 120) difference() {
    translate([hh_R - hh_h / 2 - hh_rim, -hh_w / 2 - hh_rim]) square([hh_h + 2 * hh_rim, hh_w + 2 * hh_rim]);
    translate([hh_R - hh_h / 2, -hh_w / 2]) square([hh_h + hh_rim + 1, hh_w]);
}
module b0_winding() rotate([90, 0, 0]) rotate_extrude($fn = 120)
    translate([hh_R - hh_h / 2, -hh_w / 2]) square([hh_h, hh_w]);

// ---------------------------------------------------------------- output
module probe_assembly(b0 = show_b0, table = true) {
    color("ivory") cradle();
    translate([-z_c, 0, axis_z]) rotate([0, 90, 0]) {
        color("orange") former();
        color("chocolate") winding();
        color("lightblue") translate([0, 0, -fl_t]) sleeve();
        color("dimgray") stub_tie();
        // the 50 mL tube, cap against the sleeve flange, transparent
        %translate([0, 0, -fl_t - cap_h]) sample_tube();
    }
    color("dimgray") translate([base_x[1] - anchor[0] / 2 - 2, 0, anchor[2] - 5])
        cube([3, anchor[1] + 2, 9], center = true);                              // anchor tie
    color("black") cable();
    if (b0) for (s = [-1, 1]) translate([0, s * hh_R / 2, axis_z]) {
        color("burlywood", 0.35) b0_ring();
        color("chocolate", 0.35) b0_winding();
    }
    %translate([0, 0, z_table]) cylinder(d = 25, h = -z_table);                   // tripod column
    if (table) %translate([-300, -250, z_table - 5]) cube([600, 500, 5]);         // the table
}
if (part == "assembly") probe_assembly();

// ---------------------------------------------------------------- for instrument.scad (use <probe.scad>)
// in this file's assembly frame (winding centre at x = y = 0, cradle underside z = 0)
function probe_table_z() = z_table;
function probe_cable_end() = [base_x[1] + 40, 0, anchor[2] + 2.5];            // where cable() ends
function probe_b0_leads() = [0, hh_R / 2 + hh_w / 2, axis_z - hh_R - hh_h / 2];  // +y ring, bottom
function probe_sample() = [0, 0, axis_z];
function probe_b0() = [hh_R, hh_I, hh_ohm, hh_N, hh_h];
if (part == "former") former();
if (part == "sleeve") sleeve();
if (part == "cradle") cradle();

// ---------------------------------------------------------------- numbers to check
wire_m = turns * PI * (wind_d + build) / 1000;
echo(str("winding: ", per_layer, " turns per layer, ", layers, " layers (", build, " mm), wire about ",
         round(wire_m), " m, ", round(wire_m * PI * pow(wire_cu / 2, 2) * 8.96), " g of copper (buy more)"));
echo(str("B/I = mu0 N / l = ", round(mu0 * turns / (wind_l / 1000) * 1e5) / 100, " mT/A long-solenoid approx (Biot-Savart solver: 4.61 mT/A at 60 mm)"));
echo(str("bottle (no sleeve): at most ", bore - fit, " mm diameter; water inside the winding ",
         round(PI * pow((bore - fit) / 2, 2) * wind_l / 1000), " mL (docs: 60-250 mL)"));
smp_body = [-fl_t - cap_h + cap_h, -fl_t - cap_h + smp_l - cone_l];      // tube body, former z
echo(str("50 mL tube in the sleeve: body z ", smp_body[0], "..", smp_body[1], ", tip ", -fl_t - cap_h + smp_l,
         "; winding z ", z_wind[0], "..", z_wind[1], "; sleeve wall ", (sleeve_od - sleeve_id) / 2, " mm"));
echo(str("former ", former_l, " mm long (stub ", stub_l, "), cheeks ", cheek_d, " x ", cheek_t, " mm; cradle ",
         base_x[1] - base_x[0], " x ", base_w, " mm, coil axis ", axis_z, " mm above its underside"));
echo(str("tripod: brass 1/4\"-20 nut pocket ", tri_nut_h, " mm deep under the winding centre; boss top ",
         tri_boss[1], ", winding bottom ", axis_z - wind_d / 2 - build, " mm above the table"));
hh_rad = 0.432 * pow((wind_l / 2) / hh_R, 4);      // across the pair's axis, at the winding ends
hh_ax  = 1.152 * pow((bore / 2) / hh_R, 4);        // along it, at the bore wall
hh_df  = f_L * max(hh_rad, hh_ax);
echo(str("B0 pair: ", B0 * 1000, " mT at ", hh_I, " A -> 2 x ", hh_N, " turns AWG16 on R = ", hh_R,
         " mm, channel ", hh_w, " x ", hh_h, " mm; ", round(hh_len), " m, ", round(hh_ohm * 10) / 10, " ohm, ",
         round(hh_I * hh_ohm * 10) / 10, " V, ", round(hh_I * hh_I * hh_ohm), " W, ",
         round(hh_len * PI * pow(hh_cu / 2, 2) * 8.96 / 100) / 10, " kg of copper; within the H-bridge: ",
         hh_I < hb_trip && hh_I * hh_ohm <= vext_max));
// These are the worst points (the winding's ends, the bore wall): most of the water sits much nearer
// the centre, and the line is a narrow cusp with a tail, not a 150 Hz Lorentzian - spectrum.py samples
// the field over the 50 mL tube's water (about 2 Hz FWHM, 18 ppm, tail to the extremes below). The
// thing that widens the line is B0 drift: see instrument.scad.
echo(str("B0 homogeneity, worst points of the winding: ", round(hh_rad * 1e6), " ppm across, ", round(hh_ax * 1e6),
         " ppm along -> extremes ", round(hh_df), " Hz apart (the tail of the line, not its width: spectrum.py)"));

// ---------------------------------------------------------------- assembly self-checks (fail loudly in the console)
sleeve_wall = (sleeve_od - sleeve_id) / 2;
tip_z       = -fl_t - cap_h + smp_l;                 // 50 mL tube tip, in the former frame (z along the axis)
fixing_gap  = (base_x[1] - 3.5) - (base_x[1] - anchor[0] - 2);   // corner fixing hole vs anchor block edge
assert(sleeve_wall >= 0.8, str("sleeve wall ", sleeve_wall, " mm is under the 0.8 mm print minimum"));
// the sample must cover the full winding (the active region) — the tube may legitimately overhang the shorter former
assert(smp_body[0] <= z_wind[0] && smp_body[1] >= z_wind[1],
       str("sample body z ", smp_body[0], "..", smp_body[1], " does not cover the winding z ", z_wind[0], "..", z_wind[1]));
assert(build <= cheek_h, str("winding build ", build, " mm stands above the ", cheek_h, " mm cheek rim"));
assert(hh_I < hb_trip && hh_I * hh_ohm <= vext_max, "B0 pair exceeds the H-bridge current or +VEXT limit");
echo(str("CHECK ok: sleeve wall ", round(sleeve_wall * 100) / 100, " mm; winding z ", z_wind[0], "..", z_wind[1],
         " covered by sample z ", round(smp_body[0]), "..", round(smp_body[1]), "; tube tip ", round(tip_z),
         " mm (overhang ", round(tip_z - former_l), " mm past the ", former_l, " mm former); lead-in chamfer ", chamf, " mm"));

// ---------------------------------------------------------------- bill of materials (one place, for the build)
echo("BOM printed (PLA/PETG): former x1, sleeve x1, cradle x1  (OpenSCAD -> STL; see part=)");
echo("BOM laser-cut (acrylic/ply): cut-probe.scad -> cradle rails x2, cheeks/leads, B0 rails + brace");
echo(str("BOM wire: class coil ", round(wire_m), " m AWG26 (~",
         round(wire_m * PI * pow(wire_cu / 2, 2) * 8.96), " g Cu); B0 pair ", round(hh_len), " m AWG16 (~",
         round(hh_len * PI * pow(hh_cu / 2, 2) * 8.96 / 100) / 10, " kg Cu)"));
echo(str("BOM hardware (no steel near the sample): brass/nylon 1/4\"-20 tripod nut x1, M3x? brass/nylon fixing x4, ",
         "zip ties (", tie, " mm), 50 mL centrifuge tube, two-core screened cable"));
