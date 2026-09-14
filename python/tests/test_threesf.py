import math

import numpy as np
import pytest

import threesf as g3

CUBE = ("SOLID Z ((((0 0 0, 0 1 0, 1 1 0, 1 0 0, 0 0 0)), ((0 0 1, 1 0 1, 1 1 1, 0 1 1, 0 0 1)), "
        "((0 0 0, 1 0 0, 1 0 1, 0 0 1, 0 0 0)), ((0 1 0, 0 1 1, 1 1 1, 1 1 0, 0 1 0)), "
        "((0 0 0, 0 0 1, 0 1 1, 0 1 0, 0 0 0)), ((1 0 0, 1 1 0, 1 1 1, 1 0 1, 1 0 0))))")


def test_version():
    assert g3.version() == "0.2.0"


def test_measures_from_wkt():
    cube = g3.read_wkt(CUBE)
    assert cube.geom_type == "SOLID"
    assert g3.st_volume(cube) == pytest.approx(1.0)
    assert g3.st_3darea(cube) == pytest.approx(6.0)
    assert g3.st_issolid(cube) and g3.st_isclosed(cube)
    np.testing.assert_allclose(g3.st_3dcentroid(cube), [0.5, 0.5, 0.5])
    assert g3.st_3dlength(g3.read_wkt("LINESTRING Z (0 0 0, 3 4 0, 3 4 12)")) == pytest.approx(17.0)


def test_holed_polygon_and_extrude():
    holed = g3.polygon([(0, 2, 0), (10, 2, 0), (10, 2, 10), (0, 2, 10)], holes=[[(4, 2, 4), (4, 2, 6), (6, 2, 6), (6, 2, 4)]])
    assert g3.st_3darea(holed) == pytest.approx(96.0)
    assert g3.st_3dperimeter(holed) == pytest.approx(48.0)
    prism = g3.st_extrude(holed, 0, 3, 0)
    assert prism.geom_type == "SOLID"
    assert g3.st_volume(prism) == pytest.approx(288.0)
    assert g3.triangles(prism).shape[1:] == (3, 3)


def test_distance_family():
    cube, p = g3.read_wkt(CUBE), g3.point(0.5, 0.5, 3)
    assert g3.st_3ddistance(p, cube) == pytest.approx(2.0)
    np.testing.assert_allclose(g3.st_3dshortestline(p, cube)[1], [0.5, 0.5, 1.0])
    assert g3.st_3ddistance(g3.point(0.5, 0.5, 0.5), cube) == 0.0
    assert g3.st_3ddwithin(p, cube, 2.0) and not g3.st_3ddwithin(p, cube, 1.99)
    assert g3.st_3dmaxdistance(p, cube) == pytest.approx(math.sqrt(9.5))


@pytest.mark.parametrize("wkt,expected", [
    ("POINT Z (1 2 3)", "POINT Z (1 2 3)"),
    ("POINTZ(1 2 3)", "POINT Z (1 2 3)"),
    ("POINT(1 2 3)", "POINT Z (1 2 3)"),
    ("POINT (1 2)", "POINT Z (1 2 0)"),
    ("POINT ZM (1 2 3 9)", "POINT Z (1 2 3)"),
    ("POINT Z EMPTY", "POINT Z EMPTY"),
    ("MULTIPOINT Z (1 2 3, 4 5 6)", "MULTIPOINT Z ((1 2 3), (4 5 6))"),
    ("TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)))", "TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)))"),
    ("GEOMETRYCOLLECTION Z (POINT Z (1 2 3), GEOMETRYCOLLECTION Z (LINESTRING Z (0 0 0, 1 1 1)))",
     "GEOMETRYCOLLECTION Z (POINT Z (1 2 3), GEOMETRYCOLLECTION Z (LINESTRING Z (0 0 0, 1 1 1)))"),
])
def test_wkt_wkb_roundtrip(wkt, expected):
    g = g3.read_wkt(wkt)
    assert g.wkt == expected
    assert g3.read_wkb(g.wkb).wkt == expected
    g.srid = 5179
    back = g3.read_wkb(g.ewkb)
    assert back.wkt == expected and back.srid == 5179
    assert g3.read_hexwkb(g.hex).wkt == expected


def test_postgis_fixtures():
    pg = g3.read_hexwkb("01010000A0E6100000000000000000F03F00000000000000400000000000000840")
    assert pg.ewkt == "SRID=4326;POINT Z (1 2 3)"
    iso = g3.read_hexwkb("01E9030000000000000000F03F00000000000000400000000000000840")
    assert iso.hex == "01E9030000000000000000F03F00000000000000400000000000000840"


def test_errors():
    with pytest.raises(ValueError, match="expected"):
        g3.read_wkt("POLYGON Z ((0 0 0, 1 1 1)")
    with pytest.raises(ValueError):
        g3.read_wkt("NOPE (1 2 3)")


def test_shapely_interop():
    shapely = pytest.importorskip("shapely")
    sp = shapely.Polygon([(0, 0, 0), (4, 0, 0), (4, 3, 0), (0, 3, 0)])
    g = g3.from_shapely(sp)
    assert g3.st_3darea(g) == pytest.approx(12.0)
    assert g.to_shapely().equals(sp)
