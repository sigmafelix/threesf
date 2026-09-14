# threesf (Python)

3D vector measures for Simple Features Z geometries — `ST_3DArea`, `ST_Volume`,
`ST_3DLength`, `ST_3DDistance`, `ST_Tesselate`, `ST_Extrude` and friends — without
PostGIS. Reads/writes WKT, EWKT, WKB, EWKB, hex; interoperates with shapely via WKB.

```sh
pip install .            # compiles the C++ core (needs a C++17 compiler)
```

```python
import threesf as g3
poly = g3.read_wkt("POLYGON Z ((0 0 0, 10 0 0, 10 10 0, 0 10 0, 0 0 0))")
prism = g3.st_extrude(poly, 0, 0, 5)
g3.st_volume(prism), prism.geom_type      # (500.0, 'SOLID')
```

See the repository README for the full function table and algorithm notes.

3D construction is available through `st_3dconvexhull`, `st_3dintersection`,
`st_3dunion`, and `st_3ddifference`. Booleans accept closed solids/surfaces and
return solids (including cavities and disconnected components), or EMPTY for
boundary-only intersections. All accept an absolute `tol=1e-9`.

`st_selfintersects` detects crossing rings/faces. Invalid input emits a
`RuntimeWarning`; unsafe construction also raises `ValueError`. A convex hull
can still use self-intersecting input's vertices after warning. The kernel uses
doubles and rejects numerically unresolved results; it does not repair meshes.

Plotting is optional: install `pip install 'threesf[plot]'` (or
`pip install '.[plot]'` from this directory) for Matplotlib and Plotly.

```python
ax = g3.plot(prism, color="steelblue", alpha=.8)  # static 3D axes
ax.figure.savefig("prism.png")
fig = prism.plot(interactive=True)                # Plotly Figure
fig.show()
fig.write_html("prism.html")
```

Pass a list to `g3.plot` with one color per geometry to compare objects.
`edges=False` hides mesh edges while retaining curves. Both modes support all
geometry families and empty inputs; faces use core tessellation to preserve holes.
No backend is imported until plotting is requested. Static and transparent plots
use approximate depth sorting; use the interactive camera for spatial inspection.
