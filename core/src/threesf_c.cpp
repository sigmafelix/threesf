#include "threesf/threesf_c.h"
#include "threesf/threesf.hpp"
#include "threesf/io.hpp"
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <string>

using namespace threesf;
static const double NaN = std::numeric_limits<double>::quiet_NaN();

struct threesf_handle { Geometry g; };

namespace {
thread_local std::string g_last_error;
thread_local std::string g_last_warning;
void record_warning(const char* message) { g_last_warning = message; }
struct ApiScope {
    WarningHandler previous = warning_handler;
    ApiScope() { g_last_error.clear(); g_last_warning.clear(); warning_handler = record_warning; }
    ~ApiScope() { warning_handler = previous; }
};
void set_error(const std::string& s) { g_last_error = s; }

// ---- flat-array decoding --------------------------------------------------
Vec3 at(const threesf_geom* g, size_t i) { return {g->coords[3 * i], g->coords[3 * i + 1], g->coords[3 * i + 2]}; }
Ring ring(const threesf_geom* g, size_t r) {
    Ring out;
    for (size_t i = g->ring_starts[r]; i < g->ring_starts[r + 1]; ++i) out.push_back(at(g, i));
    return out;
}
Polygon polygon(const threesf_geom* g, size_t p) {
    Polygon out;
    size_t r0 = g->patch_starts[p], r1 = g->patch_starts[p + 1];
    if (r1 <= r0) return out;
    out.exterior = ring(g, r0);
    for (size_t r = r0 + 1; r < r1; ++r) out.holes.push_back(ring(g, r));
    return out;
}
PolyhedralSurface surface(const threesf_geom* g, size_t p0, size_t p1) {
    PolyhedralSurface s;
    for (size_t p = p0; p < p1; ++p) s.patches.push_back(polygon(g, p));
    return s;
}
bool valid(const threesf_geom* g) {
    if (!g || (!g->coords && g->n_coords > 0)) return false;
    switch (g->type) {
        case THREESF_POINT: return true;
        case THREESF_LINESTRING: return g->ring_starts != nullptr;
        case THREESF_POLYGON: case THREESF_SURFACE: return g->ring_starts && g->patch_starts;
        case THREESF_SOLID: return g->ring_starts && g->patch_starts && g->shell_starts;
        default: return false;
    }
}
bool decode(const threesf_geom* g, Geometry& out) {
    if (!valid(g)) { set_error("invalid flat geometry"); return false; }
    switch (g->type) {
        case THREESF_POINT:
            out.type = g->n_coords == 1 ? GeomType::Point : GeomType::MultiPoint;
            for (size_t i = 0; i < g->n_coords; ++i) out.points.push_back(at(g, i));
            return true;
        case THREESF_LINESTRING:
            out.type = g->n_rings == 1 ? GeomType::LineString : GeomType::MultiLineString;
            for (size_t r = 0; r < g->n_rings; ++r) { LineString l; l.pts = ring(g, r); out.lines.push_back(std::move(l)); }
            return true;
        case THREESF_POLYGON:
            out.type = g->n_patches == 1 ? GeomType::Polygon : GeomType::MultiPolygon;
            for (size_t p = 0; p < g->n_patches; ++p) out.polygons.push_back(polygon(g, p));
            return true;
        case THREESF_SURFACE:
            out.type = GeomType::PolyhedralSurface;
            out.surfaces.push_back(surface(g, 0, g->n_patches));
            return true;
        case THREESF_SOLID: {
            if (g->n_shells == 0) { set_error("solid needs at least one shell"); return false; }
            out.type = GeomType::Solid;
            Solid s;
            s.outer = surface(g, g->shell_starts[0], g->shell_starts[1]);
            for (size_t sh = 1; sh < g->n_shells; ++sh) s.inner.push_back(surface(g, g->shell_starts[sh], g->shell_starts[sh + 1]));
            out.solids.push_back(std::move(s));
            return true;
        }
    }
    return false;
}

// ---- shared operations on Geometry -----------------------------------------
void collect_surface(const Geometry& g, PolyhedralSurface& s) {
    for (auto& p : g.polygons) s.patches.push_back(p);
    for (auto& x : g.surfaces) s.patches.insert(s.patches.end(), x.patches.begin(), x.patches.end());
    for (auto& so : g.solids) { s.patches.insert(s.patches.end(), so.outer.patches.begin(), so.outer.patches.end()); for (auto& in : so.inner) s.patches.insert(s.patches.end(), in.patches.begin(), in.patches.end()); }
    for (auto& c : g.children) collect_surface(c, s);
}
const PolyhedralSurface* first_shell(const Geometry& g) {
    if (!g.solids.empty()) return &g.solids[0].outer;
    if (!g.surfaces.empty()) return &g.surfaces[0];
    for (auto& c : g.children) if (auto* s = first_shell(c)) return s;
    return nullptr;
}
const Polygon* first_polygon(const Geometry& g) {
    if (!g.polygons.empty()) return &g.polygons[0];
    if (!g.surfaces.empty() && !g.surfaces[0].patches.empty()) return &g.surfaces[0].patches[0];
    for (auto& c : g.children) if (auto* p = first_polygon(c)) return p;
    return nullptr;
}
void collect_tris(const Geometry& g, std::vector<Triangle>& out) {
    auto add = [&](const std::vector<Triangle>& t) { out.insert(out.end(), t.begin(), t.end()); };
    for (auto& p : g.polygons) add(tessellate(p));
    for (auto& s : g.surfaces) add(tessellate(s));
    for (auto& s : g.solids) { add(tessellate(s.outer)); for (auto& in : s.inner) add(tessellate(in)); }
    for (auto& c : g.children) collect_tris(c, out);
}
Geometry tin_from(const std::vector<Triangle>& tris, int srid) {
    Geometry g; g.type = GeomType::Tin; g.srid = srid;
    PolyhedralSurface s;
    for (auto& t : tris) { Polygon p; p.exterior = {t.a, t.b, t.c, t.a}; s.patches.push_back(std::move(p)); }
    g.surfaces.push_back(std::move(s));
    return g;
}

double op_area(const Geometry& G) {
    double a = area(G);
    std::function<void(const Geometry&)> rec = [&](const Geometry& g) { for (auto& s : g.solids) a += area(s); for (auto& c : g.children) rec(c); };
    rec(G);
    return a;
}
double op_area_tess(const Geometry& G) { PolyhedralSurface s; collect_surface(G, s); double a = 0; for (auto& p : s.patches) a += area_tessellated(p); return a; }
double op_volume(const Geometry& G) {
    if (G.type == GeomType::PolyhedralSurface || G.type == GeomType::Tin) {
        if (G.surfaces.empty() || !is_closed(G.surfaces[0])) { set_error("volume: surface is not closed"); return NaN; }
        return volume(G.surfaces[0]);
    }
    return volume(G);
}
double op_signed_volume(const Geometry& G) { auto* s = first_shell(G); if (!s) { set_error("signed volume: no surface"); return NaN; } return signed_volume(*s); }
int op_centroid(const Geometry& G, double out[3]) {
    Vec3 c; double w = 0;
    std::function<void(const Geometry&)> rec = [&](const Geometry& g) {
        for (auto& s : g.solids) { double v = volume(s); c += centroid(s) * v; w += v; }
        for (auto& c2 : g.children) rec(c2);
    };
    rec(G);
    if (w == 0) {  // no solids: area-weighted
        PolyhedralSurface s; collect_surface(G, s);
        for (auto& t : tessellate(s)) { double a = area(t); c += (t.a + t.b + t.c) * (a / 3.0); w += a; }
    }
    if (w == 0) {  // no area: length-weighted
        std::function<void(const Geometry&)> rl = [&](const Geometry& g) { for (auto& l : g.lines) { double L = length(l); c += centroid(l) * L; w += L; } for (auto& c2 : g.children) rl(c2); };
        rl(G);
    }
    if (w == 0) {  // points
        std::function<void(const Geometry&)> rp = [&](const Geometry& g) { for (auto& p : g.points) { c += p; w += 1; } for (auto& c2 : g.children) rp(c2); };
        rp(G);
    }
    if (w == 0) { set_error("centroid: empty geometry"); return -1; }
    c = c / w; out[0] = c.x; out[1] = c.y; out[2] = c.z;
    return 0;
}
int op_bbox(const Geometry& G, double out[6]) {
    Box3 b = bbox(G); if (b.empty()) { set_error("bbox: empty geometry"); return -1; }
    out[0] = b.min.x; out[1] = b.min.y; out[2] = b.min.z; out[3] = b.max.x; out[4] = b.max.y; out[5] = b.max.z;
    return 0;
}
int op_is_planar(const Geometry& G, double tol) { PolyhedralSurface s; collect_surface(G, s); if (s.patches.empty()) { set_error("is_planar: no polygons"); return -1; } return is_planar(s, tol) ? 1 : 0; }
int op_is_closed(const Geometry& G) { auto* s = first_shell(G); if (!s) { set_error("is_closed: no surface"); return -1; } return is_closed(*s) ? 1 : 0; }
int op_is_solid(const Geometry& G) {
    if (!G.solids.empty()) return is_solid(G.solids[0]) ? 1 : 0;
    auto* s = first_shell(G); if (!s) { set_error("is_solid: no surface"); return -1; }
    return is_solid(*s) ? 1 : 0;
}
int op_orientation(const Geometry& G) { auto* p = first_polygon(G); return p ? orientation(*p) : 0; }
template <class R> int op_pair(const R& r, double out[6]) { out[0] = r.a.x; out[1] = r.a.y; out[2] = r.a.z; out[3] = r.b.x; out[4] = r.b.y; out[5] = r.b.z; return 0; }

int emit_triangles(const std::vector<Triangle>& tris, double** out, size_t* n) {
    *n = tris.size();
    *out = static_cast<double*>(std::malloc(sizeof(double) * 9 * std::max<size_t>(tris.size(), 1)));
    if (!*out) return -1;
    for (size_t i = 0; i < tris.size(); ++i) {
        const Vec3* v[3] = {&tris[i].a, &tris[i].b, &tris[i].c};
        for (int k = 0; k < 3; ++k) { (*out)[9 * i + 3 * k] = v[k]->x; (*out)[9 * i + 3 * k + 1] = v[k]->y; (*out)[9 * i + 3 * k + 2] = v[k]->z; }
    }
    return 0;
}
char* dup_string(const std::string& s) { char* p = static_cast<char*>(std::malloc(s.size() + 1)); if (p) std::memcpy(p, s.c_str(), s.size() + 1); return p; }
threesf_handle* wrap(Geometry g) { return new threesf_handle{std::move(g)}; }
threesf_handle* wrap_input(Geometry g) {
    if (self_intersects(g)) detail::warn("self-intersection found in input geometry");
    return wrap(std::move(g));
}
}  // namespace

extern "C" {

const char* threesf_version(void) { return threesf::version(); }
const char* threesf_last_error(void) { return g_last_error.c_str(); }
const char* threesf_last_warning(void) { return g_last_warning.c_str(); }
void threesf_free(void* p) { std::free(p); }

// ---------------------------------------------------------------------------
// Flat-array API
// ---------------------------------------------------------------------------
double threesf_length(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? length(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_perimeter(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? perimeter(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_area(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_area(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_area_tessellated(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_area_tess(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_volume(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_volume(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_signed_volume(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_signed_volume(G) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
int threesf_centroid(const threesf_geom* g, double out[3]) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_centroid(G, out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_bbox(const threesf_geom* g, double out[6]) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_bbox(G, out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_is_planar(const threesf_geom* g, double tol) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_is_planar(G, tol) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_is_closed(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_is_closed(G) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_is_solid(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_is_solid(G) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_orientation(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? op_orientation(G) : 0; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
double threesf_distance(const threesf_geom* a, const threesf_geom* b) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? distance(A, B) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_max_distance(const threesf_geom* a, const threesf_geom* b) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? max_distance(A, B) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
int threesf_closest_points(const threesf_geom* a, const threesf_geom* b, double out[6]) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? op_pair(closest_points(A, B), out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_farthest_points(const threesf_geom* a, const threesf_geom* b, double out[6]) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? op_pair(farthest_points(A, B), out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_dwithin(const threesf_geom* a, const threesf_geom* b, double d) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? (dwithin(A, B, d) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_dfully_within(const threesf_geom* a, const threesf_geom* b, double d) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? (dfully_within(A, B, d) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_intersects(const threesf_geom* a, const threesf_geom* b, double eps) {
    ApiScope scope; try { Geometry A, B; return decode(a, A) && decode(b, B) ? (intersects(A, B, eps) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_tessellate(const threesf_geom* g, double** tris, size_t* n_tris) {
    ApiScope scope; try {
        Geometry G; if (!decode(g, G)) return -1;
        if (self_intersects(G)) detail::reject_self_intersection();
        std::vector<Triangle> out; collect_tris(G, out);
        return emit_triangles(out, tris, n_tris);
    } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_extrude(const threesf_geom* g, double dx, double dy, double dz, double** tris, size_t* n_tris) {
    ApiScope scope; try {
        Geometry G; if (!decode(g, G) || G.polygons.empty()) return -1;
        Solid s = extrude(G.polygons[0], {dx, dy, dz});
        return emit_triangles(tessellate(s.outer), tris, n_tris);
    } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_force_outward(const threesf_geom* g, double** tris, size_t* n_tris) {
    ApiScope scope; try {
        Geometry G; if (!decode(g, G)) return -1;
        auto* s = first_shell(G); if (!s) return -1;
        return emit_triangles(tessellate(force_outward(*s)), tris, n_tris);
    } catch (const std::exception& e) { set_error(e.what()); return -1; }
}

// ---------------------------------------------------------------------------
// Handle API
// ---------------------------------------------------------------------------
threesf_handle* threesf_read_wkt(const char* wkt) {
    ApiScope scope; try { if (!wkt) { set_error("null WKT"); return nullptr; } return wrap_input(read_wkt(wkt)); } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_read_wkb(const unsigned char* wkb, size_t len) {
    ApiScope scope; try { if (!wkb) { set_error("null WKB"); return nullptr; } return wrap_input(read_wkb(wkb, len)); } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_read_hexwkb(const char* hex) {
    ApiScope scope; try { if (!hex) { set_error("null hex"); return nullptr; } return wrap_input(read_hexwkb(hex)); } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_from_flat(const threesf_geom* g) {
    ApiScope scope; try { Geometry G; return decode(g, G) ? wrap_input(std::move(G)) : nullptr; } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
void threesf_h_free(threesf_handle* h) { delete h; }
threesf_handle* threesf_h_clone(const threesf_handle* h) {
    ApiScope scope; try { return h ? wrap(h->g) : nullptr; } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}

char* threesf_h_wkt(const threesf_handle* h, int ewkt) {
    ApiScope scope; try { return h ? dup_string(write_wkt(h->g, ewkt != 0)) : nullptr; } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
unsigned char* threesf_h_wkb(const threesf_handle* h, int ewkb, size_t* len) {
    ApiScope scope; try {
        if (!h) return nullptr;
        auto v = write_wkb(h->g, ewkb != 0);
        auto* p = static_cast<unsigned char*>(std::malloc(std::max<size_t>(v.size(), 1)));
        if (p) std::memcpy(p, v.data(), v.size());
        *len = v.size();
        return p;
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
char* threesf_h_hexwkb(const threesf_handle* h, int ewkb) {
    ApiScope scope; try { return h ? dup_string(write_hexwkb(h->g, ewkb != 0)) : nullptr; } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
const char* threesf_h_type(const threesf_handle* h) {
    ApiScope scope; try { return h ? type_name(h->g.type) : ""; } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
int threesf_h_srid(const threesf_handle* h) {
    ApiScope scope; try { return h ? h->g.srid : 0; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
void threesf_h_set_srid(threesf_handle* h, int srid) {
    ApiScope scope; try { if (h) h->g.srid = srid; } catch (const std::exception& e) { set_error(e.what()); return; }
}
int threesf_h_is_empty(const threesf_handle* h) {
    ApiScope scope; try { return h ? (h->g.empty() ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
size_t threesf_h_num_vertices(const threesf_handle* h) {
    ApiScope scope; try { if (!h) return 0; return primitives(h->g).vertices.size(); } catch (const std::exception& e) { set_error(e.what()); return 0; }
}

double threesf_h_length(const threesf_handle* g) {
    ApiScope scope; try { return g ? length(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_perimeter(const threesf_handle* g) {
    ApiScope scope; try { return g ? perimeter(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_area(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_area(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_area_tessellated(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_area_tess(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_volume(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_volume(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_signed_volume(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_signed_volume(g->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
int threesf_h_centroid(const threesf_handle* g, double out[3]) {
    ApiScope scope; try { return g ? op_centroid(g->g, out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_bbox(const threesf_handle* g, double out[6]) {
    ApiScope scope; try { return g ? op_bbox(g->g, out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_is_planar(const threesf_handle* g, double tol) {
    ApiScope scope; try { return g ? op_is_planar(g->g, tol) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_is_closed(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_is_closed(g->g) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_is_solid(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_is_solid(g->g) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_orientation(const threesf_handle* g) {
    ApiScope scope; try { return g ? op_orientation(g->g) : 0; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
double threesf_h_distance(const threesf_handle* a, const threesf_handle* b) {
    ApiScope scope; try { return a && b ? distance(a->g, b->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
double threesf_h_max_distance(const threesf_handle* a, const threesf_handle* b) {
    ApiScope scope; try { return a && b ? max_distance(a->g, b->g) : NaN; } catch (const std::exception& e) { set_error(e.what()); return NaN; }
}
int threesf_h_closest_points(const threesf_handle* a, const threesf_handle* b, double out[6]) {
    ApiScope scope; try { return a && b ? op_pair(closest_points(a->g, b->g), out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_farthest_points(const threesf_handle* a, const threesf_handle* b, double out[6]) {
    ApiScope scope; try { return a && b ? op_pair(farthest_points(a->g, b->g), out) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_dwithin(const threesf_handle* a, const threesf_handle* b, double d) {
    ApiScope scope; try { return a && b ? (dwithin(a->g, b->g, d) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_dfully_within(const threesf_handle* a, const threesf_handle* b, double d) {
    ApiScope scope; try { return a && b ? (dfully_within(a->g, b->g, d) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
int threesf_h_intersects(const threesf_handle* a, const threesf_handle* b, double eps) {
    ApiScope scope; try { return a && b ? (intersects(a->g, b->g, eps) ? 1 : 0) : -1; } catch (const std::exception& e) { set_error(e.what()); return -1; }
}

threesf_handle* threesf_h_tessellate(const threesf_handle* g) {
    ApiScope scope; try {
        if (!g) return nullptr;
        if (self_intersects(g->g)) detail::reject_self_intersection();
        std::vector<Triangle> tris; collect_tris(g->g, tris);
        if (tris.empty()) { set_error("tessellate: no surface content"); return nullptr; }
        return wrap(tin_from(tris, g->g.srid));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_extrude(const threesf_handle* g, double dx, double dy, double dz) {
    ApiScope scope; try {
        if (!g) return nullptr;
        auto* p = first_polygon(g->g); if (!p) { set_error("extrude: no polygon"); return nullptr; }
        Geometry out; out.type = GeomType::Solid; out.srid = g->g.srid;
        out.solids.push_back(extrude(*p, {dx, dy, dz}));
        return wrap(std::move(out));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_make_solid(const threesf_handle* g) {
    ApiScope scope; try {
        if (!g) return nullptr;
        auto* s = first_shell(g->g); if (!s) { set_error("make_solid: no surface"); return nullptr; }
        Solid so; if (!make_solid(*s, so)) { set_error("make_solid: surface is not closed or consistently oriented"); return nullptr; }
        Geometry out; out.type = GeomType::Solid; out.srid = g->g.srid; out.solids.push_back(std::move(so));
        return wrap(std::move(out));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_force_outward(const threesf_handle* g) {
    ApiScope scope; try {
        if (!g) return nullptr;
        Geometry out = g->g;
        if (!out.solids.empty()) { out.solids[0].outer = force_outward(out.solids[0].outer); for (auto& in : out.solids[0].inner) in = force_inward(in); }
        else if (!out.surfaces.empty()) out.surfaces[0] = force_outward(out.surfaces[0]);
        else { set_error("force_outward: no surface"); return nullptr; }
        return wrap(std::move(out));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_force_ccw(const threesf_handle* g) {
    ApiScope scope; try {
        if (!g) return nullptr;
        Geometry out = g->g;
        for (auto& p : out.polygons) p = force_ccw(p);
        for (auto& s : out.surfaces) for (auto& p : s.patches) p = force_ccw(p);
        return wrap(std::move(out));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_force_cw(const threesf_handle* g) {
    ApiScope scope; try {
        if (!g) return nullptr;
        Geometry out = g->g;
        for (auto& p : out.polygons) p = force_cw(p);
        for (auto& s : out.surfaces) for (auto& p : s.patches) p = force_cw(p);
        return wrap(std::move(out));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}

int threesf_h_self_intersects(const threesf_handle* g, double tol) {
    ApiScope scope; try {
        if (!g) throw std::invalid_argument("null geometry");
        bool found = self_intersects(g->g, tol);
        if (found) detail::warn("self-intersection found in input geometry");
        return found ? 1 : 0;
    } catch (const std::exception& e) { set_error(e.what()); return -1; }
}
threesf_handle* threesf_h_convex_hull(const threesf_handle* g, double tol) {
    ApiScope scope; try {
        if (!g) throw std::invalid_argument("null geometry");
        return wrap(convex_hull(g->g, tol));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_intersection(const threesf_handle* a, const threesf_handle* b, double tol) {
    ApiScope scope; try {
        if (!a || !b) throw std::invalid_argument("null geometry");
        return wrap(intersection(a->g, b->g, tol));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_union(const threesf_handle* a, const threesf_handle* b, double tol) {
    ApiScope scope; try {
        if (!a || !b) throw std::invalid_argument("null geometry");
        return wrap(union_(a->g, b->g, tol));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}
threesf_handle* threesf_h_difference(const threesf_handle* a, const threesf_handle* b, double tol) {
    ApiScope scope; try {
        if (!a || !b) throw std::invalid_argument("null geometry");
        return wrap(difference(a->g, b->g, tol));
    } catch (const std::exception& e) { set_error(e.what()); return nullptr; }
}

int threesf_h_plot_data(const threesf_handle* g, int edges, double** data, size_t counts[3]) {
    ApiScope scope; try {
        if (!data || !counts) throw std::invalid_argument("plot: null output buffer");
        *data = nullptr; counts[0] = counts[1] = counts[2] = 0;
        if (!g) throw std::invalid_argument("plot: null geometry");
        if (self_intersects(g->g)) detail::warn("self-intersection found in plotted geometry");
        auto p = primitives(g->g);
        if (!edges) {
            p.segments.clear();
            std::function<void(const Geometry&)> curves = [&](const Geometry& item) {
                for (auto& line : item.lines)
                    for (size_t i = 1; i < line.pts.size(); ++i) p.segments.push_back({line.pts[i-1], line.pts[i]});
                for (auto& child : item.children) curves(child);
            };
            curves(g->g);
        }
        std::vector<double> xyz;
        auto add = [&](const Vec3& v) {
            if (!detail::finite(v)) throw std::invalid_argument("plot: non-finite coordinate");
            xyz.insert(xyz.end(), {v.x, v.y, v.z});
        };
        for (auto& v : p.points) add(v);
        for (auto& e : p.segments) { add(e.first); add(e.second); }
        for (auto& t : p.triangles) { add(t.a); add(t.b); add(t.c); }
        if (!xyz.empty()) {
            *data = static_cast<double*>(std::malloc(xyz.size() * sizeof(double)));
            if (!*data) throw std::bad_alloc();
            std::memcpy(*data, xyz.data(), xyz.size() * sizeof(double));
        }
        counts[0] = p.points.size(); counts[1] = p.segments.size(); counts[2] = p.triangles.size();
        return 0;
    } catch (const std::exception& e) { set_error(e.what()); return -1; }
}

}  // extern "C"
