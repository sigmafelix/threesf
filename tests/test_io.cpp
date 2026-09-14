#include "threesf/threesf.hpp"
#include "threesf/io.hpp"
#include "threesf/threesf_c.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace threesf;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); if (std::fabs(_a - _b) > (tol)) { std::printf("FAIL %s:%d  %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); ++fails; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { std::printf("FAIL %s:%d  %s\n   got: %s\n  want: %s\n", __FILE__, __LINE__, #a, std::string(_a).c_str(), std::string(_b).c_str()); ++fails; } } while (0)

static void roundtrip(const std::string& wkt_in, const std::string& wkt_expected) {
    Geometry g = read_wkt(wkt_in);
    std::string w = write_wkt(g);
    CHECK_EQ(w, wkt_expected);
    // WKT -> WKB (ISO) -> WKT
    Geometry g2 = read_wkb(write_wkb(g, false));
    CHECK_EQ(write_wkt(g2), wkt_expected);
    // WKT -> EWKB -> WKT, srid preserved
    g.srid = 5179;
    Geometry g3 = read_wkb(write_wkb(g, true));
    CHECK_EQ(write_wkt(g3), wkt_expected);
    CHECK(g3.srid == 5179);
    // hex path
    CHECK_EQ(write_wkt(read_hexwkb(write_hexwkb(g))), wkt_expected);
}

int main() {
    // --- every SF Z type round-trips through WKT and WKB ---------------------
    roundtrip("POINT Z (1 2 3)", "POINT Z (1 2 3)");
    roundtrip("POINTZ(1 2 3)", "POINT Z (1 2 3)");                       // glued marker
    roundtrip("POINT(1 2 3)", "POINT Z (1 2 3)");                        // PostGIS style, inferred Z
    roundtrip("POINT (1 2)", "POINT Z (1 2 0)");                         // 2D -> z = 0
    roundtrip("POINT M (1 2 9)", "POINT Z (1 2 0)");                     // M dropped
    roundtrip("POINT ZM (1 2 3 9)", "POINT Z (1 2 3)");                  // M dropped, Z kept
    roundtrip("POINT Z EMPTY", "POINT Z EMPTY");
    roundtrip("point z (0.1 -2.5e3 1e-7)", "POINT Z (0.1 -2500 1e-07)"); // lowercase, sci notation
    roundtrip("LINESTRING Z (0 0 0, 3 4 0, 3 4 12)", "LINESTRING Z (0 0 0, 3 4 0, 3 4 12)");
    roundtrip("POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0), (4 4 0, 6 4 0, 6 6 0, 4 6 0, 4 4 0))",
              "POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0), (4 4 0, 6 4 0, 6 6 0, 4 6 0, 4 4 0))");
    roundtrip("TRIANGLE Z ((0 0 0, 1 0 0, 0 1 0, 0 0 0))", "TRIANGLE Z ((0 0 0, 1 0 0, 0 1 0, 0 0 0))");
    roundtrip("MULTIPOINT Z ((1 2 3), (4 5 6))", "MULTIPOINT Z ((1 2 3), (4 5 6))");
    roundtrip("MULTIPOINT Z (1 2 3, 4 5 6)", "MULTIPOINT Z ((1 2 3), (4 5 6))");  // bare form
    roundtrip("MULTILINESTRING Z ((0 0 0, 1 1 1), (2 2 2, 3 3 3, 4 4 4))", "MULTILINESTRING Z ((0 0 0, 1 1 1), (2 2 2, 3 3 3, 4 4 4))");
    roundtrip("MULTIPOLYGON Z (((0 0 0, 1 0 0, 1 1 0, 0 0 0)), ((5 5 5, 6 5 5, 6 6 5, 5 5 5)))",
              "MULTIPOLYGON Z (((0 0 0, 1 0 0, 1 1 0, 0 0 0)), ((5 5 5, 6 5 5, 6 6 5, 5 5 5)))");
    roundtrip("POLYHEDRALSURFACE Z (((0 0 0, 0 1 0, 1 1 0, 1 0 0, 0 0 0)), ((0 0 1, 1 0 1, 1 1 1, 0 1 1, 0 0 1)))",
              "POLYHEDRALSURFACE Z (((0 0 0, 0 1 0, 1 1 0, 1 0 0, 0 0 0)), ((0 0 1, 1 0 1, 1 1 1, 0 1 1, 0 0 1)))");
    roundtrip("TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)), ((0 0 0, 0 1 0, 0 0 1, 0 0 0)))",
              "TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)), ((0 0 0, 0 1 0, 0 0 1, 0 0 0)))");
    roundtrip("GEOMETRYCOLLECTION Z (POINT Z (1 2 3), LINESTRING Z (0 0 0, 1 1 1), GEOMETRYCOLLECTION Z (POLYGON Z ((0 0 0, 1 0 0, 0 1 0, 0 0 0))))",
              "GEOMETRYCOLLECTION Z (POINT Z (1 2 3), LINESTRING Z (0 0 0, 1 1 1), GEOMETRYCOLLECTION Z (POLYGON Z ((0 0 0, 1 0 0, 0 1 0, 0 0 0))))");
    roundtrip("GEOMETRYCOLLECTION Z EMPTY", "GEOMETRYCOLLECTION Z EMPTY");
    // SFCGAL SOLID: outer shell with 6 faces, then a cavity shell
    const std::string cube =
        "((0 0 0, 0 1 0, 1 1 0, 1 0 0, 0 0 0)), ((0 0 1, 1 0 1, 1 1 1, 0 1 1, 0 0 1)), ((0 0 0, 1 0 0, 1 0 1, 0 0 1, 0 0 0)), "
        "((0 1 0, 0 1 1, 1 1 1, 1 1 0, 0 1 0)), ((0 0 0, 0 0 1, 0 1 1, 0 1 0, 0 0 0)), ((1 0 0, 1 1 0, 1 1 1, 1 0 1, 1 0 0))";
    roundtrip("SOLID Z ((" + cube + "))", "SOLID Z ((" + cube + "))");
    roundtrip("SRID=4326;POINT(1 2 3)", "POINT Z (1 2 3)");
    CHECK(read_wkt("SRID=4326;POINT(1 2 3)").srid == 4326);
    CHECK_EQ(write_wkt(read_wkt("SRID=4326;POINT(1 2 3)"), true), std::string("SRID=4326;POINT Z (1 2 3)"));

    // --- measures straight from WKT -----------------------------------------
    CHECK_NEAR(area(read_wkt("POLYGON Z ((0 2 0, 10 2 0, 10 2 10, 0 2 10, 0 2 0), (4 2 4, 4 2 6, 6 2 6, 6 2 4, 4 2 4))")), 96.0, 1e-12);
    CHECK_NEAR(length(read_wkt("MULTILINESTRING Z ((0 0 0, 3 4 0), (0 0 0, 0 0 12))")), 17.0, 1e-12);
    Geometry solid = read_wkt("SOLID Z ((" + cube + "))");
    CHECK_NEAR(volume(solid), 1.0, 1e-12);
    CHECK(is_solid(solid.solids[0]));
    Geometry tin = read_wkt("TIN Z (((0 0 0, 0 1 0, 1 0 0, 0 0 0)), ((0 0 0, 1 0 0, 0 0 1, 0 0 0)), ((0 0 0, 0 0 1, 0 1 0, 0 0 0)), ((1 0 0, 0 1 0, 0 0 1, 1 0 0)))");
    CHECK(is_closed(tin.surfaces[0]));
    CHECK_NEAR(volume(tin.surfaces[0]), 1.0 / 6.0, 1e-12);  // unit tetrahedron
    CHECK_NEAR(distance(read_wkt("POINT Z (0.5 0.5 3)"), solid), 2.0, 1e-12);
    CHECK_NEAR(area(read_wkt("GEOMETRYCOLLECTION Z (POLYGON Z ((0 0 0, 1 0 0, 0 1 0, 0 0 0)), GEOMETRYCOLLECTION Z (TRIANGLE Z ((0 0 5, 2 0 5, 0 2 5, 0 0 5))))")), 2.5, 1e-12);

    // --- PostGIS fixtures -----------------------------------------------------
    // SELECT ST_AsEWKB('SRID=4326;POINT(1 2 3)'::geometry)  -> little-endian, Z|SRID flags
    Geometry pg = read_hexwkb("01010000A0E6100000000000000000F03F00000000000000400000000000000840");
    CHECK(pg.type == GeomType::Point && pg.srid == 4326);
    CHECK_NEAR(pg.points[0].z, 3.0, 0);
    // SELECT ST_AsBinary('POINT Z (1 2 3)'::geometry)  -> ISO type 1001
    Geometry iso = read_hexwkb("01E9030000000000000000F03F00000000000000400000000000000840");
    CHECK(iso.type == GeomType::Point && iso.srid == 0 && iso.points[0].y == 2.0);
    CHECK_EQ(write_hexwkb(iso), std::string("01E9030000000000000000F03F00000000000000400000000000000840"));
    // big-endian ISO POINT Z (1 2 3)
    Geometry be = read_hexwkb("00000003E93FF000000000000040000000000000004008000000000000");
    CHECK(be.points[0].x == 1.0 && be.points[0].z == 3.0);
    // 2D WKB is accepted (z = 0): SELECT ST_AsBinary('LINESTRING(0 0, 3 4)'::geometry)
    Geometry l2d = read_hexwkb("010200000002000000000000000000000000000000000000000000000000000840000000000000" "1040");
    CHECK_NEAR(length(l2d), 5.0, 1e-12);
    // POINT EMPTY as NaN NaN (PostGIS convention) -> empty
    Geometry pe = read_wkb(write_wkb(read_wkt("POINT Z EMPTY")));
    CHECK(pe.empty() && pe.type == GeomType::Point);

    // --- errors are exceptions, not crashes ---------------------------------
    for (const char* bad : {"POINT Z (1 2)", "FOO (1 2 3)", "POLYGON Z ((0 0 0, 1 1 1)", "POINT Z (1 2 3) trailing", ""}) {
        bool threw = false; try { read_wkt(bad); } catch (const ParseError&) { threw = true; }
        CHECK(threw);
    }
    { bool threw = false; try { read_hexwkb("0101"); } catch (const ParseError&) { threw = true; } CHECK(threw); }

    // --- C handle API ----------------------------------------------------------
    threesf_handle* h = threesf_read_wkt(("SOLID Z ((" + cube + "))").c_str());
    CHECK(h != nullptr);
    CHECK_NEAR(threesf_h_volume(h), 1.0, 1e-12);
    CHECK(std::string(threesf_h_type(h)) == "SOLID");
    CHECK(threesf_h_num_vertices(h) == 24);
    threesf_handle* p = threesf_read_wkt("POINT Z (0.5 0.5 3)");
    CHECK_NEAR(threesf_h_distance(p, h), 2.0, 1e-12);
    threesf_handle* t = threesf_h_tessellate(h);
    CHECK(std::string(threesf_h_type(t)) == "TIN");
    CHECK_NEAR(threesf_h_area(t), 6.0, 1e-12);
    char* w = threesf_h_wkt(t, 0); CHECK(std::string(w).rfind("TIN Z (((", 0) == 0); threesf_free(w);
    size_t n = 0; unsigned char* b = threesf_h_wkb(h, 1, &n); CHECK(n > 0);
    threesf_handle* h2 = threesf_read_wkb(b, n); CHECK_NEAR(threesf_h_volume(h2), 1.0, 1e-12); threesf_free(b);
    threesf_handle* sq = threesf_read_wkt("POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0))");
    threesf_handle* prism = threesf_h_extrude(sq, 0, 0, 5);
    CHECK_NEAR(threesf_h_volume(prism), 500.0, 1e-9);
    CHECK(threesf_h_is_solid(prism) == 1);
    CHECK(threesf_read_wkt("NOPE") == nullptr);
    CHECK(std::string(threesf_last_error()).find("unknown geometry type") != std::string::npos);
    for (auto* x : {h, p, t, h2, sq, prism}) threesf_h_free(x);

    if (fails == 0) std::printf("all io tests passed\n");
    return fails ? 1 : 0;
}
