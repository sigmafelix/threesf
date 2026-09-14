// threesf/io.hpp — Simple Features WKT / WKB (ISO 13249-3 / OGC 06-103r4) with
// PostGIS EWKT/EWKB extensions and SFCGAL SOLID/MULTISOLID.
//
// Reading
//   * Types: POINT, LINESTRING, POLYGON, TRIANGLE, MULTIPOINT, MULTILINESTRING,
//     MULTIPOLYGON, POLYHEDRALSURFACE, TIN, GEOMETRYCOLLECTION, SOLID, MULTISOLID
//   * Dimension markers: "POINT Z (…)", "POINT ZM (…)", "POINT M (…)", "POINTZ(…)",
//     or no marker with 3/4 numbers per vertex (PostGIS style). M is parsed and
//     discarded; 2D input gets z = 0 (matches how PostGIS 3D functions treat it).
//   * EWKT "SRID=4326;…" prefix; EWKB 0x80000000/0x40000000/0x20000000 flags;
//     ISO WKB type offsets 1000 (Z), 2000 (M), 3000 (ZM); both byte orders.
//   * EMPTY at any level.
// Writing
//   * write_wkt → ISO style "POLYGON Z ((…))" (EWKT "SRID=n;" prefix optional)
//   * write_wkb → ISO Z types by default, or EWKB with SRID.
#pragma once
#include "threesf.hpp"
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#if defined(_MSC_VER)
#include <stdlib.h>
#endif
#include <string>
#include <string_view>

namespace threesf {
namespace io_detail {
inline std::uint32_t bswap32(std::uint32_t v) {
#if defined(_MSC_VER)
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}
inline std::uint64_t bswap64(std::uint64_t v) {
#if defined(_MSC_VER)
    return _byteswap_uint64(v);
#else
    return __builtin_bswap64(v);
#endif
}
}  // namespace io_detail

struct ParseError : std::runtime_error { using std::runtime_error::runtime_error; };

// ===========================================================================
// WKT reader
// ===========================================================================
namespace wkt_detail {
struct Reader {
    std::string_view s; std::size_t i = 0;
    explicit Reader(std::string_view sv) : s(sv) {}

    void ws() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    bool eof() { ws(); return i >= s.size(); }
    char peek() { ws(); return i < s.size() ? s[i] : '\0'; }
    void expect(char c) {
        if (peek() != c) throw ParseError(std::string("WKT: expected '") + c + "' at offset " + std::to_string(i));
        ++i;
    }
    bool accept(char c) { if (peek() == c) { ++i; return true; } return false; }
    std::string word() {
        ws(); std::size_t b = i;
        while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
        std::string w(s.substr(b, i - b));
        for (auto& ch : w) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return w;
    }
    double number() {
        ws();
        const char* b = s.data() + i; char* e = nullptr;
        double v = std::strtod(b, &e);
        if (e == b) throw ParseError("WKT: expected number at offset " + std::to_string(i));
        i += static_cast<std::size_t>(e - b);
        return v;
    }
    bool is_number_start() { char c = peek(); return c == '-' || c == '+' || c == '.' || std::isdigit(static_cast<unsigned char>(c)); }

    // dim: 0 = undeclared (infer), 2, 3 (Z), 13 (M only), 4 (ZM)
    Vec3 vertex(int dim) {
        double v[4]; int n = 0;
        while (n < 4 && is_number_start()) v[n++] = number();
        if (n < 2) throw ParseError("WKT: vertex needs at least 2 coordinates");
        switch (dim) {
            case 2:  if (n != 2) throw ParseError("WKT: expected 2 coordinates"); return {v[0], v[1], 0.0};
            case 3:  if (n != 3) throw ParseError("WKT: expected 3 coordinates (Z)"); return {v[0], v[1], v[2]};
            case 13: if (n != 3) throw ParseError("WKT: expected 3 coordinates (M)"); return {v[0], v[1], 0.0};
            case 4:  if (n != 4) throw ParseError("WKT: expected 4 coordinates (ZM)"); return {v[0], v[1], v[2]};
            default: return n >= 3 ? Vec3{v[0], v[1], v[2]} : Vec3{v[0], v[1], 0.0};  // inferred
        }
    }
    bool empty_tag() { std::size_t save = i; std::string w = word(); if (w == "EMPTY") return true; i = save; return false; }

    Ring ring(int dim) {
        Ring r; expect('(');
        do { r.push_back(vertex(dim)); } while (accept(','));
        expect(')'); return r;
    }
    Polygon polygon(int dim) {          // ( ring, ring, … )
        Polygon p; if (empty_tag()) return p;
        expect('(');
        p.exterior = ring(dim);
        while (accept(',')) p.holes.push_back(ring(dim));
        expect(')'); return p;
    }
    PolyhedralSurface surface(int dim) {  // ( polygon, polygon, … )
        PolyhedralSurface s; if (empty_tag()) return s;
        expect('(');
        do { s.patches.push_back(polygon(dim)); } while (accept(','));
        expect(')'); return s;
    }
    Solid solid(int dim) {              // ( surface, surface, … ) — first is outer
        Solid so; if (empty_tag()) return so;
        expect('(');
        so.outer = surface(dim);
        while (accept(',')) so.inner.push_back(surface(dim));
        expect(')'); return so;
    }

    Geometry geometry() {
        Geometry g;
        std::string t = word();
        // Dimension marker: separate token ("POINT Z") or glued ("POINTZ")
        int dim = 0;
        auto strip = [&](const std::string& suffix, int d) {
            if (t.size() > suffix.size() && t.compare(t.size() - suffix.size(), suffix.size(), suffix) == 0) { t.resize(t.size() - suffix.size()); dim = d; return true; }
            return false;
        };
        if (!strip("ZM", 4)) if (!strip("Z", 3)) strip("M", 13);
        if (dim == 0) {
            std::size_t save = i; std::string m = word();
            if (m == "ZM") dim = 4; else if (m == "Z") dim = 3; else if (m == "M") dim = 13; else i = save;
        }
        static const std::pair<const char*, GeomType> types[] = {
            {"POINT", GeomType::Point}, {"LINESTRING", GeomType::LineString}, {"POLYGON", GeomType::Polygon},
            {"TRIANGLE", GeomType::Triangle}, {"MULTIPOINT", GeomType::MultiPoint}, {"MULTILINESTRING", GeomType::MultiLineString},
            {"MULTIPOLYGON", GeomType::MultiPolygon}, {"POLYHEDRALSURFACE", GeomType::PolyhedralSurface}, {"TIN", GeomType::Tin},
            {"GEOMETRYCOLLECTION", GeomType::GeometryCollection}, {"SOLID", GeomType::Solid}, {"MULTISOLID", GeomType::MultiSolid}};
        bool known = false;
        for (auto& [name, gt] : types) if (t == name) { g.type = gt; known = true; break; }
        if (!known) throw ParseError("WKT: unknown geometry type '" + t + "'");
        if (empty_tag()) return g;

        switch (g.type) {
            case GeomType::Point: expect('('); g.points.push_back(vertex(dim)); expect(')'); break;
            case GeomType::LineString: { LineString l; l.pts = ring(dim); g.lines.push_back(std::move(l)); break; }
            case GeomType::Polygon: case GeomType::Triangle: g.polygons.push_back(polygon(dim)); break;
            case GeomType::MultiPoint: {
                expect('(');
                do {  // both "MULTIPOINT ((1 2 3),(4 5 6))" and "MULTIPOINT (1 2 3, 4 5 6)"
                    if (accept('(')) { g.points.push_back(vertex(dim)); expect(')'); }
                    else if (!empty_tag()) g.points.push_back(vertex(dim));
                } while (accept(','));
                expect(')'); break;
            }
            case GeomType::MultiLineString: {
                expect('(');
                do { LineString l; if (!empty_tag()) l.pts = ring(dim); g.lines.push_back(std::move(l)); } while (accept(','));
                expect(')'); break;
            }
            case GeomType::MultiPolygon: {
                expect('(');
                do { g.polygons.push_back(polygon(dim)); } while (accept(','));
                expect(')'); break;
            }
            case GeomType::PolyhedralSurface: case GeomType::Tin: g.surfaces.push_back(surface(dim)); break;
            case GeomType::Solid: g.solids.push_back(solid(dim)); break;
            case GeomType::MultiSolid: {
                expect('(');
                do { g.solids.push_back(solid(dim)); } while (accept(','));
                expect(')'); break;
            }
            case GeomType::GeometryCollection: {
                expect('(');
                do { g.children.push_back(geometry()); } while (accept(','));
                expect(')'); break;
            }
        }
        return g;
    }
};
}  // namespace wkt_detail

inline Geometry read_wkt(std::string_view text) {
    wkt_detail::Reader r(text);
    int srid = 0;
    r.ws();
    if (text.size() > 5 && (text.substr(r.i, 5) == "SRID=" || text.substr(r.i, 5) == "srid=")) {
        r.i += 5; srid = static_cast<int>(r.number()); r.expect(';');
    }
    Geometry g = r.geometry();
    g.srid = srid;
    if (!r.eof()) throw ParseError("WKT: trailing characters at offset " + std::to_string(r.i));
    return g;
}

// ===========================================================================
// WKT writer
// ===========================================================================
namespace wkt_detail {
// Shortest decimal representation that round-trips through strtod (portable
// stand-in for std::to_chars(double), which older libc++ lacks).
inline void num(std::string& out, double v) {
    char buf[40];
    if (v == 0) { out += (std::signbit(v) ? "-0" : "0"); return; }
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (prec == 17 || std::strtod(buf, nullptr) == v) break;
    }
    out += buf;
}
inline void vertex(std::string& out, const Vec3& p) { num(out, p.x); out += ' '; num(out, p.y); out += ' '; num(out, p.z); }
inline void ring(std::string& out, const Ring& r) {
    out += '(';
    for (std::size_t i = 0; i < r.size(); ++i) { if (i) out += ", "; vertex(out, r[i]); }
    out += ')';
}
inline void polygon(std::string& out, const Polygon& p) {
    if (p.exterior.empty()) { out += "EMPTY"; return; }
    out += '('; ring(out, p.exterior);
    for (auto& h : p.holes) { out += ", "; ring(out, h); }
    out += ')';
}
inline void surface(std::string& out, const PolyhedralSurface& s) {
    if (s.patches.empty()) { out += "EMPTY"; return; }
    out += '(';
    for (std::size_t i = 0; i < s.patches.size(); ++i) { if (i) out += ", "; polygon(out, s.patches[i]); }
    out += ')';
}
inline void solid(std::string& out, const Solid& s) {
    if (s.outer.patches.empty()) { out += "EMPTY"; return; }
    out += '('; surface(out, s.outer);
    for (auto& in : s.inner) { out += ", "; surface(out, in); }
    out += ')';
}
inline void geometry(std::string& out, const Geometry& g) {
    out += type_name(g.type); out += " Z ";
    if (g.empty()) { out += "EMPTY"; return; }
    switch (g.type) {
        case GeomType::Point: out += '('; vertex(out, g.points[0]); out += ')'; break;
        case GeomType::MultiPoint:
            out += '(';
            for (std::size_t i = 0; i < g.points.size(); ++i) { if (i) out += ", "; out += '('; vertex(out, g.points[i]); out += ')'; }
            out += ')'; break;
        case GeomType::LineString: ring(out, g.lines[0].pts); break;
        case GeomType::MultiLineString:
            out += '(';
            for (std::size_t i = 0; i < g.lines.size(); ++i) { if (i) out += ", "; if (g.lines[i].pts.empty()) out += "EMPTY"; else ring(out, g.lines[i].pts); }
            out += ')'; break;
        case GeomType::Polygon: case GeomType::Triangle: polygon(out, g.polygons[0]); break;
        case GeomType::MultiPolygon:
            out += '(';
            for (std::size_t i = 0; i < g.polygons.size(); ++i) { if (i) out += ", "; polygon(out, g.polygons[i]); }
            out += ')'; break;
        case GeomType::PolyhedralSurface: case GeomType::Tin: surface(out, g.surfaces[0]); break;
        case GeomType::Solid: solid(out, g.solids[0]); break;
        case GeomType::MultiSolid:
            out += '(';
            for (std::size_t i = 0; i < g.solids.size(); ++i) { if (i) out += ", "; solid(out, g.solids[i]); }
            out += ')'; break;
        case GeomType::GeometryCollection:
            out += '(';
            for (std::size_t i = 0; i < g.children.size(); ++i) { if (i) out += ", "; geometry(out, g.children[i]); }
            out += ')'; break;
    }
}
}  // namespace wkt_detail

inline std::string write_wkt(const Geometry& g, bool ewkt_srid = false) {
    std::string out;
    if (ewkt_srid && g.srid) { out += "SRID="; out += std::to_string(g.srid); out += ';'; }
    wkt_detail::geometry(out, g);
    return out;
}

// ===========================================================================
// WKB
// ===========================================================================
namespace wkb_detail {
constexpr std::uint32_t EWKB_Z = 0x80000000u, EWKB_M = 0x40000000u, EWKB_SRID = 0x20000000u;

inline std::uint32_t base_type(GeomType t) {
    switch (t) {
        case GeomType::Point: return 1;  case GeomType::LineString: return 2;  case GeomType::Polygon: return 3;
        case GeomType::MultiPoint: return 4; case GeomType::MultiLineString: return 5; case GeomType::MultiPolygon: return 6;
        case GeomType::GeometryCollection: return 7; case GeomType::PolyhedralSurface: return 15; case GeomType::Tin: return 16;
        case GeomType::Triangle: return 17; case GeomType::Solid: return 101; case GeomType::MultiSolid: return 102;  // SFCGAL
    }
    return 0;
}
inline bool geom_type(std::uint32_t b, GeomType& t) {
    switch (b) {
        case 1: t = GeomType::Point; return true;  case 2: t = GeomType::LineString; return true;
        case 3: t = GeomType::Polygon; return true; case 4: t = GeomType::MultiPoint; return true;
        case 5: t = GeomType::MultiLineString; return true; case 6: t = GeomType::MultiPolygon; return true;
        case 7: t = GeomType::GeometryCollection; return true; case 15: t = GeomType::PolyhedralSurface; return true;
        case 16: t = GeomType::Tin; return true; case 17: t = GeomType::Triangle; return true;
        case 101: t = GeomType::Solid; return true; case 102: t = GeomType::MultiSolid; return true;
    }
    return false;
}
inline bool host_little() { std::uint16_t x = 1; return *reinterpret_cast<std::uint8_t*>(&x) == 1; }

struct Reader {
    const std::uint8_t* p; std::size_t n, i = 0; bool little = true;
    Reader(const std::uint8_t* d, std::size_t len) : p(d), n(len) {}
    void need(std::size_t k) { if (i + k > n) throw ParseError("WKB: unexpected end of data"); }
    std::uint8_t u8() { need(1); return p[i++]; }
    std::uint32_t u32() {
        need(4); std::uint32_t v;
        std::memcpy(&v, p + i, 4); i += 4;
        if (little != host_little()) v = io_detail::bswap32(v);
        return v;
    }
    double f64() {
        need(8); std::uint64_t v;
        std::memcpy(&v, p + i, 8); i += 8;
        if (little != host_little()) v = io_detail::bswap64(v);
        double d; std::memcpy(&d, &v, 8); return d;
    }
    Vec3 vertex(int ncoord, bool has_z) {
        double c[4]; for (int k = 0; k < ncoord; ++k) c[k] = f64();
        return {c[0], c[1], has_z ? c[2] : 0.0};
    }
    Ring ring(int nc, bool z) { std::uint32_t m = u32(); Ring r; r.reserve(m); for (std::uint32_t k = 0; k < m; ++k) r.push_back(vertex(nc, z)); return r; }
    Polygon polygon_body(int nc, bool z) {
        std::uint32_t nr = u32(); Polygon p;
        if (nr == 0) return p;
        p.exterior = ring(nc, z);
        for (std::uint32_t k = 1; k < nr; ++k) p.holes.push_back(ring(nc, z));
        return p;
    }
    // Sub-geometries in collections carry their own byte-order/type header.
    Geometry geometry() {
        std::uint8_t bo = u8();
        if (bo > 1) throw ParseError("WKB: bad byte order");
        little = bo == 1;
        std::uint32_t raw = u32();
        bool has_z = false, has_m = false; int srid = 0;
        std::uint32_t base = raw;
        if (raw & (EWKB_Z | EWKB_M | EWKB_SRID)) {          // EWKB
            has_z = raw & EWKB_Z; has_m = raw & EWKB_M;
            if (raw & EWKB_SRID) { srid = static_cast<int>(u32()); }
            base = raw & 0x0FFFFFFFu;
        } else if (raw >= 1000) {                           // ISO
            std::uint32_t d = raw / 1000; base = raw % 1000;
            has_z = (d == 1 || d == 3); has_m = (d == 2 || d == 3);
        }
        int nc = 2 + (has_z ? 1 : 0) + (has_m ? 1 : 0);
        Geometry g; g.srid = srid;
        if (!geom_type(base, g.type)) throw ParseError("WKB: unknown geometry type " + std::to_string(base));
        auto sub_polygon = [&]() { Geometry s = geometry(); if (s.polygons.empty()) return Polygon{}; return s.polygons[0]; };
        auto sub_surface = [&]() { Geometry s = geometry(); if (s.surfaces.empty()) return PolyhedralSurface{}; return s.surfaces[0]; };
        switch (g.type) {
            case GeomType::Point: {
                Vec3 v = vertex(nc, has_z);
                if (!(std::isnan(v.x) && std::isnan(v.y))) g.points.push_back(v);  // NaN NaN encodes POINT EMPTY
                break;
            }
            case GeomType::LineString: { LineString l; l.pts = ring(nc, has_z); g.lines.push_back(std::move(l)); break; }
            case GeomType::Polygon: case GeomType::Triangle: g.polygons.push_back(polygon_body(nc, has_z)); break;
            case GeomType::MultiPoint: { std::uint32_t m = u32(); for (std::uint32_t k = 0; k < m; ++k) { Geometry s = geometry(); if (!s.points.empty()) g.points.push_back(s.points[0]); } break; }
            case GeomType::MultiLineString: { std::uint32_t m = u32(); for (std::uint32_t k = 0; k < m; ++k) { Geometry s = geometry(); g.lines.push_back(s.lines.empty() ? LineString{} : s.lines[0]); } break; }
            case GeomType::MultiPolygon: { std::uint32_t m = u32(); for (std::uint32_t k = 0; k < m; ++k) g.polygons.push_back(sub_polygon()); break; }
            case GeomType::PolyhedralSurface: case GeomType::Tin: {
                std::uint32_t m = u32(); PolyhedralSurface s;
                for (std::uint32_t k = 0; k < m; ++k) s.patches.push_back(sub_polygon());
                g.surfaces.push_back(std::move(s)); break;
            }
            case GeomType::Solid: {
                std::uint32_t m = u32(); Solid so;
                for (std::uint32_t k = 0; k < m; ++k) { PolyhedralSurface s = sub_surface(); if (k == 0) so.outer = std::move(s); else so.inner.push_back(std::move(s)); }
                g.solids.push_back(std::move(so)); break;
            }
            case GeomType::MultiSolid: { std::uint32_t m = u32(); for (std::uint32_t k = 0; k < m; ++k) { Geometry s = geometry(); g.solids.push_back(s.solids.empty() ? Solid{} : s.solids[0]); } break; }
            case GeomType::GeometryCollection: { std::uint32_t m = u32(); for (std::uint32_t k = 0; k < m; ++k) g.children.push_back(geometry()); break; }
        }
        return g;
    }
};

struct Writer {
    std::vector<std::uint8_t> out; bool ewkb; bool little = true;
    explicit Writer(bool e) : ewkb(e) {}
    void u8(std::uint8_t v) { out.push_back(v); }
    void u32(std::uint32_t v) { if (little != host_little()) v = io_detail::bswap32(v); std::uint8_t b[4]; std::memcpy(b, &v, 4); out.insert(out.end(), b, b + 4); }
    void f64(double d) { std::uint64_t v; std::memcpy(&v, &d, 8); if (little != host_little()) v = io_detail::bswap64(v); std::uint8_t b[8]; std::memcpy(b, &v, 8); out.insert(out.end(), b, b + 8); }
    void vertex(const Vec3& p) { f64(p.x); f64(p.y); f64(p.z); }
    void ring(const Ring& r) { u32(static_cast<std::uint32_t>(r.size())); for (auto& p : r) vertex(p); }
    void header(GeomType t, int srid, bool top) {
        u8(1);  // little endian
        std::uint32_t b = base_type(t);
        if (ewkb) { b |= EWKB_Z; if (top && srid) b |= EWKB_SRID; u32(b); if (top && srid) u32(static_cast<std::uint32_t>(srid)); }
        else u32(b + 1000);
    }
    void polygon_body(const Polygon& p) {
        if (p.exterior.empty()) { u32(0); return; }
        u32(static_cast<std::uint32_t>(1 + p.holes.size()));
        ring(p.exterior); for (auto& h : p.holes) ring(h);
    }
    void polygon(const Polygon& p, GeomType t = GeomType::Polygon) { header(t, 0, false); polygon_body(p); }
    void surface(const PolyhedralSurface& s, GeomType t) {
        header(t, 0, false); u32(static_cast<std::uint32_t>(s.patches.size()));
        for (auto& p : s.patches) polygon(p);
    }
    void solid(const Solid& s) {
        header(GeomType::Solid, 0, false);
        if (s.outer.patches.empty()) { u32(0); return; }
        u32(static_cast<std::uint32_t>(1 + s.inner.size()));
        surface(s.outer, GeomType::PolyhedralSurface); for (auto& in : s.inner) surface(in, GeomType::PolyhedralSurface);
    }
    void geometry(const Geometry& g, bool top) {
        header(g.type, g.srid, top);
        auto nan = std::numeric_limits<double>::quiet_NaN();
        switch (g.type) {
            case GeomType::Point: if (g.points.empty()) vertex({nan, nan, nan}); else vertex(g.points[0]); break;
            case GeomType::LineString: ring(g.lines.empty() ? Ring{} : g.lines[0].pts); break;
            case GeomType::Polygon: case GeomType::Triangle: polygon_body(g.polygons.empty() ? Polygon{} : g.polygons[0]); break;
            case GeomType::MultiPoint: u32(static_cast<std::uint32_t>(g.points.size())); for (auto& p : g.points) { header(GeomType::Point, 0, false); vertex(p); } break;
            case GeomType::MultiLineString: u32(static_cast<std::uint32_t>(g.lines.size())); for (auto& l : g.lines) { header(GeomType::LineString, 0, false); ring(l.pts); } break;
            case GeomType::MultiPolygon: u32(static_cast<std::uint32_t>(g.polygons.size())); for (auto& p : g.polygons) polygon(p); break;
            case GeomType::PolyhedralSurface: case GeomType::Tin: {
                const PolyhedralSurface& s = g.surfaces.empty() ? PolyhedralSurface{} : g.surfaces[0];
                u32(static_cast<std::uint32_t>(s.patches.size()));
                for (auto& p : s.patches) polygon(p, g.type == GeomType::Tin ? GeomType::Triangle : GeomType::Polygon);
                break;
            }
            case GeomType::Solid: {
                const Solid& s = g.solids.empty() ? Solid{} : g.solids[0];
                if (s.outer.patches.empty()) { u32(0); break; }
                u32(static_cast<std::uint32_t>(1 + s.inner.size()));
                surface(s.outer, GeomType::PolyhedralSurface); for (auto& in : s.inner) surface(in, GeomType::PolyhedralSurface);
                break;
            }
            case GeomType::MultiSolid: u32(static_cast<std::uint32_t>(g.solids.size())); for (auto& s : g.solids) solid(s); break;
            case GeomType::GeometryCollection: u32(static_cast<std::uint32_t>(g.children.size())); for (auto& c : g.children) geometry(c, false); break;
        }
    }
};
}  // namespace wkb_detail

inline Geometry read_wkb(const std::uint8_t* data, std::size_t len) {
    wkb_detail::Reader r(data, len);
    Geometry g = r.geometry();
    if (r.i != len) throw ParseError("WKB: trailing bytes");
    return g;
}
inline Geometry read_wkb(const std::vector<std::uint8_t>& v) { return read_wkb(v.data(), v.size()); }
inline std::vector<std::uint8_t> write_wkb(const Geometry& g, bool ewkb = false) {
    wkb_detail::Writer w(ewkb); w.geometry(g, true); return std::move(w.out);
}
// Hex helpers (PostGIS text form of WKB)
inline std::string to_hex(const std::vector<std::uint8_t>& v) {
    static const char* hx = "0123456789ABCDEF"; std::string s; s.reserve(v.size() * 2);
    for (auto b : v) { s += hx[b >> 4]; s += hx[b & 15]; }
    return s;
}
inline std::vector<std::uint8_t> from_hex(std::string_view h) {
    if (h.size() % 2) throw ParseError("hex: odd length");
    std::vector<std::uint8_t> v; v.reserve(h.size() / 2);
    auto nib = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); if (c >= 'A' && c <= 'F') return c - 'A' + 10; throw ParseError("hex: bad digit"); };
    for (std::size_t i = 0; i < h.size(); i += 2) v.push_back(static_cast<std::uint8_t>(nib(h[i]) * 16 + nib(h[i + 1])));
    return v;
}
inline Geometry read_hexwkb(std::string_view h) { return read_wkb(from_hex(h)); }
inline std::string write_hexwkb(const Geometry& g, bool ewkb = false) { return to_hex(write_wkb(g, ewkb)); }

}  // namespace threesf
