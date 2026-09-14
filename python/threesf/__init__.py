"""threesf — 3D vector measures (PostGIS/SFCGAL-style) without PostGIS.

Geometries are Simple Features with Z. Build them from WKT/EWKT, WKB/EWKB,
hex WKB, shapely objects, or the coordinate constructors below; every
function is named after its PostGIS counterpart.

    >>> import threesf as g3
    >>> poly = g3.read_wkt("POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0))")
    >>> prism = g3.st_extrude(poly, 0, 0, 5)
    >>> g3.st_volume(prism), prism.geom_type, g3.st_astext(poly)
    (500.0, 'SOLID', 'POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0))')
"""
from __future__ import annotations

import ctypes
import ctypes.util
import os
import struct
import warnings
from typing import Iterable, Optional, Sequence

import numpy as np

__all__ = [
    "Geometry", "read_wkt", "read_wkb", "read_hexwkb", "from_shapely",
    "point", "multipoint", "linestring", "multilinestring", "polygon", "multipolygon",
    "surface", "tin", "solid", "collection",
    "st_geomfromtext", "st_geomfromwkb", "st_astext", "st_asewkt", "st_asbinary", "st_asewkb", "st_ashexewkb",
    "st_3dlength", "st_3dperimeter", "st_3darea", "st_3darea_tessellated", "st_volume", "st_signed_volume",
    "st_3dcentroid", "st_3dextent", "st_isplanar", "st_isclosed", "st_issolid", "st_orientation",
    "st_3ddistance", "st_3dmaxdistance", "st_3dclosestpoint", "st_3dshortestline", "st_3dlongestline",
    "st_3ddwithin", "st_3ddfullywithin", "st_3dintersects",
    "st_tesselate", "st_extrude", "st_makesolid", "st_force_outward", "st_forcepolygonccw", "st_forcepolygoncw",
    "st_3dconvexhull", "st_3dintersection", "st_3dunion", "st_3ddifference", "st_selfintersects",
    "plot", "version",
]

# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------
def _load() -> ctypes.CDLL:
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [os.environ.get("THREESF_LIBRARY")]
    import glob
    candidates += sorted(glob.glob(os.path.join(here, "_libthreesf*")))  # built by pip/setuptools
    candidates += [os.path.join(here, n) for n in ("libthreesf.so", "libthreesf.dylib", "threesf.dll", "libthreesf.dll")]  # make python
    candidates.append(ctypes.util.find_library("threesf"))
    for c in candidates:
        if c and os.path.exists(c):
            return ctypes.CDLL(c)
    raise OSError("libthreesf not found; `pip install .` (or `make python` in the repo) or set THREESF_LIBRARY")

_lib = _load()
_H = ctypes.c_void_p
_D = ctypes.c_double
_dbl3, _dbl6 = _D * 3, _D * 6
_sig = {
    "threesf_version": (ctypes.c_char_p, []), "threesf_last_error": (ctypes.c_char_p, []),
    "threesf_last_warning": (ctypes.c_char_p, []),
    "threesf_h_plot_data": (ctypes.c_int, [_H, ctypes.c_int, ctypes.POINTER(ctypes.POINTER(_D)), ctypes.POINTER(ctypes.c_size_t)]),
    "threesf_h_self_intersects": (ctypes.c_int, [_H, _D]),
    "threesf_h_convex_hull": (_H, [_H, _D]),
    "threesf_h_intersection": (_H, [_H, _H, _D]),
    "threesf_h_union": (_H, [_H, _H, _D]),
    "threesf_h_difference": (_H, [_H, _H, _D]),
    "threesf_free": (None, [ctypes.c_void_p]),
    "threesf_read_wkt": (_H, [ctypes.c_char_p]), "threesf_read_wkb": (_H, [ctypes.c_char_p, ctypes.c_size_t]),
    "threesf_read_hexwkb": (_H, [ctypes.c_char_p]),
    "threesf_h_free": (None, [_H]), "threesf_h_clone": (_H, [_H]),
    "threesf_h_wkt": (ctypes.c_void_p, [_H, ctypes.c_int]),
    "threesf_h_wkb": (ctypes.c_void_p, [_H, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t)]),
    "threesf_h_hexwkb": (ctypes.c_void_p, [_H, ctypes.c_int]),
    "threesf_h_type": (ctypes.c_char_p, [_H]), "threesf_h_srid": (ctypes.c_int, [_H]),
    "threesf_h_set_srid": (None, [_H, ctypes.c_int]), "threesf_h_is_empty": (ctypes.c_int, [_H]),
    "threesf_h_num_vertices": (ctypes.c_size_t, [_H]),
    "threesf_h_length": (_D, [_H]), "threesf_h_perimeter": (_D, [_H]), "threesf_h_area": (_D, [_H]),
    "threesf_h_area_tessellated": (_D, [_H]), "threesf_h_volume": (_D, [_H]), "threesf_h_signed_volume": (_D, [_H]),
    "threesf_h_centroid": (ctypes.c_int, [_H, _dbl3]), "threesf_h_bbox": (ctypes.c_int, [_H, _dbl6]),
    "threesf_h_is_planar": (ctypes.c_int, [_H, _D]), "threesf_h_is_closed": (ctypes.c_int, [_H]),
    "threesf_h_is_solid": (ctypes.c_int, [_H]), "threesf_h_orientation": (ctypes.c_int, [_H]),
    "threesf_h_distance": (_D, [_H, _H]), "threesf_h_max_distance": (_D, [_H, _H]),
    "threesf_h_closest_points": (ctypes.c_int, [_H, _H, _dbl6]), "threesf_h_farthest_points": (ctypes.c_int, [_H, _H, _dbl6]),
    "threesf_h_dwithin": (ctypes.c_int, [_H, _H, _D]), "threesf_h_dfully_within": (ctypes.c_int, [_H, _H, _D]),
    "threesf_h_intersects": (ctypes.c_int, [_H, _H, _D]),
    "threesf_h_tessellate": (_H, [_H]), "threesf_h_extrude": (_H, [_H, _D, _D, _D]),
    "threesf_h_make_solid": (_H, [_H]), "threesf_h_force_outward": (_H, [_H]),
    "threesf_h_force_ccw": (_H, [_H]), "threesf_h_force_cw": (_H, [_H]),
}
for _name, (_res, _args) in _sig.items():
    _f = getattr(_lib, _name); _f.restype, _f.argtypes = _res, _args

def _err() -> str:
    return (_lib.threesf_last_error() or b"").decode()

def _warn() -> None:
    message = _lib.threesf_last_warning()
    if message: warnings.warn(message.decode(), RuntimeWarning, stacklevel=3)

def _take_str(ptr) -> str:
    if not ptr: raise MemoryError("allocation failed")
    try: return ctypes.string_at(ptr).decode()
    finally: _lib.threesf_free(ptr)

# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------
class Geometry:
    """Opaque handle to a native Simple Features Z geometry."""
    __slots__ = ("_h",)

    def __init__(self, handle: int):
        self._h = handle
        _warn()
        if not handle: raise ValueError(_err() or "invalid geometry")

    def __del__(self):
        h = getattr(self, "_h", None)
        if h: _lib.threesf_h_free(h); self._h = None

    def __repr__(self) -> str:
        w = self.wkt
        return f"<threesf {w if len(w) <= 80 else w[:77] + '...'}>"

    def __copy__(self): return Geometry(_lib.threesf_h_clone(self._h))
    copy = __copy__

    @property
    def geom_type(self) -> str: return _lib.threesf_h_type(self._h).decode()
    @property
    def srid(self) -> int: return _lib.threesf_h_srid(self._h)
    @srid.setter
    def srid(self, v: int) -> None: _lib.threesf_h_set_srid(self._h, int(v))
    @property
    def is_empty(self) -> bool: return _lib.threesf_h_is_empty(self._h) == 1
    @property
    def num_vertices(self) -> int: return _lib.threesf_h_num_vertices(self._h)
    @property
    def wkt(self) -> str: return _take_str(_lib.threesf_h_wkt(self._h, 0))
    @property
    def ewkt(self) -> str: return _take_str(_lib.threesf_h_wkt(self._h, 1))
    @property
    def wkb(self) -> bytes: return self.to_wkb(ewkb=False)
    @property
    def ewkb(self) -> bytes: return self.to_wkb(ewkb=True)
    @property
    def hex(self) -> str: return _take_str(_lib.threesf_h_hexwkb(self._h, 0))

    def to_wkb(self, ewkb: bool = False) -> bytes:
        n = ctypes.c_size_t()
        p = _lib.threesf_h_wkb(self._h, int(ewkb), ctypes.byref(n))
        if not p: raise MemoryError("allocation failed")
        try: return ctypes.string_at(p, n.value)
        finally: _lib.threesf_free(p)

    def plot(self, **kwargs):
        """Plot in 3D; see :func:`plot` for static and interactive options."""
        return plot(self, **kwargs)

    def to_shapely(self):
        """Point/Line/Polygon families only (shapely has no surface/solid types)."""
        import shapely
        return shapely.from_wkb(self.wkb)

# Readers --------------------------------------------------------------------
def read_wkt(text: str) -> Geometry:
    return Geometry(_lib.threesf_read_wkt(text.encode()))

def read_wkb(data: bytes) -> Geometry:
    data = bytes(data)
    return Geometry(_lib.threesf_read_wkb(data, len(data)))

def read_hexwkb(hexstr: str) -> Geometry:
    return Geometry(_lib.threesf_read_hexwkb(hexstr.encode()))

def from_shapely(geom) -> Geometry:
    """Any shapely geometry (Z is kept; 2D gets z = 0)."""
    return read_wkb(geom.wkb)

st_geomfromtext, st_geomfromwkb = read_wkt, read_wkb
def st_astext(g: Geometry) -> str: return g.wkt
def st_asewkt(g: Geometry) -> str: return g.ewkt
def st_asbinary(g: Geometry) -> bytes: return g.wkb
def st_asewkb(g: Geometry) -> bytes: return g.ewkb
def st_ashexewkb(g: Geometry) -> str: return _take_str(_lib.threesf_h_hexwkb(g._h, 1))

# Coordinate constructors (encoded as ISO WKB, so doubles are exact) ---------
_ISO = {"POINT": 1001, "LINESTRING": 1002, "POLYGON": 1003, "MULTIPOINT": 1004, "MULTILINESTRING": 1005,
        "MULTIPOLYGON": 1006, "GEOMETRYCOLLECTION": 1007, "POLYHEDRALSURFACE": 1015, "TIN": 1016, "TRIANGLE": 1017,
        "SOLID": 1101}

def _xyz(pts) -> np.ndarray:
    a = np.ascontiguousarray(np.asarray(pts, dtype=np.float64))
    if a.ndim != 2 or a.shape[1] not in (2, 3):
        raise ValueError(f"expected (n, 2|3) coordinates, got shape {a.shape}")
    if a.shape[1] == 2: a = np.column_stack([a, np.zeros(len(a))])
    return a

def _hdr(t: str) -> bytes: return b"\x01" + struct.pack("<I", _ISO[t])
def _ring(pts) -> bytes:
    a = _xyz(pts)
    if len(a) and not np.array_equal(a[0], a[-1]): a = np.vstack([a, a[:1]])  # close ring
    return struct.pack("<I", len(a)) + a.tobytes()
def _line(pts) -> bytes: a = _xyz(pts); return struct.pack("<I", len(a)) + a.tobytes()
def _poly_body(rings) -> bytes: return struct.pack("<I", len(rings)) + b"".join(_ring(r) for r in rings)
def _poly(rings, t="POLYGON") -> bytes: return _hdr(t) + _poly_body(rings)
def _norm_patch(p):  # a patch is an (n,3) ring or [exterior, *holes]
    return p if (isinstance(p, (list, tuple)) and len(p) and np.ndim(p[0]) == 2) else [p]
def _surface(patches, t="POLYHEDRALSURFACE") -> bytes:
    ps = [_norm_patch(p) for p in patches]
    return _hdr(t) + struct.pack("<I", len(ps)) + b"".join(_poly(p, "TRIANGLE" if t == "TIN" else "POLYGON") for p in ps)

def point(x: float, y: float, z: float = 0.0) -> Geometry:
    return read_wkb(_hdr("POINT") + struct.pack("<ddd", x, y, z))
def multipoint(pts) -> Geometry:
    a = _xyz(pts)
    return read_wkb(_hdr("MULTIPOINT") + struct.pack("<I", len(a)) + b"".join(_hdr("POINT") + p.tobytes() for p in a))
def linestring(pts) -> Geometry: return read_wkb(_hdr("LINESTRING") + _line(pts))
def multilinestring(lines: Iterable) -> Geometry:
    ls = list(lines)
    return read_wkb(_hdr("MULTILINESTRING") + struct.pack("<I", len(ls)) + b"".join(_hdr("LINESTRING") + _line(l) for l in ls))
def polygon(exterior, holes: Optional[Iterable] = None) -> Geometry:
    return read_wkb(_poly([exterior, *(holes or [])]))
def multipolygon(polys: Iterable[Sequence]) -> Geometry:
    """polys: iterable of [exterior, *holes]."""
    ps = [_norm_patch(p) for p in polys]
    return read_wkb(_hdr("MULTIPOLYGON") + struct.pack("<I", len(ps)) + b"".join(_poly(p) for p in ps))
def surface(patches: Iterable) -> Geometry:
    """POLYHEDRALSURFACE Z. Each patch is an (n,3) ring or [exterior, *holes]."""
    return read_wkb(_surface(list(patches)))
def tin(triangles: Iterable) -> Geometry: return read_wkb(_surface(list(triangles), "TIN"))
def solid(outer: Iterable, inner: Optional[Iterable[Iterable]] = None) -> Geometry:
    """SOLID Z (SFCGAL): outer shell patches plus optional cavity shells."""
    shells = [list(outer), *(list(s) for s in (inner or []))]
    return read_wkb(_hdr("SOLID") + struct.pack("<I", len(shells)) + b"".join(_surface(s) for s in shells))
def collection(geoms: Iterable[Geometry]) -> Geometry:
    gs = list(geoms)
    return read_wkb(_hdr("GEOMETRYCOLLECTION") + struct.pack("<I", len(gs)) + b"".join(g.wkb for g in gs))

# ---------------------------------------------------------------------------
# Functions
# ---------------------------------------------------------------------------
def version() -> str: return _lib.threesf_version().decode()

def _num(v: float, what: str) -> float:
    _warn()
    if v != v: raise ValueError(f"{what}: {_err() or 'invalid geometry'}")
    return v
def _pred(rc: int) -> bool:
    _warn()
    if rc < 0: raise ValueError(_err() or "invalid geometry for predicate")
    return bool(rc)
def _vec(rc: int, buf, shape=None) -> np.ndarray:
    _warn()
    if rc != 0: raise ValueError(_err() or "invalid geometry")
    a = np.array(buf); return a.reshape(shape) if shape else a

def st_3dlength(g: Geometry) -> float: return _num(_lib.threesf_h_length(g._h), "length")
def st_3dperimeter(g: Geometry) -> float: return _num(_lib.threesf_h_perimeter(g._h), "perimeter")
def st_3darea(g: Geometry) -> float: return _num(_lib.threesf_h_area(g._h), "area")
def st_3darea_tessellated(g: Geometry) -> float: return _num(_lib.threesf_h_area_tessellated(g._h), "area")
def st_volume(g: Geometry) -> float: return _num(_lib.threesf_h_volume(g._h), "volume")
def st_signed_volume(g: Geometry) -> float: return _num(_lib.threesf_h_signed_volume(g._h), "signed volume")
def st_3dcentroid(g: Geometry) -> np.ndarray: b = _dbl3(); return _vec(_lib.threesf_h_centroid(g._h, b), b)
def st_3dextent(g: Geometry) -> np.ndarray:
    """[xmin, ymin, zmin, xmax, ymax, zmax]"""
    b = _dbl6(); return _vec(_lib.threesf_h_bbox(g._h, b), b)

def st_isplanar(g: Geometry, tol: float = 1e-9) -> bool: return _pred(_lib.threesf_h_is_planar(g._h, tol))
def st_isclosed(g: Geometry) -> bool: return _pred(_lib.threesf_h_is_closed(g._h))
def st_issolid(g: Geometry) -> bool: return _pred(_lib.threesf_h_is_solid(g._h))
def st_orientation(g: Geometry) -> int: return int(_lib.threesf_h_orientation(g._h))

def st_3ddistance(a: Geometry, b: Geometry) -> float: return _num(_lib.threesf_h_distance(a._h, b._h), "distance")
def st_3dmaxdistance(a: Geometry, b: Geometry) -> float: return _num(_lib.threesf_h_max_distance(a._h, b._h), "max distance")
def st_3dshortestline(a: Geometry, b: Geometry) -> np.ndarray:
    """(2, 3): row 0 on a, row 1 on b."""
    buf = _dbl6(); return _vec(_lib.threesf_h_closest_points(a._h, b._h, buf), buf, (2, 3))
def st_3dclosestpoint(a: Geometry, b: Geometry) -> np.ndarray: return st_3dshortestline(a, b)[0]
def st_3dlongestline(a: Geometry, b: Geometry) -> np.ndarray:
    buf = _dbl6(); return _vec(_lib.threesf_h_farthest_points(a._h, b._h, buf), buf, (2, 3))
def st_3ddwithin(a: Geometry, b: Geometry, d: float) -> bool: return _pred(_lib.threesf_h_dwithin(a._h, b._h, d))
def st_3ddfullywithin(a: Geometry, b: Geometry, d: float) -> bool: return _pred(_lib.threesf_h_dfully_within(a._h, b._h, d))
def st_3dintersects(a: Geometry, b: Geometry, eps: float = 1e-9) -> bool: return _pred(_lib.threesf_h_intersects(a._h, b._h, eps))

def st_tesselate(g: Geometry) -> Geometry:
    """TIN Z of all surface content."""
    return Geometry(_lib.threesf_h_tessellate(g._h))
def st_extrude(g: Geometry, dx: float, dy: float, dz: float) -> Geometry:
    """SOLID Z swept from the first polygon of g."""
    return Geometry(_lib.threesf_h_extrude(g._h, dx, dy, dz))
def st_makesolid(g: Geometry) -> Geometry: return Geometry(_lib.threesf_h_make_solid(g._h))
def st_force_outward(g: Geometry) -> Geometry: return Geometry(_lib.threesf_h_force_outward(g._h))
def st_forcepolygonccw(g: Geometry) -> Geometry: return Geometry(_lib.threesf_h_force_ccw(g._h))
def st_forcepolygoncw(g: Geometry) -> Geometry: return Geometry(_lib.threesf_h_force_cw(g._h))

def triangles(g: Geometry) -> np.ndarray:
    """Tessellate and return an (n, 3, 3) numpy array of triangle vertices."""
    t = st_tesselate(g)
    data = t.wkb
    # ISO WKB TIN: hdr(5) count(4) then per triangle hdr(5) nrings(4) npts(4) 4*24 bytes
    n = struct.unpack_from("<I", data, 5)[0]
    out = np.empty((n, 3, 3)); off = 9
    for i in range(n):
        off += 5 + 4 + 4
        out[i] = np.frombuffer(data, dtype="<f8", count=12, offset=off).reshape(4, 3)[:3]
        off += 4 * 24
    return out


def st_selfintersects(g: Geometry, tol: float = 1e-9) -> bool:
    """Detect crossing rings/faces; warn when a self-intersection is found."""
    return _pred(_lib.threesf_h_self_intersects(g._h, tol))

def st_3dconvexhull(g: Geometry, tol: float = 1e-9) -> Geometry:
    """Hull of all vertices: SOLID, POLYGON, LINESTRING, POINT, or EMPTY."""
    return Geometry(_lib.threesf_h_convex_hull(g._h, tol))

def st_3dintersection(a: Geometry, b: Geometry, tol: float = 1e-9) -> Geometry:
    """Regularized solid intersection; boundary-only contact returns EMPTY."""
    return Geometry(_lib.threesf_h_intersection(a._h, b._h, tol))

def st_3dunion(a: Geometry, b: Geometry, tol: float = 1e-9) -> Geometry:
    """Union of closed solids/surfaces, preserving cavities and components."""
    return Geometry(_lib.threesf_h_union(a._h, b._h, tol))

def st_3ddifference(a: Geometry, b: Geometry, tol: float = 1e-9) -> Geometry:
    """Regularized solid difference a minus b."""
    return Geometry(_lib.threesf_h_difference(a._h, b._h, tol))


from .plotting import plot
