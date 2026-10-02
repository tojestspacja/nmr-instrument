// nmr-render — a tiny CPU software rasteriser that renders the canonical NMR bench scene to a PNG, so visual choices
// (camera, framing, holder visibility, B0/B1 readability, transparency, exploded view) can be inspected, not guessed.
//
// Geometry is NOT defined here: it is loaded from assets/scene.bin, a 1:1 re-container of legacy/simulator/m/*.txt (the
// same meshes the Three.js site loads, built from the OpenSCAD by legacy/simulator/build/pack.py). Only PRESENTATION
// (colours, per-preset visibility/opacity, cameras) lives in this file. Canonical frame: +x sample/RF-solenoid axis,
// +y Helmholtz/B0 axis, +z up.
//
// Why a CPU rasteriser and not GLFW/OpenGL: this tool's main user runs in a headless automation sandbox where a GPU GL
// context is unreliable; a software rasteriser is deterministic and dependency-free (only stb_image_write), so it
// always runs here. Same CLI/PNG contract a GL backend would have.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ------------------------------------------------------------------ tiny math (column-major 4x4, like GL)
struct V3 { double x = 0, y = 0, z = 0; };
static V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
static double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static double len(V3 a) { return std::sqrt(dot(a, a)); }
static V3 norm(V3 a) { double l = len(a); return l > 1e-12 ? a * (1.0 / l) : a; }

struct M4 { double m[16] = {0}; };   // column-major: m[col*4+row]
static M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}
// returns clip-space (x,y,z,w)
static void mulVec4(const M4& a, double x, double y, double z, double w, double out[4]) {
    for (int row = 0; row < 4; ++row)
        out[row] = a.m[0 * 4 + row] * x + a.m[1 * 4 + row] * y + a.m[2 * 4 + row] * z + a.m[3 * 4 + row] * w;
}
static M4 lookAt(V3 eye, V3 c, V3 up) {
    V3 f = norm(c - eye), s = norm(cross(f, up)), u = cross(s, f);
    M4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z; r.m[12] = -dot(s, eye);
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z; r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    r.m[15] = 1;
    return r;
}
static M4 perspective(double fovyDeg, double asp, double zn, double zf) {
    double f = 1.0 / std::tan(fovyDeg * 3.14159265358979 / 360.0);
    M4 r;
    r.m[0] = f / asp; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1;
    r.m[14] = (2 * zf * zn) / (zn - zf);
    return r;
}
static M4 ortho(double hw, double hh, double zn, double zf) {
    M4 r;
    r.m[0] = 1 / hw; r.m[5] = 1 / hh; r.m[10] = -2 / (zf - zn); r.m[14] = -(zf + zn) / (zf - zn); r.m[15] = 1;
    return r;
}

// ------------------------------------------------------------------ scene data (geometry from scene.bin)
struct Part { std::string id; std::vector<float> pos; std::vector<uint32_t> idx; };
static std::vector<Part> load_scene(const char* path) {
    std::vector<Part> parts;
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return parts; }
    char magic[4]; uint32_t np = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "NMRS", 4) != 0) { std::fclose(f); return parts; }
    std::fread(&np, 4, 1, f);
    for (uint32_t i = 0; i < np; ++i) {
        uint32_t nl, nv, ni;
        std::fread(&nl, 4, 1, f);
        std::string id(nl, 0); std::fread(id.data(), 1, nl, f);
        std::fread(&nv, 4, 1, f); std::fread(&ni, 4, 1, f);
        Part p; p.id = id; p.pos.resize(nv * 3); p.idx.resize(ni);
        std::fread(p.pos.data(), 4, nv * 3, f); std::fread(p.idx.data(), 4, ni, f);
        parts.push_back(std::move(p));
    }
    std::fclose(f);
    return parts;
}

// ------------------------------------------------------------------ presentation (colour/group per part)
struct Mat { double r, g, b, a; const char* group; };   // group: holder|wiring|pcb|b0|housing|probe
static Mat material(const std::string& id) {
    // colours echo the Three.js MATS; alpha < 1 marks see-through parts
    if (id == "sample")      return {0.26, 0.56, 0.85, 0.34, "holder"};   // water in the tube
    if (id == "sleeve")      return {0.40, 0.68, 0.90, 0.92, "holder"};   // printed sleeve: keep it readable
    if (id == "former")      return {0.90, 0.55, 0.18, 1.00, "holder"};
    if (id == "winding")     return {0.74, 0.46, 0.22, 0.80, "holder"};   // slightly see-through to show the sample
    if (id == "ties")        return {0.23, 0.25, 0.24, 1.00, "holder"};
    if (id == "cradle")      return {0.93, 0.90, 0.82, 1.00, "holder"};
    if (id == "tripod")      return {0.58, 0.62, 0.59, 1.00, "holder"};
    if (id == "b0_rings")    return {0.80, 0.71, 0.52, 0.30, "b0"};       // big transparent formers: don't dominate
    if (id == "b0_windings") return {0.62, 0.36, 0.17, 0.95, "b0"};
    if (id == "panel")       return {0.13, 0.38, 0.24, 1.00, "pcb"};
    if (id == "main")        return {0.18, 0.48, 0.33, 1.00, "pcb"};
    if (id == "dev")         return {0.17, 0.31, 0.50, 1.00, "pcb"};
    if (id == "box")         return {0.76, 0.88, 0.92, 0.30, "housing"};
    if (id == "shell")       return {0.90, 0.88, 0.83, 0.55, "housing"};
    if (id == "hood")        return {0.90, 0.55, 0.18, 1.00, "housing"};
    if (id == "probe_cable" || id == "cable_rx") return {0.09, 0.09, 0.09, 1.00, "wiring"};
    if (id == "cable_tx")    return {0.23, 0.25, 0.24, 1.00, "wiring"};
    if (id == "cable_hb")    return {0.62, 0.36, 0.17, 1.00, "wiring"};
    return {0.7, 0.7, 0.7, 1.0, "holder"};
}

// ------------------------------------------------------------------ framebuffer
struct FB {
    int w, h; std::vector<float> col; std::vector<float> depth;   // col rgb, depth in NDC (-1..1), +inf far
    FB(int W, int H, V3 bg) : w(W), h(H), col(W * H * 3), depth(W * H, 1e30) {
        for (int i = 0; i < W * H; ++i) { col[i * 3] = bg.x; col[i * 3 + 1] = bg.y; col[i * 3 + 2] = bg.z; }
    }
};

struct SV { double x, y, z, w; };   // clip-space vertex
// rasterise one triangle (screen coords already), flat colour c, alpha a, writing depth only if opaque
static void tri(FB& fb, SV a, SV b, SV c, double cr, double cg, double cb, double alpha, bool writeDepth) {
    // perspective divide to NDC, then to pixels
    auto to_px = [&](SV v, double& px, double& py, double& pz) {
        px = (v.x / v.w * 0.5 + 0.5) * fb.w;
        py = (1.0 - (v.y / v.w * 0.5 + 0.5)) * fb.h;
        pz = v.z / v.w;
    };
    double ax, ay, az, bx, by, bz, cx, cy, cz;
    if (a.w <= 0 || b.w <= 0 || c.w <= 0) return;   // skip tris crossing the camera plane (good enough here)
    to_px(a, ax, ay, az); to_px(b, bx, by, bz); to_px(c, cx, cy, cz);
    int minx = std::max(0, (int)std::floor(std::min({ax, bx, cx})));
    int maxx = std::min(fb.w - 1, (int)std::ceil(std::max({ax, bx, cx})));
    int miny = std::max(0, (int)std::floor(std::min({ay, by, cy})));
    int maxy = std::min(fb.h - 1, (int)std::ceil(std::max({ay, by, cy})));
    double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (std::fabs(area) < 1e-9) return;
    for (int y = miny; y <= maxy; ++y)
        for (int x = minx; x <= maxx; ++x) {
            double px = x + 0.5, py = y + 0.5;
            double w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) / area;
            double w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) / area;
            double w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            double z = w0 * az + w1 * bz + w2 * cz;
            int di = y * fb.w + x;
            if (z > fb.depth[di]) continue;
            int ci = di * 3;
            if (alpha >= 0.999) { fb.col[ci] = cr; fb.col[ci + 1] = cg; fb.col[ci + 2] = cb; }
            else { fb.col[ci] = cr * alpha + fb.col[ci] * (1 - alpha);
                   fb.col[ci + 1] = cg * alpha + fb.col[ci + 1] * (1 - alpha);
                   fb.col[ci + 2] = cb * alpha + fb.col[ci + 2] * (1 - alpha); }
            if (writeDepth) fb.depth[di] = z;
        }
}

// append an arrow (shaft + head) as triangles into a Part, from p0 toward p0+dir (mm)
static void add_arrow(Part& P, V3 p0, V3 dir, double shaftR, double headR, double headLen) {
    double L = len(dir); if (L < 1e-6) return;
    V3 ax = norm(dir);
    V3 up = std::fabs(ax.z) < 0.9 ? V3{0, 0, 1} : V3{1, 0, 0};
    V3 u = norm(cross(ax, up)), v = cross(ax, u);
    const int N = 16;
    double shaftL = L - headLen;
    auto ring = [&](V3 center, double r, int seg) {
        std::vector<V3> pts(seg);
        for (int i = 0; i < seg; ++i) { double a = 2 * 3.14159265 * i / seg; pts[i] = center + u * (std::cos(a) * r) + v * (std::sin(a) * r); }
        return pts;
    };
    auto quad = [&](V3 a, V3 b, V3 c, V3 d) {
        uint32_t base = (uint32_t)(P.pos.size() / 3);
        for (V3 q : {a, b, c, d}) { P.pos.push_back((float)q.x); P.pos.push_back((float)q.y); P.pos.push_back((float)q.z); }
        for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) P.idx.push_back(base + i);
    };
    auto r0 = ring(p0, shaftR, N), r1 = ring(p0 + ax * shaftL, shaftR, N);
    for (int i = 0; i < N; ++i) quad(r0[i], r0[(i + 1) % N], r1[(i + 1) % N], r1[i]);
    auto rc = ring(p0 + ax * shaftL, headR, N);
    V3 tip = p0 + ax * L;
    for (int i = 0; i < N; ++i) {
        uint32_t base = (uint32_t)(P.pos.size() / 3);
        for (V3 q : {rc[i], rc[(i + 1) % N], tip}) { P.pos.push_back((float)q.x); P.pos.push_back((float)q.y); P.pos.push_back((float)q.z); }
        P.idx.push_back(base); P.idx.push_back(base + 1); P.idx.push_back(base + 2);
    }
}

int main(int argc, char** argv) {
    std::string preset = "instrument", out = "artifacts/instrument.png", projection = "";
    std::string scenePath = "assets/scene.bin";
    int W = 1600, H = 1000;
    double az = 1e9, el = 1e9, dist = 1e9, fov = 1e9;
    int showGrid = -1, showFields = -1, showWiring = -1, showPcb = -1, showHolder = -1, showLabels = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--preset") preset = next();
        else if (a == "--output" || a == "-o") out = next();
        else if (a == "--scene") scenePath = next();
        else if (a == "--projection") projection = next();
        else if (a == "--azimuth") az = atof(next());
        else if (a == "--elevation") el = atof(next());
        else if (a == "--distance") dist = atof(next());
        else if (a == "--fov") fov = atof(next());
        else if (a == "--width") W = atoi(next());
        else if (a == "--height") H = atoi(next());
        else if (a == "--show-grid") showGrid = 1;
        else if (a == "--no-grid") showGrid = 0;
        else if (a == "--show-fields") showFields = 1;
        else if (a == "--no-fields") showFields = 0;
        else if (a == "--show-wiring") showWiring = 1;
        else if (a == "--no-wiring") showWiring = 0;
        else if (a == "--show-pcb") showPcb = 1;
        else if (a == "--no-pcb") showPcb = 0;
        else if (a == "--no-holder") showHolder = 0;
        else if (a == "--show-labels") showLabels = 1;
    }

    std::vector<Part> parts = load_scene(scenePath.c_str());
    if (parts.empty()) { std::fprintf(stderr, "no geometry loaded from %s\n", scenePath.c_str()); return 2; }

    // ---- preset defaults (projection, camera, visibility, flags) over the SAME geometry
    bool fields = false, wiring = true, pcb = true, grid = false, holder = true, exploded = false;
    bool persp = true; double vfov = 14, elev = 20, azim = -55, explodeAmt = 0;
    V3 bg = {0.94, 0.95, 0.96};
    V3 target = {20, 0, 33};          // probe/sample region (winding centre x=0, coil axis z=AXIS~33)
    double fit = 1.25;                // framing margin
    if (preset == "instrument") { fields = false; wiring = false; pcb = false; grid = false; azim = -58; elev = 18; vfov = 14; }
    else if (preset == "physics") { fields = true; wiring = false; pcb = false; grid = false; azim = 32; elev = 15; vfov = 12; persp = false; }
    else if (preset == "probe") { fields = false; wiring = false; pcb = false; grid = false; azim = -60; elev = 16; vfov = 16; target = {0, 0, 33}; fit = 0.75; }
    else if (preset == "exploded") { fields = false; wiring = true; pcb = true; grid = false; exploded = true; explodeAmt = 1.0; azim = -58; elev = 18; vfov = 16; }
    if (projection == "ortho") persp = false; else if (projection == "perspective") persp = true;
    if (az < 1e8) azim = az; if (el < 1e8) elev = el; if (fov < 1e8) vfov = fov;
    if (showFields >= 0) fields = showFields; if (showWiring >= 0) wiring = showWiring;
    if (showPcb >= 0) pcb = showPcb; if (showGrid >= 0) grid = showGrid; if (showHolder >= 0) holder = showHolder;

    // ---- exploded offsets (along real assembly axes), mirrors the WebGL `ex` intents
    auto explodeOff = [&](const std::string& id) -> V3 {
        if (!exploded) return {0, 0, 0};
        double s = explodeAmt;
        if (id == "sample") return {-160 * s, 0, 0};
        if (id == "sleeve") return {-95 * s, 0, 0};
        if (id == "former" || id == "winding" || id == "ties") return {0, 0, 75 * s};
        if (id == "tripod") return {0, 0, -50 * s};
        if (id == "panel") return {0, 0, 50 * s};
        if (id == "dev") return {0, 0, -40 * s};
        return {0, 0, 0};
    };
    auto visible = [&](const std::string& id, const Mat& m) {
        if (id == "shell" || id == "hood") return false;            // default housing = laser-cut box
        std::string g = m.group;
        if (g == "wiring" && !wiring) return false;
        if (g == "pcb" && !pcb) return false;
        if (g == "holder" && !holder) return false;
        if (g == "housing" && (preset == "physics" || preset == "probe" || preset == "instrument")) return false;
        if (g == "wiring" && preset == "instrument") return false;
        if (g == "b0" && (preset == "probe" || preset == "exploded")) return false;
        if (g == "pcb" && preset == "probe") return false;
        return true;
    };

    // ---- field arrows + 90-degree marker (physics): B0 along +y, B1/coil axis along +x, at the sample
    Part arrowsB0{"__b0"}, arrowsB1{"__b1"}, marker{"__90"};
    if (fields) {
        V3 c = {0, 0, 33};
        add_arrow(arrowsB0, c - V3{0, 150, 0}, {0, 300, 0}, 3.0, 9, 22);   // B0 (y)
        add_arrow(arrowsB1, c - V3{150, 0, 0}, {300, 0, 0}, 3.0, 9, 22);   // B1 / coil axis (x)
        // small right-angle marker in the xy plane at the sample
        double s = 34;
        Part& M = marker; uint32_t b = 0;
        auto addq = [&](V3 p0, V3 p1, V3 p2, V3 p3) {
            uint32_t base = (uint32_t)(M.pos.size() / 3);
            for (V3 q : {p0, p1, p2, p3}) { M.pos.push_back((float)q.x); M.pos.push_back((float)q.y); M.pos.push_back((float)q.z); }
            for (uint32_t k : {0u, 1u, 2u, 0u, 2u, 3u}) M.idx.push_back(base + k);
        };
        double t = 3;
        addq(c + V3{0, 0, 0}, c + V3{s, 0, 0}, c + V3{s, t, 0}, c + V3{0, t, 0});   // along x
        addq(c + V3{0, 0, 0}, c + V3{0, s, 0}, c + V3{t, s, 0}, c + V3{t, 0, 0});   // along y
        (void)b;
    }

    // ---- compute framing from visible geometry bounds
    V3 lo = {1e30, 1e30, 1e30}, hi = {-1e30, -1e30, -1e30};
    for (auto& p : parts) {
        Mat m = material(p.id);
        if (!visible(p.id, m)) continue;
        V3 off = explodeOff(p.id);
        for (size_t i = 0; i < p.pos.size(); i += 3) {
            V3 v = {p.pos[i] + off.x, p.pos[i + 1] + off.y, p.pos[i + 2] + off.z};
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
    }
    V3 center = (lo + hi) * 0.5;
    if (preset == "probe") center = target;                       // keep probe centred, ignore stray bounds
    double radius = len(hi - lo) * 0.5;
    double asp = (double)W / H;

    // camera: spherical around target. Default z up. The physics preset uses y up so B0 (+y) reads screen-vertical and
    // B1 (+x) screen-horizontal (the trade-off: the Helmholtz rings are then seen edge-on rather than face-on).
    double ar = azim * 3.14159265 / 180, er = elev * 3.14159265 / 180;
    bool yUp = (preset == "physics");
    V3 dir = yUp ? V3{std::cos(er) * std::sin(ar), std::sin(er), std::cos(er) * std::cos(ar)}
                 : V3{std::cos(er) * std::cos(ar), std::cos(er) * std::sin(ar), std::sin(er)};
    V3 up = yUp ? V3{0, 1, 0} : V3{0, 0, 1};
    double D = (dist < 1e8) ? dist : radius * 3.0;
    V3 eye = center + dir * D;
    M4 view = lookAt(eye, center, up);
    M4 proj = persp ? perspective(vfov, asp, std::max(1.0, D - radius * 2), D + radius * 3)
                    : ortho(radius * fit * asp, radius * fit, -radius * 4, radius * 4);
    M4 VP = mul(proj, view);
    V3 light = norm({-0.35, -0.55, 0.75});

    FB fb(W, H, bg);
    // assemble a render list (parts + arrows), opaque first then transparent (coarse back-to-front by part centre)
    struct Item { Part* p; Mat m; V3 off; };
    std::vector<Item> items;
    for (auto& p : parts) { Mat m = material(p.id); if (visible(p.id, m)) items.push_back({&p, m, explodeOff(p.id)}); }
    if (fields) {
        items.push_back({&arrowsB0, {0.20, 0.45, 0.80, 1.0, "fx"}, {0, 0, 0}});
        items.push_back({&arrowsB1, {0.90, 0.53, 0.18, 1.0, "fx"}, {0, 0, 0}});
        items.push_back({&marker, {0.35, 0.35, 0.38, 1.0, "fx"}, {0, 0, 0}});
    }
    auto draw = [&](Item& it, bool writeDepth) {
        Part& p = *it.p; Mat& m = it.m;
        for (size_t t = 0; t + 2 < p.idx.size(); t += 3) {
            uint32_t ia = p.idx[t] * 3, ib = p.idx[t + 1] * 3, ic = p.idx[t + 2] * 3;
            V3 A = {p.pos[ia] + it.off.x, p.pos[ia + 1] + it.off.y, p.pos[ia + 2] + it.off.z};
            V3 B = {p.pos[ib] + it.off.x, p.pos[ib + 1] + it.off.y, p.pos[ib + 2] + it.off.z};
            V3 C = {p.pos[ic] + it.off.x, p.pos[ic + 1] + it.off.y, p.pos[ic + 2] + it.off.z};
            V3 n = norm(cross(B - A, C - A));
            double diff = std::fabs(dot(n, light));
            double sh = 0.30 + 0.70 * diff;                       // two-sided lambert + ambient
            double ca[4], cb[4], cc[4];
            mulVec4(VP, A.x, A.y, A.z, 1, ca); mulVec4(VP, B.x, B.y, B.z, 1, cb); mulVec4(VP, C.x, C.y, C.z, 1, cc);
            tri(fb, {ca[0], ca[1], ca[2], ca[3]}, {cb[0], cb[1], cb[2], cb[3]}, {cc[0], cc[1], cc[2], cc[3]},
                m.r * sh, m.g * sh, m.b * sh, m.a, writeDepth);
        }
    };
    for (auto& it : items) if (it.m.a >= 0.999) draw(it, true);
    // transparent, sorted far->near by centre distance to eye
    std::vector<Item*> tr;
    for (auto& it : items) if (it.m.a < 0.999) tr.push_back(&it);
    std::sort(tr.begin(), tr.end(), [&](Item* a, Item* b) {
        auto cen = [&](Item* it) { V3 s{0, 0, 0}; size_t n = it->p->pos.size() / 3; for (size_t i = 0; i < it->p->pos.size(); i += 3) s = s + V3{it->p->pos[i], it->p->pos[i + 1], it->p->pos[i + 2]}; return n ? s * (1.0 / n) + it->off : it->off; };
        return len(cen(a) - eye) > len(cen(b) - eye);
    });
    for (auto* it : tr) draw(*it, false);

    // to 8-bit RGB
    std::vector<uint8_t> px(W * H * 3);
    for (int i = 0; i < W * H * 3; ++i) px[i] = (uint8_t)std::round(std::min(1.0, std::max(0.0, (double)fb.col[i])) * 255);
    std::string dir_out = out.substr(0, out.find_last_of("/\\"));
    if (!dir_out.empty() && dir_out != out) { std::string cmd = "mkdir \"" + dir_out + "\" 2>nul"; std::system(cmd.c_str()); }
    if (!stbi_write_png(out.c_str(), W, H, 3, px.data(), W * 3)) { std::fprintf(stderr, "failed to write %s\n", out.c_str()); return 3; }

    std::printf("preset: %s\nprojection: %s\nresolution: %dx%d\ncamera: az %.0f el %.0f fov %.0f dist %.0f\ntarget: [%.0f %.0f %.0f]\noutput: %s\nstatus: OK\n",
                preset.c_str(), persp ? "perspective" : "orthographic", W, H, azim, elev, vfov, D, center.x, center.y, center.z, out.c_str());
    return 0;
}
