// threesf — header-only 3D vector geometry measures (PostGIS/SFCGAL-style)
// C++17, no external dependencies.
//
// Coverage (PostGIS equivalent in brackets):
//   length          [ST_3DLength]        perimeter        [ST_3DPerimeter]
//   area            [ST_3DArea]          volume           [ST_Volume]
//   centroid        [ST_3DCentroid*]     bbox             [ST_3DExtent]
//   distance        [ST_3DDistance]      max_distance     [ST_3DMaxDistance]
//   closest_points  [ST_3DClosestPoint / ST_3DShortestLine / ST_3DLongestLine]
//   dwithin         [ST_3DDWithin]       dfully_within    [ST_3DDFullyWithin]
//   intersects      [ST_3DIntersects]    is_planar        [ST_IsPlanar]
//   is_closed       [ST_IsClosed]        is_solid         [ST_IsSolid]
//   make_solid      [ST_MakeSolid]       orientation      [ST_Orientation]
//   force_ccw/cw    [ST_ForceLHR/RHR]    force_outward    [—]
//   tessellate      [ST_Tesselate]       extrude          [ST_Extrude]
//
// Conventions: rings may or may not repeat the first vertex; both accepted.
// A PolyhedralSurface is a list of polygon patches (a TIN is a surface whose
// patches are triangles). A Solid has one outer shell and zero or more inner
// shells (cavities). Volume is unsigned; signed_volume exposes orientation.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace threesf {

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------
struct Vec3 {
    double x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3& o) const { return !(*this == o); }
};
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double norm2(const Vec3& a) { return dot(a, a); }
inline double norm(const Vec3& a) { return std::sqrt(norm2(a)); }
inline double dist(const Vec3& a, const Vec3& b) { return norm(a - b); }
inline Vec3 normalize(const Vec3& a) { double n = norm(a); return n > 0 ? a / n : a; }

using Ring = std::vector<Vec3>;

struct LineString { std::vector<Vec3> pts; };
struct Polygon { Ring exterior; std::vector<Ring> holes; };
struct Triangle { Vec3 a, b, c; };
struct PolyhedralSurface { std::vector<Polygon> patches; };
struct Solid { PolyhedralSurface outer; std::vector<PolyhedralSurface> inner; };

struct Box3 {
    Vec3 min{ std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity()};
    Vec3 max{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    bool empty() const { return min.x > max.x; }
    void extend(const Vec3& p) {
        min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
        max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
    }
    void extend(const Box3& b) { if (!b.empty()) { extend(b.min); extend(b.max); } }
    Box3 expanded(double d) const { Box3 b = *this; b.min = b.min - Vec3{d, d, d}; b.max = b.max + Vec3{d, d, d}; return b; }
    bool intersects(const Box3& o) const {
        if (empty() || o.empty()) return false;
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
};

// Simple Features geometry types (Z variants; SOLID/MULTISOLID are SFCGAL extensions).
enum class GeomType {
    Point, MultiPoint, LineString, MultiLineString, Polygon, Triangle, MultiPolygon,
    PolyhedralSurface, Tin, Solid, MultiSolid, GeometryCollection
};
inline const char* type_name(GeomType t) {
    static const char* names[] = {"POINT", "MULTIPOINT", "LINESTRING", "MULTILINESTRING", "POLYGON", "TRIANGLE",
                                  "MULTIPOLYGON", "POLYHEDRALSURFACE", "TIN", "SOLID", "MULTISOLID", "GEOMETRYCOLLECTION"};
    return names[static_cast<int>(t)];
}

// Typed geometry tree. The storage member used depends on `type`:
//   Point/MultiPoint -> points; LineString/MultiLineString -> lines;
//   Polygon/Triangle/MultiPolygon -> polygons; PolyhedralSurface/Tin -> surfaces[0];
//   Solid/MultiSolid -> solids; GeometryCollection -> children.
// Measures accept any Geometry and recurse through children, so the struct also
// serves as an ad-hoc collection (default type is GeometryCollection).
struct Geometry {
    GeomType type = GeomType::GeometryCollection;
    int srid = 0;
    std::vector<Vec3> points;
    std::vector<LineString> lines;
    std::vector<Polygon> polygons;
    std::vector<PolyhedralSurface> surfaces;
    std::vector<Solid> solids;
    std::vector<Geometry> children;
    bool empty() const {
        return points.empty() && lines.empty() && polygons.empty() && surfaces.empty() && solids.empty() && children.empty();
    }
};

// Warnings default to stderr. A thread-local handler lets language bindings
// report them through their native warning mechanism instead.
using WarningHandler = void (*)(const char*);
inline thread_local WarningHandler warning_handler = nullptr;
namespace detail {
inline void warn(const char* message) {
    if (warning_handler) warning_handler(message);
#ifndef THREESF_R_BUILD
    else std::fprintf(stderr, "threesf warning: %s\n", message);
#endif
}
[[noreturn]] inline void reject_self_intersection() {
    const char* message = "self-intersection found; operation refused";
    warn(message);
    throw std::invalid_argument(message);
}
}
inline bool self_intersects(const Polygon& p, double tol = 1e-9);
inline bool self_intersects(const PolyhedralSurface& s, double tol = 1e-9);
inline bool self_intersects(const Solid& s, double tol = 1e-9);
inline bool self_intersects(const Geometry& g, double tol = 1e-9);

// ---------------------------------------------------------------------------
// Ring helpers
// ---------------------------------------------------------------------------
namespace detail {
// Number of distinct vertices, ignoring a repeated closing vertex.
inline std::size_t ring_size(const Ring& r) {
    if (r.size() >= 2 && r.front() == r.back()) return r.size() - 1;
    return r.size();
}
inline Ring open_ring(const Ring& r) { return Ring(r.begin(), r.begin() + ring_size(r)); }
inline Ring closed_ring(const Ring& r) { Ring o = open_ring(r); if (!o.empty()) o.push_back(o.front()); return o; }
}  // namespace detail

// ---------------------------------------------------------------------------
// Plane geometry: Newell normal, planarity, projection basis
// ---------------------------------------------------------------------------
// Newell's method. Returns an un-normalised normal whose magnitude equals
// twice the area of the polygon projected onto its best-fit plane. Works for
// concave and (slightly) non-planar rings, and orientation follows winding.
inline Vec3 newell_normal(const Ring& ring) {
    Vec3 n;
    std::size_t m = detail::ring_size(ring);
    Vec3 origin = m ? ring[0] : Vec3{};
    for (std::size_t i = 0; i < m; ++i) {
        Vec3 p = ring[i] - origin;
        Vec3 q = ring[(i + 1) % m] - origin;
        n.x += (p.y - q.y) * (p.z + q.z);
        n.y += (p.z - q.z) * (p.x + q.x);
        n.z += (p.x - q.x) * (p.y + q.y);
    }
    return n;
}
inline Vec3 unit_normal(const Polygon& poly) { return normalize(newell_normal(poly.exterior)); }

inline Vec3 ring_centroid_vertices(const Ring& ring) {
    Vec3 c; std::size_t m = detail::ring_size(ring);
    for (std::size_t i = 0; i < m; ++i) c += ring[i];
    return m ? c / double(m) : c;
}

// Orthonormal basis (u, v) spanning the plane with normal n (n need not be unit).
inline std::pair<Vec3, Vec3> plane_basis(const Vec3& n_in) {
    Vec3 n = normalize(n_in);
    Vec3 helper = std::fabs(n.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    Vec3 u = normalize(cross(helper, n));
    Vec3 v = cross(n, u);
    return {u, v};
}

// Maximum absolute distance of any vertex from the polygon's best-fit plane.
inline double plane_deviation(const Polygon& poly) {
    Vec3 n = unit_normal(poly);
    if (norm2(n) == 0) return 0.0;
    Vec3 c = ring_centroid_vertices(poly.exterior);
    double d = 0;
    auto scan = [&](const Ring& r) { for (const Vec3& p : r) d = std::max(d, std::fabs(dot(p - c, n))); };
    scan(poly.exterior);
    for (const Ring& h : poly.holes) scan(h);
    return d;
}
inline bool is_planar(const Polygon& poly, double tol = 1e-9) { return plane_deviation(poly) <= tol; }
inline bool is_planar(const PolyhedralSurface& s, double tol = 1e-9) {
    for (const Polygon& p : s.patches) if (!is_planar(p, tol)) return false;
    return true;
}

// Orientation of the exterior ring, viewed from +Z (falls back to +Y, +X for
// vertical polygons). +1 = counter-clockwise, -1 = clockwise, 0 = degenerate.
// Mirrors ST_Orientation for the 2D case.
inline int orientation(const Polygon& poly) {
    Vec3 n = newell_normal(poly.exterior);
    double s = n.z != 0 ? n.z : (n.y != 0 ? n.y : n.x);
    return s > 0 ? 1 : (s < 0 ? -1 : 0);
}
// Orientation relative to an arbitrary reference normal: +1 if the ring winds
// counter-clockwise when viewed from the tip of `ref`.
inline int orientation(const Ring& ring, const Vec3& ref) {
    double s = dot(newell_normal(ring), ref);
    return s > 0 ? 1 : (s < 0 ? -1 : 0);
}

// Force exterior CCW / holes CW relative to `ref` (default: the polygon's own
// +Z-ish orientation as in ST_ForcePolygonCCW). force_cw is the mirror.
inline Polygon force_ccw(Polygon poly, const Vec3& ref = {0, 0, 1}) {
    if (orientation(poly.exterior, ref) < 0) std::reverse(poly.exterior.begin(), poly.exterior.end());
    for (Ring& h : poly.holes) if (orientation(h, ref) > 0) std::reverse(h.begin(), h.end());
    return poly;
}
inline Polygon force_cw(Polygon poly, const Vec3& ref = {0, 0, 1}) {
    if (orientation(poly.exterior, ref) > 0) std::reverse(poly.exterior.begin(), poly.exterior.end());
    for (Ring& h : poly.holes) if (orientation(h, ref) < 0) std::reverse(h.begin(), h.end());
    return poly;
}
inline Polygon reversed(Polygon poly) {
    std::reverse(poly.exterior.begin(), poly.exterior.end());
    for (Ring& h : poly.holes) std::reverse(h.begin(), h.end());
    return poly;
}

// ---------------------------------------------------------------------------
// Tessellation (ear clipping in the best-fit plane, holes bridged)
// ---------------------------------------------------------------------------
namespace detail {
struct V2 { double x, y; std::size_t idx; };  // idx -> 3D vertex

inline double cross2(const V2& o, const V2& a, const V2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}
inline double signed_area2(const std::vector<V2>& r) {
    double a = 0; std::size_t m = r.size();
    for (std::size_t i = 0; i < m; ++i) { const V2& p = r[i]; const V2& q = r[(i + 1) % m]; a += p.x * q.y - q.x * p.y; }
    return 0.5 * a;
}
inline bool point_in_tri(const V2& p, const V2& a, const V2& b, const V2& c) {
    double d1 = cross2(a, b, p), d2 = cross2(b, c, p), d3 = cross2(c, a, p);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}
inline bool point_in_ring(const V2& p, const std::vector<V2>& ring) {
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[i]; const auto& b = ring[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < a.x + (b.x-a.x)*(p.y-a.y)/(b.y-a.y)) inside = !inside;
    }
    return inside;
}
inline bool seg_intersect(const V2& p1, const V2& p2, const V2& q1, const V2& q2) {
    double d1 = cross2(q1, q2, p1), d2 = cross2(q1, q2, p2), d3 = cross2(p1, p2, q1), d4 = cross2(p1, p2, q2);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

// Project a ring into plane coordinates (u, v); `base` is the running index offset.
inline std::vector<V2> project(const Ring& r, const Vec3& origin, const Vec3& u, const Vec3& v, std::size_t base) {
    std::vector<V2> out; std::size_t m = ring_size(r);
    out.reserve(m);
    for (std::size_t i = 0; i < m; ++i) {
        Vec3 d = r[i] - origin;
        out.push_back({dot(d, u), dot(d, v), base + i});
    }
    return out;
}

// True if segment a-b crosses any edge of any ring (endpoints excluded).
inline bool segment_blocked(const V2& a, const V2& b, const std::vector<std::vector<V2>>& rings) {
    for (const auto& r : rings) {
        std::size_t m = r.size();
        for (std::size_t i = 0; i < m; ++i) {
            const V2& p = r[i]; const V2& q = r[(i + 1) % m];
            if (p.idx == a.idx || p.idx == b.idx || q.idx == a.idx || q.idx == b.idx) continue;
            if (seg_intersect(a, b, p, q)) return true;
        }
    }
    return false;
}

// Merge holes into the outer ring with zero-width bridges (visible-vertex pairs).
inline std::vector<V2> bridge_holes(std::vector<V2> outer, std::vector<std::vector<V2>> holes) {
    // Process holes from right-most to left-most, as in classic ear clipping.
    std::sort(holes.begin(), holes.end(), [](const auto& a, const auto& b) {
        auto mx = [](const auto& r) { double m = -1e300; for (auto& p : r) m = std::max(m, p.x); return m; };
        return mx(a) > mx(b);
    });
    std::vector<std::vector<V2>> all; all.push_back(outer); for (auto& h : holes) all.push_back(h);
    for (std::size_t hi = 0; hi < holes.size(); ++hi) {
        const auto& hole = holes[hi];
        // Candidate pairs by increasing distance; take the first mutually visible.
        std::vector<std::tuple<double, std::size_t, std::size_t>> cand;
        for (std::size_t i = 0; i < outer.size(); ++i)
            for (std::size_t j = 0; j < hole.size(); ++j) {
                double dx = outer[i].x - hole[j].x, dy = outer[i].y - hole[j].y;
                cand.emplace_back(dx * dx + dy * dy, i, j);
            }
        std::sort(cand.begin(), cand.end());
        std::size_t bi = 0, bj = 0; bool found = false;
        for (auto& [d, i, j] : cand) {
            if (!segment_blocked(outer[i], hole[j], all)) { bi = i; bj = j; found = true; break; }
        }
        if (!found) throw std::invalid_argument("tessellate: no visible bridge to hole");
        std::vector<V2> merged;
        merged.reserve(outer.size() + hole.size() + 2);
        for (std::size_t i = 0; i <= bi; ++i) merged.push_back(outer[i]);
        for (std::size_t k = 0; k <= hole.size(); ++k) merged.push_back(hole[(bj + k) % hole.size()]);
        for (std::size_t i = bi; i < outer.size(); ++i) merged.push_back(outer[i]);
        outer = std::move(merged);
        all[0] = outer;
    }
    return outer;
}

// Ear clipping on a simple CCW ring. Emits index triples.
inline void earclip(std::vector<V2> r, std::vector<std::array<std::size_t, 3>>& out) {
    if (r.size() < 3) return;
    if (signed_area2(r) < 0) std::reverse(r.begin(), r.end());
    std::vector<std::size_t> idx(r.size());
    for (std::size_t i = 0; i < r.size(); ++i) idx[i] = i;
    std::size_t guard = 0;
    while (idx.size() > 3 && guard < r.size() * r.size()) {
        bool clipped = false;
        std::size_t n = idx.size();
        for (std::size_t i = 0; i < n; ++i) {
            const V2& a = r[idx[(i + n - 1) % n]]; const V2& b = r[idx[i]]; const V2& c = r[idx[(i + 1) % n]];
            if (cross2(a, b, c) <= 0) continue;  // reflex or collinear
            bool ear = true;
            for (std::size_t k = 0; k < n && ear; ++k) {
                const V2& p = r[idx[k]];
                if (p.idx == a.idx || p.idx == b.idx || p.idx == c.idx) continue;
                if (point_in_tri(p, a, b, c)) ear = false;
            }
            if (!ear) continue;
            out.push_back({a.idx, b.idx, c.idx});
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) throw std::invalid_argument("tessellate: invalid or degenerate polygon");
        ++guard;
    }
    if (idx.size() == 3) out.push_back({r[idx[0]].idx, r[idx[1]].idx, r[idx[2]].idx});
}
}  // namespace detail

// Tessellate a polygon (with holes) into 3D triangles. The triangles inherit
// the exterior ring's winding. Returns an empty vector for < 3 vertices.
inline std::vector<Triangle> tessellate(const Polygon& poly) {
    if (self_intersects(poly)) detail::reject_self_intersection();
    std::vector<Triangle> tris;
    Ring ext = detail::open_ring(poly.exterior);
    if (ext.size() < 3) return tris;
    Vec3 n = newell_normal(ext);
    if (norm2(n) == 0) return tris;
    auto [u, v] = plane_basis(n);
    Vec3 origin = ext[0];

    std::vector<Vec3> verts(ext);
    std::vector<detail::V2> outer = detail::project(ext, origin, u, v, 0);
    if (detail::signed_area2(outer) < 0) {  // make CCW in (u,v), restore later
        std::reverse(outer.begin(), outer.end());
    }
    std::vector<std::vector<detail::V2>> holes;
    for (const Ring& h : poly.holes) {
        Ring ho = detail::open_ring(h);
        if (ho.size() < 3) continue;
        auto hp = detail::project(ho, origin, u, v, verts.size());
        if (!detail::point_in_ring(hp[0], outer)) throw std::invalid_argument("tessellate: hole outside exterior");
        for (const auto& other : holes)
            if (detail::point_in_ring(hp[0], other) || detail::point_in_ring(other[0], hp))
                throw std::invalid_argument("tessellate: nested holes");
        if (detail::signed_area2(hp) > 0) std::reverse(hp.begin(), hp.end());  // holes CW
        verts.insert(verts.end(), ho.begin(), ho.end());
        holes.push_back(std::move(hp));
    }
    std::vector<detail::V2> merged = holes.empty() ? outer : detail::bridge_holes(outer, holes);
    std::vector<std::array<std::size_t, 3>> idx;
    detail::earclip(merged, idx);

    // Ear clipping produced CCW triangles in (u,v); flip if the original ring
    // was CW there so that output winding matches the input ring.
    bool input_cw = detail::signed_area2(detail::project(ext, origin, u, v, 0)) < 0;
    tris.reserve(idx.size());
    for (auto& t : idx) {
        if (input_cw) tris.push_back({verts[t[0]], verts[t[2]], verts[t[1]]});
        else tris.push_back({verts[t[0]], verts[t[1]], verts[t[2]]});
    }
    return tris;
}
inline std::vector<Triangle> tessellate(const PolyhedralSurface& s) {
    if (self_intersects(s)) detail::reject_self_intersection();
    std::vector<Triangle> out;
    for (const Polygon& p : s.patches) { auto t = tessellate(p); out.insert(out.end(), t.begin(), t.end()); }
    return out;
}

// ---------------------------------------------------------------------------
// Measures
// ---------------------------------------------------------------------------
inline double length(const LineString& l) {
    double s = 0;
    for (std::size_t i = 1; i < l.pts.size(); ++i) s += dist(l.pts[i - 1], l.pts[i]);
    return s;
}
inline double ring_length(const Ring& r) {
    Ring c = detail::closed_ring(r);
    double s = 0;
    for (std::size_t i = 1; i < c.size(); ++i) s += dist(c[i - 1], c[i]);
    return s;
}
inline double perimeter(const Polygon& p) {
    double s = ring_length(p.exterior);
    for (const Ring& h : p.holes) s += ring_length(h);
    return s;
}
inline double perimeter(const PolyhedralSurface& s) { double t = 0; for (auto& p : s.patches) t += perimeter(p); return t; }

inline double area(const Triangle& t) { return 0.5 * norm(cross(t.b - t.a, t.c - t.a)); }
// Planar (or near-planar) polygon area via Newell; holes subtracted.
inline double area(const Polygon& p) {
    double a = 0.5 * norm(newell_normal(p.exterior));
    for (const Ring& h : p.holes) a -= 0.5 * norm(newell_normal(h));
    return std::max(a, 0.0);
}
// Area via tessellation — use for non-planar patches where Newell's projected
// area under-estimates the true surface.
inline double area_tessellated(const Polygon& p) { double a = 0; for (auto& t : tessellate(p)) a += area(t); return a; }
inline double area(const PolyhedralSurface& s) { double a = 0; for (auto& p : s.patches) a += area(p); return a; }
inline double area(const Solid& s) { double a = area(s.outer); for (auto& i : s.inner) a += area(i); return a; }

// Signed volume of a (closed) surface via the divergence theorem:
// V = 1/6 Σ a·(b×c) over triangles. Positive when normals point outward.
inline double signed_volume(const PolyhedralSurface& s) {
    double v = 0;
    Vec3 origin;
    if (!s.patches.empty() && !s.patches[0].exterior.empty()) origin = s.patches[0].exterior[0];
    for (const Triangle& t : tessellate(s)) v += dot(t.a-origin, cross(t.b-origin, t.c-origin));
    return v / 6.0;
}
inline double volume(const PolyhedralSurface& s) { return std::fabs(signed_volume(s)); }
inline double volume(const Solid& s) {
    double v = volume(s.outer);
    for (const auto& i : s.inner) v -= volume(i);
    return std::max(v, 0.0);
}

inline Vec3 centroid(const LineString& l) {
    Vec3 c; double w = 0;
    for (std::size_t i = 1; i < l.pts.size(); ++i) {
        double d = dist(l.pts[i - 1], l.pts[i]);
        c += (l.pts[i - 1] + l.pts[i]) * (0.5 * d); w += d;
    }
    return w > 0 ? c / w : (l.pts.empty() ? c : l.pts[0]);
}
inline Vec3 centroid(const Polygon& p) {
    Vec3 c; double w = 0;
    for (const Triangle& t : tessellate(p)) { double a = area(t); c += (t.a + t.b + t.c) * (a / 3.0); w += a; }
    return w > 0 ? c / w : ring_centroid_vertices(p.exterior);
}
inline Vec3 centroid(const PolyhedralSurface& s) {
    Vec3 c; double w = 0;
    for (const Triangle& t : tessellate(s)) { double a = area(t); c += (t.a + t.b + t.c) * (a / 3.0); w += a; }
    return w > 0 ? c / w : c;
}
// Volume-weighted centroid of a closed surface / solid (tetrahedra to origin).
inline Vec3 centroid(const Solid& s) {
    Vec3 c; double w = 0;
    Vec3 origin;
    if (!s.outer.patches.empty() && !s.outer.patches[0].exterior.empty()) origin = s.outer.patches[0].exterior[0];
    auto accumulate = [&](const PolyhedralSurface& surf, double sign) {
        for (const Triangle& t : tessellate(surf)) {
            Vec3 a = t.a-origin, b = t.b-origin, d = t.c-origin;
            double v = dot(a, cross(b, d)) / 6.0;
            c += (a + b + d) * (0.25 * v * sign); w += v * sign;
        }
    };
    double outer_sign = signed_volume(s.outer) >= 0 ? 1.0 : -1.0;
    accumulate(s.outer, outer_sign);
    for (const auto& i : s.inner) accumulate(i, -(signed_volume(i) >= 0 ? 1.0 : -1.0));
    return w != 0 ? origin + c / w : c;
}

inline Box3 bbox(const Ring& r) { Box3 b; for (auto& p : r) b.extend(p); return b; }
inline Box3 bbox(const LineString& l) { return bbox(l.pts); }
inline Box3 bbox(const Polygon& p) { Box3 b = bbox(p.exterior); for (auto& h : p.holes) b.extend(bbox(h)); return b; }
inline Box3 bbox(const PolyhedralSurface& s) { Box3 b; for (auto& p : s.patches) b.extend(bbox(p)); return b; }
inline Box3 bbox(const Solid& s) { Box3 b = bbox(s.outer); for (auto& i : s.inner) b.extend(bbox(i)); return b; }
inline Box3 bbox(const Geometry& g) {
    Box3 b; for (auto& p : g.points) b.extend(p);
    for (auto& l : g.lines) b.extend(bbox(l));
    for (auto& p : g.polygons) b.extend(bbox(p));
    for (auto& s : g.surfaces) b.extend(bbox(s));
    for (auto& s : g.solids) b.extend(bbox(s));
    for (auto& c : g.children) b.extend(bbox(c));
    return b;
}

// Collection-level measures (PostGIS semantics: length counts curves only,
// area counts surfaces only, volume counts solids only).
inline double length(const Geometry& g) {
    double s = 0; for (auto& l : g.lines) s += length(l); for (auto& c : g.children) s += length(c); return s;
}
inline double perimeter(const Geometry& g) {
    double s = 0; for (auto& p : g.polygons) s += perimeter(p); for (auto& x : g.surfaces) s += perimeter(x);
    for (auto& c : g.children) s += perimeter(c);
    return s;
}
inline double area(const Geometry& g) {
    double s = 0; for (auto& p : g.polygons) s += area(p); for (auto& x : g.surfaces) s += area(x);
    for (auto& c : g.children) s += area(c);
    return s;
}
inline double volume(const Geometry& g) {
    double s = 0; for (auto& x : g.solids) s += volume(x); for (auto& c : g.children) s += volume(c); return s;
}

// ---------------------------------------------------------------------------
// Topology of surfaces: closed / solid / outward orientation
// ---------------------------------------------------------------------------
namespace detail {
using VKey = std::tuple<double, double, double>;
inline VKey key(const Vec3& p) { return {p.x, p.y, p.z}; }
struct EdgeStats { int forward = 0, backward = 0; };
// Map over undirected edges (min,max keys) with directed counts.
inline std::map<std::pair<VKey, VKey>, EdgeStats> edge_stats(const PolyhedralSurface& s) {
    std::map<std::pair<VKey, VKey>, EdgeStats> m;
    auto add_ring = [&](const Ring& r) {
        Ring c = closed_ring(r);
        for (std::size_t i = 1; i < c.size(); ++i) {
            VKey a = key(c[i - 1]), b = key(c[i]);
            if (a == b) continue;
            if (a < b) m[{a, b}].forward++; else m[{b, a}].backward++;
        }
    };
    for (const Polygon& p : s.patches) { add_ring(p.exterior); for (auto& h : p.holes) add_ring(h); }
    return m;
}
}  // namespace detail

// Every edge is shared by exactly two patches (manifold, watertight).
inline bool is_closed(const PolyhedralSurface& s) {
    if (s.patches.empty()) return false;
    for (auto& [e, st] : detail::edge_stats(s)) if (st.forward + st.backward != 2) return false;
    return true;
}
// Closed AND consistently oriented (each edge traversed once in each direction).
inline bool is_solid(const PolyhedralSurface& s) {
    if (s.patches.empty()) return false;
    for (auto& [e, st] : detail::edge_stats(s)) if (st.forward != 1 || st.backward != 1) return false;
    return true;
}
inline bool is_solid(const Solid& s) {
    if (!is_solid(s.outer)) return false;
    for (auto& i : s.inner) if (!is_solid(i)) return false;
    return true;
}
// Flip patches so the signed volume is positive (outward normals).
inline PolyhedralSurface force_outward(PolyhedralSurface s) {
    if (signed_volume(s) < 0) for (Polygon& p : s.patches) p = reversed(p);
    return s;
}
inline PolyhedralSurface force_inward(PolyhedralSurface s) {
    if (signed_volume(s) > 0) for (Polygon& p : s.patches) p = reversed(p);
    return s;
}
// ST_MakeSolid: wrap a closed surface as a Solid, normalising orientation.
// Returns false if the surface is not closed.
inline bool make_solid(const PolyhedralSurface& s, Solid& out) {
    if (self_intersects(s)) detail::reject_self_intersection();
    if (!is_solid(s)) return false;
    out = Solid{force_outward(s), {}};
    return true;
}

// ---------------------------------------------------------------------------
// Distances
// ---------------------------------------------------------------------------
inline Vec3 closest_point_on_segment(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 ab = b - a; double l2 = norm2(ab);
    if (l2 == 0) return a;
    double t = std::clamp(dot(p - a, ab) / l2, 0.0, 1.0);
    return a + ab * t;
}
// Closest points between segments p1-q1 and p2-q2 (Ericson, RTCD 5.1.9).
inline std::pair<Vec3, Vec3> closest_segment_segment(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2) {
    const double EPS = 1e-12;
    Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    double a = norm2(d1), e = norm2(d2), f = dot(d2, r);
    double s = 0, t = 0;
    if (a <= EPS && e <= EPS) return {p1, p2};
    if (a <= EPS) { t = std::clamp(f / e, 0.0, 1.0); }
    else {
        double c = dot(d1, r);
        if (e <= EPS) { s = std::clamp(-c / a, 0.0, 1.0); }
        else {
            double b = dot(d1, d2), denom = a * e - b * b;
            s = denom != 0 ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0) { t = 0; s = std::clamp(-c / a, 0.0, 1.0); }
            else if (t > 1) { t = 1; s = std::clamp((b - c) / a, 0.0, 1.0); }
        }
    }
    return {p1 + d1 * s, p2 + d2 * t};
}
// Closest point on triangle to p (Ericson, RTCD 5.1.5).
inline Vec3 closest_point_on_triangle(const Vec3& p, const Triangle& t) {
    const Vec3 &a = t.a, &b = t.b, &c = t.c;
    Vec3 ab = b - a, ac = c - a, ap = p - a;
    double d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    Vec3 bp = p - b; double d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    double vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { double v = d1 / (d1 - d3); return a + ab * v; }
    Vec3 cp = p - c; double d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    double vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { double w = d2 / (d2 - d6); return a + ac * w; }
    double va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) { double w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); return b + (c - b) * w; }
    double denom = 1.0 / (va + vb + vc);
    double v = vb * denom, w = vc * denom;
    return a + ab * v + ac * w;
}

// Primitive soup: distance between arbitrary geometries reduces to
// point/segment/triangle pairs. Solids are treated by their boundary
// (a point inside a solid still has distance 0 — handled via containment test).
struct Primitives {
    std::vector<Vec3> points;
    std::vector<std::pair<Vec3, Vec3>> segments;
    std::vector<Triangle> triangles;
    std::vector<PolyhedralSurface> closed_shells;  // for point-in-solid tests
    std::vector<Vec3> vertices;                    // all vertices (for max distance)
};
namespace detail {
inline void add_ring_segments(const Ring& r, Primitives& P) {
    Ring c = closed_ring(r);
    for (std::size_t i = 1; i < c.size(); ++i) P.segments.push_back({c[i - 1], c[i]});
    for (std::size_t i = 0; i + 1 < c.size(); ++i) P.vertices.push_back(c[i]);
}
inline void add_polygon(const Polygon& p, Primitives& P) {
    auto t = tessellate(p);
    P.triangles.insert(P.triangles.end(), t.begin(), t.end());
    add_ring_segments(p.exterior, P);  // edges kept: robust for degenerate tessellations
    for (auto& h : p.holes) add_ring_segments(h, P);
}
}  // namespace detail
inline Primitives primitives(const Geometry& g) {
    Primitives P;
    for (auto& p : g.points) { P.points.push_back(p); P.vertices.push_back(p); }
    for (auto& l : g.lines) {
        for (std::size_t i = 1; i < l.pts.size(); ++i) P.segments.push_back({l.pts[i - 1], l.pts[i]});
        if (l.pts.size() == 1) P.points.push_back(l.pts[0]);
        P.vertices.insert(P.vertices.end(), l.pts.begin(), l.pts.end());
    }
    for (auto& p : g.polygons) detail::add_polygon(p, P);
    for (auto& s : g.surfaces) for (auto& p : s.patches) detail::add_polygon(p, P);
    for (auto& s : g.solids) {
        for (auto& p : s.outer.patches) detail::add_polygon(p, P);
        for (auto& in : s.inner) for (auto& p : in.patches) detail::add_polygon(p, P);
        P.closed_shells.push_back(s.outer);
    }
    for (auto& c : g.children) {
        Primitives Q = primitives(c);
        P.points.insert(P.points.end(), Q.points.begin(), Q.points.end());
        P.segments.insert(P.segments.end(), Q.segments.begin(), Q.segments.end());
        P.triangles.insert(P.triangles.end(), Q.triangles.begin(), Q.triangles.end());
        P.closed_shells.insert(P.closed_shells.end(), Q.closed_shells.begin(), Q.closed_shells.end());
        P.vertices.insert(P.vertices.end(), Q.vertices.begin(), Q.vertices.end());
    }
    return P;
}

// Ray-casting point-in-closed-surface (ray along +X with tiny tilt to dodge
// edges). Adequate for solids; not robust to points exactly on the boundary.
inline bool point_in_closed_surface(const Vec3& p, const PolyhedralSurface& s) {
    Vec3 dir = normalize(Vec3{1.0, 1e-7, 3e-7});
    int hits = 0;
    for (const Triangle& t : tessellate(s)) {
        Vec3 e1 = t.b - t.a, e2 = t.c - t.a, h = cross(dir, e2);
        double a = dot(e1, h);
        if (std::fabs(a) < 1e-14) continue;
        double f = 1.0 / a; Vec3 sv = p - t.a;
        double u = f * dot(sv, h); if (u < 0 || u > 1) continue;
        Vec3 q = cross(sv, e1); double v = f * dot(dir, q); if (v < 0 || u + v > 1) continue;
        double tt = f * dot(e2, q); if (tt > 1e-12) ++hits;
    }
    return hits % 2 == 1;
}

// Closest pair of points between two geometries and the distance.
struct ClosestResult { Vec3 a, b; double distance; };
inline ClosestResult closest_points(const Geometry& ga, const Geometry& gb) {
    Primitives A = primitives(ga), B = primitives(gb);
    ClosestResult best{{}, {}, std::numeric_limits<double>::infinity()};
    auto consider = [&](const Vec3& x, const Vec3& y) { double d = dist(x, y); if (d < best.distance) best = {x, y, d}; };

    // point-inside-solid short circuits
    for (auto& sh : A.closed_shells) for (auto& v : B.vertices) if (point_in_closed_surface(v, sh)) return {v, v, 0.0};
    for (auto& sh : B.closed_shells) for (auto& v : A.vertices) if (point_in_closed_surface(v, sh)) return {v, v, 0.0};

    for (auto& p : A.points) {
        for (auto& q : B.points) consider(p, q);
        for (auto& s : B.segments) consider(p, closest_point_on_segment(p, s.first, s.second));
        for (auto& t : B.triangles) consider(p, closest_point_on_triangle(p, t));
    }
    for (auto& s : A.segments) {
        for (auto& q : B.points) consider(closest_point_on_segment(q, s.first, s.second), q);
        for (auto& s2 : B.segments) { auto [x, y] = closest_segment_segment(s.first, s.second, s2.first, s2.second); consider(x, y); }
        for (auto& t : B.triangles) {
            // segment-triangle: endpoints vs triangle + segment vs triangle edges
            consider(s.first, closest_point_on_triangle(s.first, t));
            consider(s.second, closest_point_on_triangle(s.second, t));
            const Vec3* e[3][2] = {{&t.a, &t.b}, {&t.b, &t.c}, {&t.c, &t.a}};
            for (auto& ed : e) { auto [x, y] = closest_segment_segment(s.first, s.second, *ed[0], *ed[1]); consider(x, y); }
        }
    }
    for (auto& t : A.triangles) {
        for (auto& q : B.points) consider(closest_point_on_triangle(q, t), q);
        for (auto& s : B.segments) {
            consider(closest_point_on_triangle(s.first, t), s.first);
            consider(closest_point_on_triangle(s.second, t), s.second);
            const Vec3* e[3][2] = {{&t.a, &t.b}, {&t.b, &t.c}, {&t.c, &t.a}};
            for (auto& ed : e) { auto [x, y] = closest_segment_segment(*ed[0], *ed[1], s.first, s.second); consider(x, y); }
        }
        for (auto& t2 : B.triangles) {
            const Vec3* va[3] = {&t.a, &t.b, &t.c}; const Vec3* vb[3] = {&t2.a, &t2.b, &t2.c};
            for (auto v : va) consider(*v, closest_point_on_triangle(*v, t2));
            for (auto v : vb) consider(closest_point_on_triangle(*v, t), *v);
            const Vec3* ea[3][2] = {{&t.a, &t.b}, {&t.b, &t.c}, {&t.c, &t.a}};
            const Vec3* eb[3][2] = {{&t2.a, &t2.b}, {&t2.b, &t2.c}, {&t2.c, &t2.a}};
            for (auto& x : ea) for (auto& y : eb) { auto [p, q] = closest_segment_segment(*x[0], *x[1], *y[0], *y[1]); consider(p, q); }
        }
    }
    return best;
}
inline double distance(const Geometry& a, const Geometry& b) { return closest_points(a, b).distance; }

// Maximum distance between any two vertices (PostGIS ST_3DMaxDistance semantics).
struct FarthestResult { Vec3 a, b; double distance; };
inline FarthestResult farthest_points(const Geometry& ga, const Geometry& gb) {
    Primitives A = primitives(ga), B = primitives(gb);
    FarthestResult best{{}, {}, -1.0};
    for (auto& p : A.vertices) for (auto& q : B.vertices) { double d = dist(p, q); if (d > best.distance) best = {p, q, d}; }
    return best;
}
inline double max_distance(const Geometry& a, const Geometry& b) { return farthest_points(a, b).distance; }

inline bool dwithin(const Geometry& a, const Geometry& b, double d) {
    if (!bbox(a).expanded(d).intersects(bbox(b))) return false;
    return distance(a, b) <= d;
}
inline bool dfully_within(const Geometry& a, const Geometry& b, double d) { return max_distance(a, b) <= d; }
inline bool intersects(const Geometry& a, const Geometry& b, double eps = 1e-9) {
    if (!bbox(a).expanded(eps).intersects(bbox(b))) return false;
    return distance(a, b) <= eps;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
// ST_Extrude: sweep a polygon along vector d, returning a closed solid with
// outward normals (bottom = polygon reversed, top = translated, side quads).
inline Solid extrude(const Polygon& poly, const Vec3& d) {
    if (self_intersects(poly)) detail::reject_self_intersection();
    if (!is_planar(poly) || detail::ring_size(poly.exterior) < 3 ||
        !std::isfinite(norm(d)) || std::fabs(dot(unit_normal(poly), d)) <= 1e-12)
        throw std::invalid_argument("extrude: requires a planar polygon and a non-tangent extrusion");
    // Orient base so its normal opposes the extrusion vector (bottom faces outward).
    Polygon base = poly;
    for (Ring& h : base.holes)
        if (dot(newell_normal(h), newell_normal(base.exterior)) > 0) std::reverse(h.begin(), h.end());
    if (dot(newell_normal(base.exterior), d) > 0) base = reversed(base);
    Polygon top = reversed(base);
    for (Vec3& p : top.exterior) p += d;
    for (Ring& h : top.holes) for (Vec3& p : h) p += d;

    PolyhedralSurface s;
    s.patches.push_back(base);
    s.patches.push_back(top);
    auto side = [&](const Ring& r) {
        Ring c = detail::closed_ring(r);
        for (std::size_t i = 1; i < c.size(); ++i) {
            // base ring runs one way; walking it reversed keeps the quad outward.
            Polygon q; q.exterior = {c[i], c[i - 1], c[i - 1] + d, c[i] + d};
            s.patches.push_back(q);
        }
    };
    side(base.exterior);
    for (const Ring& h : base.holes) side(h);
    if (self_intersects(s)) detail::reject_self_intersection();
    return Solid{force_outward(s), {}};
}

inline const char* version() { return "0.2.0"; }

}  // namespace threesf

#include "constructive.hpp"
