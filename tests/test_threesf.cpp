#include "threesf/threesf.hpp"
#include "threesf/threesf_c.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace threesf;
static int fails = 0;
#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); if (std::fabs(_a - _b) > (tol)) { std::printf("FAIL %s:%d  %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); ++fails; } } while (0)
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

// Axis-aligned box with outward normals (CCW seen from outside).
static PolyhedralSurface box(double x0, double y0, double z0, double x1, double y1, double z1) {
    auto P = [](Vec3 a, Vec3 b, Vec3 c, Vec3 d) { Polygon p; p.exterior = {a, b, c, d, a}; return p; };
    Vec3 v000{x0,y0,z0}, v100{x1,y0,z0}, v110{x1,y1,z0}, v010{x0,y1,z0};
    Vec3 v001{x0,y0,z1}, v101{x1,y0,z1}, v111{x1,y1,z1}, v011{x0,y1,z1};
    PolyhedralSurface s;
    s.patches = {
        P(v000, v010, v110, v100),  // bottom (-z)
        P(v001, v101, v111, v011),  // top (+z)
        P(v000, v100, v101, v001),  // -y
        P(v010, v011, v111, v110),  // +y
        P(v000, v001, v011, v010),  // -x
        P(v100, v110, v111, v101),  // +x
    };
    return s;
}

int main() {
    // --- length / perimeter
    LineString l; l.pts = {{0,0,0},{3,4,0},{3,4,12}};
    CHECK_NEAR(length(l), 17.0, 1e-12);

    // --- tilted planar polygon: right triangle in the plane z = x, legs 1 and 1 (3D area = sqrt(2)/2)
    Polygon tri; tri.exterior = {{0,0,0},{1,0,1},{0,1,0}};
    CHECK_NEAR(area(tri), std::sqrt(2.0) / 2.0, 1e-12);
    CHECK(is_planar(tri));
    CHECK_NEAR(perimeter(tri), std::sqrt(2.0) + 1.0 + std::sqrt(3.0), 1e-12);

    // --- square with a square hole, vertical plane (y = 2): 10x10 minus 2x2
    Polygon holed;
    holed.exterior = {{0,2,0},{10,2,0},{10,2,10},{0,2,10},{0,2,0}};
    holed.holes = {{{4,2,4},{4,2,6},{6,2,6},{6,2,4},{4,2,4}}};
    CHECK_NEAR(area(holed), 96.0, 1e-12);
    CHECK_NEAR(area_tessellated(holed), 96.0, 1e-9);
    CHECK_NEAR(perimeter(holed), 48.0, 1e-12);
    CHECK(tessellate(holed).size() == 8);  // merged ring: 4 + 4 + 2 bridge vertices -> n-2 triangles
    Vec3 c = centroid(holed);
    CHECK_NEAR(c.x, 5.0, 1e-9); CHECK_NEAR(c.y, 2.0, 1e-12); CHECK_NEAR(c.z, 5.0, 1e-9);

    // --- concave (L-shaped) polygon, area 3
    Polygon L; L.exterior = {{0,0,0},{2,0,0},{2,1,0},{1,1,0},{1,2,0},{0,2,0}};
    CHECK_NEAR(area(L), 3.0, 1e-12);
    CHECK_NEAR(area_tessellated(L), 3.0, 1e-12);
    CHECK(orientation(L) == 1);
    CHECK(orientation(force_cw(L)) == -1);

    // --- unit cube
    PolyhedralSurface cube = box(0,0,0,1,1,1);
    CHECK(is_closed(cube));
    CHECK(is_solid(cube));
    CHECK_NEAR(area(cube), 6.0, 1e-12);
    CHECK_NEAR(signed_volume(cube), 1.0, 1e-12);
    CHECK_NEAR(volume(cube), 1.0, 1e-12);
    Solid s; CHECK(make_solid(cube, s));
    Vec3 cc = centroid(s);
    CHECK_NEAR(cc.x, 0.5, 1e-12); CHECK_NEAR(cc.y, 0.5, 1e-12); CHECK_NEAR(cc.z, 0.5, 1e-12);

    // inverted cube -> negative signed volume, force_outward fixes it
    PolyhedralSurface inv = cube; for (auto& p : inv.patches) p = reversed(p);
    CHECK_NEAR(signed_volume(inv), -1.0, 1e-12);
    CHECK_NEAR(signed_volume(force_outward(inv)), 1.0, 1e-12);

    // hollow cube: 4x4x4 outer, 2x2x2 cavity -> 64 - 8 = 56
    Solid hollow{box(0,0,0,4,4,4), {box(1,1,1,3,3,3)}};
    CHECK_NEAR(volume(hollow), 56.0, 1e-12);
    CHECK(is_solid(hollow));

    // open box (missing top) is not closed
    PolyhedralSurface open = cube; open.patches.erase(open.patches.begin() + 1);
    CHECK(!is_closed(open));

    // --- extrude L polygon by (0,0,5): volume 15, area 2*3 + perimeter(8)*5 = 46
    Solid ext = extrude(L, {0,0,5});
    CHECK(is_solid(ext));
    CHECK_NEAR(volume(ext), 15.0, 1e-12);
    CHECK_NEAR(area(ext), 46.0, 1e-12);
    Solid ext_h = extrude(holed, {0,3,0});  // extrude holed square through y
    CHECK_NEAR(volume(ext_h), 96.0 * 3.0, 1e-9);
    CHECK(is_solid(ext_h));

    // --- distances
    Geometry gp; gp.points = {{0.5,0.5,3}};
    Geometry gc; gc.solids = {s};
    CHECK_NEAR(distance(gp, gc), 2.0, 1e-12);
    auto cp = closest_points(gp, gc);
    CHECK_NEAR(cp.b.z, 1.0, 1e-12);
    Geometry inside; inside.points = {{0.5,0.5,0.5}};
    CHECK_NEAR(distance(inside, gc), 0.0, 1e-12);     // inside solid
    CHECK(intersects(inside, gc));
    CHECK_NEAR(max_distance(gp, gc), std::sqrt(0.5 + 9.0), 1e-12);
    CHECK(dwithin(gp, gc, 2.0)); CHECK(!dwithin(gp, gc, 1.99));

    Geometry la; la.lines = {LineString{{{0,0,0},{1,0,0}}}};
    Geometry lb; lb.lines = {LineString{{{0.5,-1,2},{0.5,1,2}}}};
    CHECK_NEAR(distance(la, lb), 2.0, 1e-12);  // skew segments
    Geometry pl; pl.polygons = {tri};
    Geometry far; far.points = {{-1,-1,-1}};
    CHECK_NEAR(distance(far, pl), std::sqrt(3.0), 1e-12);  // nearest vertex (0,0,0)

    // --- C ABI round trip: unit cube as SOLID
    std::vector<double> coords; std::vector<size_t> rs{0}, ps{0}, ss{0};
    for (auto& p : cube.patches) { for (auto& v : p.exterior) { coords.push_back(v.x); coords.push_back(v.y); coords.push_back(v.z); } rs.push_back(coords.size() / 3); ps.push_back(rs.size() - 1); }
    ss.push_back(ps.size() - 1);
    threesf_geom g{THREESF_SOLID, coords.data(), coords.size() / 3, rs.data(), rs.size() - 1, ps.data(), ps.size() - 1, ss.data(), ss.size() - 1};
    CHECK_NEAR(threesf_volume(&g), 1.0, 1e-12);
    CHECK_NEAR(threesf_area(&g), 6.0, 1e-12);
    CHECK(threesf_is_solid(&g) == 1);
    double bb[6]; CHECK(threesf_bbox(&g, bb) == 0); CHECK_NEAR(bb[3], 1.0, 1e-12);
    double* tris = nullptr; size_t nt = 0;
    CHECK(threesf_tessellate(&g, &tris, &nt) == 0); CHECK(nt == 12); threesf_free(tris);
    double pc[3] = {0.5, 0.5, 3.0};
    threesf_geom gpt{THREESF_POINT, pc, 1, nullptr, 0, nullptr, 0, nullptr, 0};
    CHECK_NEAR(threesf_distance(&gpt, &g), 2.0, 1e-12);
    threesf_geom bad{99, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0};
    CHECK(std::isnan(threesf_area(&bad)));

    if (fails == 0) std::printf("all tests passed\n");
    return fails ? 1 : 0;
}
