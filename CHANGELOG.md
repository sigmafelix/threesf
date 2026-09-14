# Changelog

## Unreleased
- Optional static and interactive 3D plotting in Python (Matplotlib/Plotly) and
  R (base graphics/Plotly), with shared native primitives preserving holes,
  curves, cavity shells and collections; image/HTML export examples and tests.
- 3D convex hull and regularized solid intersection, union and difference in
  C++, C, Python and R; cavities, disconnected outputs and degenerate hulls.
- Self-intersection detection and native warnings; unsafe constructive operations
  fail explicitly instead of returning partial or intersecting meshes.
- Stable local-origin normal, volume and centroid calculations.
- Build-artifact cleanup/ignore rules, vendored-header synchronization, and
  analytic/randomized native and language-binding regression coverage.

## 0.2.0 — 2026-09-13
- Full Simple Features Z WKT/EWKT and WKB/EWKB reader/writer (all SF types,
  SFCGAL SOLID/MULTISOLID, nested GEOMETRYCOLLECTION, EMPTY, both byte orders).
- Typed `Geometry` tree; measures recurse through collections.
- Opaque-handle C ABI (`threesf_read_wkt`, `threesf_h_*`, `threesf_last_error`).
- Python: pip-installable package, WKT/WKB/hex I/O, shapely interop.
- R: external-pointer handles, `sf` interop, testthat suite, R CMD check clean.
- CMake build, CTest, GitHub Actions CI (Linux/macOS/Windows, R).

## 0.1.0
- Initial header-only core: 3D length/perimeter/area/volume/centroid/extent,
  3D distance family, planarity/closure/solid predicates, tessellation, extrusion.
