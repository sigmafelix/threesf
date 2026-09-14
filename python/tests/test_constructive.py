import warnings

import numpy as np
import pytest

import threesf as g3


def box(x=0, y=0, z=0, size=1):
    p = g3.polygon([(x, y, z), (x+size, y, z), (x+size, y+size, z), (x, y+size, z)])
    return g3.st_extrude(p, 0, 0, size)


def assert_valid(g):
    assert not g3.st_selfintersects(g)
    if not g.is_empty:
        assert g3.st_issolid(g)
    assert g3.read_wkb(g.wkb).wkt == g.wkt


@pytest.mark.parametrize("points,kind,measure", [
    ([(0, 0, 0)], "POINT", 0),
    ([(0, 0, 0), (2, 2, 2), (1, 1, 1)], "LINESTRING", 0),
    ([(0, 2, 0), (2, 2, 0), (2, 2, 2), (0, 2, 2)], "POLYGON", 4),
    ([(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)], "SOLID", 1/6),
])
def test_hull_dimensions(points, kind, measure):
    g = g3.multipoint(points)
    g.srid = 5179
    result = g3.st_3dconvexhull(g)
    assert result.geom_type == kind
    assert result.srid == 5179
    assert not g3.st_selfintersects(result)
    if kind == "SOLID":
        assert g3.st_volume(result) == pytest.approx(measure)
    elif kind == "POLYGON":
        assert g3.st_3darea(result) == pytest.approx(measure)


@pytest.mark.parametrize("operation,expected", [
    (g3.st_3dintersection, .125), (g3.st_3dunion, 1.875), (g3.st_3ddifference, .875),
])
def test_boolean_overlap(operation, expected):
    a, b = box(), box(.5, .5, .5)
    before = (a.wkt, b.wkt)
    result = operation(a, b)
    assert g3.st_volume(result) == pytest.approx(expected)
    assert_valid(result)
    assert (a.wkt, b.wkt) == before


def test_cavity_disconnected_and_empty():
    outer, inner = box(size=4), box(1, 1, 1, 2)
    cavity = g3.st_3ddifference(outer, inner)
    assert_valid(cavity)
    assert g3.st_volume(cavity) == pytest.approx(56)
    assert g3.st_3dintersection(cavity, inner).is_empty
    assert g3.st_volume(g3.st_3dunion(cavity, inner)) == pytest.approx(64)
    disconnected = g3.st_3dunion(box(), box(2))
    assert disconnected.geom_type == "MULTISOLID"
    assert_valid(disconnected)
    assert g3.st_volume(disconnected) == pytest.approx(2)
    assert g3.st_3ddifference(outer, outer).is_empty
    assert g3.st_3dintersection(box(), box(1)).is_empty
    assert g3.st_3dconvexhull(g3.read_wkt("MULTIPOINT Z EMPTY")).is_empty


def test_warning_and_rejection():
    with pytest.warns(RuntimeWarning, match="self-intersection"):
        bad = g3.polygon([(0, 0, 0), (2, 2, 0), (0, 2, 0), (2, 0, 0)])
    with pytest.warns(RuntimeWarning, match="self-intersection"):
        assert g3.st_selfintersects(bad)
    for operation in (g3.st_tesselate, lambda g: g3.st_extrude(g, 0, 0, 1), g3.st_3darea_tessellated):
        with pytest.warns(RuntimeWarning, match="self-intersection"), pytest.raises(ValueError, match="self-intersection"):
            operation(bad)
    with pytest.warns(RuntimeWarning, match="self-intersection"):
        hull = g3.st_3dconvexhull(bad)
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert g3.st_3darea(hull) == pytest.approx(4)
        assert not g3.st_selfintersects(hull)
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        with pytest.raises(RuntimeWarning):
            g3.read_wkb(bad.wkb)


def test_crossing_faces_warn_on_read_and_boolean():
    a, b = g3.triangles(box()), g3.triangles(box(.5, .5, .5))
    with pytest.warns(RuntimeWarning, match="self-intersection"):
        bad = g3.surface(np.concatenate([a, b]))
    other = box()
    with pytest.warns(RuntimeWarning, match="self-intersection"), pytest.raises(ValueError, match="self-intersection"):
        g3.st_3dunion(bad, other)


@pytest.mark.parametrize("tol", [0, -1, float("nan"), float("inf")])
def test_invalid_tolerance(tol):
    with pytest.raises(ValueError, match="tolerance"):
        g3.st_3dconvexhull(g3.point(0, 0, 0), tol)


def test_invalid_operands_and_srid():
    a = box()
    with pytest.raises(ValueError, match="solids or closed"):
        g3.st_3dunion(a, g3.point(0, 0, 0))
    b = box()
    b.srid = 4326
    with pytest.raises(ValueError, match="SRID"):
        g3.st_3ddifference(a, b)
    with pytest.raises(ValueError, match="non-tangent"):
        g3.st_extrude(g3.polygon([(0, 0), (1, 0), (0, 1)]), 0, 0, 0)


def test_large_offset():
    a, b = box(1e8, -1e8, 1e8), box(1e8+.5, -1e8, 1e8)
    h = g3.st_3dconvexhull(a)
    assert g3.st_volume(h) == pytest.approx(1)
    np.testing.assert_allclose(g3.st_3dcentroid(h), [1e8+.5, -1e8+.5, 1e8+.5], rtol=0, atol=1e-7)
    result = g3.st_3dintersection(a, b)
    assert g3.st_volume(result) == pytest.approx(.5)
    assert_valid(result)
