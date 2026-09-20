# threesf R API. A "threesf" object is an external pointer to a native Simple
# Features Z geometry; build one from WKT/WKB, sf, or the g3_* constructors.

# I/O ------------------------------------------------------------------------
#' Read a geometry from WKT or EWKT
#'
#' Parses a WKT or EWKT string into a native `threesf` geometry. Two-dimensional
#' input is promoted to Z coordinates with zero elevation.
#'
#' @param wkt A WKT or EWKT string.
#' @return A `threesf` geometry external pointer.
#' @export
#' @examples
#' st_geomfromtext("POINT Z (1 2 3)")
st_geomfromtext <- function(wkt) .Call(R_read_wkt, as.character(wkt))
#' Read a geometry from WKB or EWKB
#'
#' Accepts either a raw vector or a hexadecimal string.
#'
#' @param wkb A raw WKB/EWKB vector or hexadecimal WKB/EWKB string.
#' @return A `threesf` geometry external pointer.
#' @export
#' @examples
#' st_geomfromwkb(st_asbinary(st_geomfromtext("POINT Z (1 2 3)")))
st_geomfromwkb <- function(wkb) {
  if (is.character(wkb)) .Call(R_read_hexwkb, wkb) else .Call(R_read_wkb, as.raw(wkb))
}
#' Convert a geometry to WKT
#'
#' @param g A `threesf` geometry.
#' @return A character string containing WKT without an SRID prefix.
#' @export
#' @examples
#' st_astext(st_geomfromtext("POINT Z (1 2 3)"))
st_astext <- function(g) .Call(R_wkt, g, FALSE)
#' Convert a geometry to EWKT
#'
#' @param g A `threesf` geometry.
#' @return A character string containing EWKT, including an SRID when set.
#' @export
#' @examples
#' st_asewkt(st_setsrid(st_geomfromtext("POINT Z (1 2 3)"), 4326))
st_asewkt <- function(g) .Call(R_wkt, g, TRUE)
#' Convert a geometry to WKB
#'
#' @param g A `threesf` geometry.
#' @return A raw vector containing WKB without an SRID.
#' @export
#' @examples
#' st_asbinary(st_geomfromtext("POINT Z (1 2 3)"))
st_asbinary <- function(g) .Call(R_wkb, g, FALSE)
#' Convert a geometry to EWKB
#'
#' @param g A `threesf` geometry.
#' @return A raw vector containing EWKB, including an SRID when set.
#' @export
#' @examples
#' st_asewkb(st_setsrid(st_geomfromtext("POINT Z (1 2 3)"), 4326))
st_asewkb <- function(g) .Call(R_wkb, g, TRUE)
#' Convert a geometry to hexadecimal EWKB
#'
#' @param g A `threesf` geometry.
#' @return A hexadecimal character representation of EWKB.
#' @export
#' @examples
#' st_ashexewkb(st_geomfromtext("POINT Z (1 2 3)"))
st_ashexewkb <- function(g) .Call(R_hexwkb, g, TRUE)
#' Get the geometry type
#'
#' @param g A `threesf` geometry.
#' @return A character scalar such as `"POINT"` or `"SOLID"`.
#' @export
#' @examples
#' st_geometrytype(st_geomfromtext("POINT Z (1 2 3)"))
st_geometrytype <- function(g) .Call(R_type, g)
#' Get a geometry SRID
#'
#' @param g A `threesf` geometry.
#' @return An integer SRID, or zero when no SRID is set.
#' @export
#' @examples
#' st_srid(st_geomfromtext("SRID=4326;POINT Z (1 2 3)"))
st_srid <- function(g) .Call(R_srid, g)
#' Set a geometry SRID
#'
#' @param g A `threesf` geometry.
#' @param srid An integer spatial reference identifier.
#' @return A `threesf` geometry with the requested SRID.
#' @export
#' @examples
#' st_setsrid(st_geomfromtext("POINT Z (1 2 3)"), 4326)
st_setsrid <- function(g, srid) .Call(R_set_srid, g, as.integer(srid))
#' Test whether a geometry is empty
#'
#' @param g A `threesf` geometry.
#' @return A logical scalar.
#' @export
#' @examples
#' st_isempty(st_geomfromtext("POINT Z EMPTY"))
st_isempty <- function(g) .Call(R_is_empty, g)
#' Count geometry vertices
#'
#' @param g A `threesf` geometry.
#' @return An integer vertex count.
#' @export
#' @examples
#' st_npoints(st_geomfromtext("LINESTRING Z (0 0 0, 1 1 1)"))
st_npoints <- function(g) .Call(R_num_vertices, g)

#' Print a threesf geometry
#'
#' @param x A `threesf` geometry.
#' @param ... Additional arguments, currently ignored.
#' @return `x`, invisibly.
#' @export
#' @examples
#' print(g3_point(1, 2, 3))
print.threesf <- function(x, ...) {
  w <- st_astext(x)
  if (nchar(w) > 100) w <- paste0(substr(w, 1, 97), "...")
  cat("<threesf ", w, ">\n", sep = "")
  invisible(x)
}

# sf interop: any sfg/sfc/sf with Z (2D gets z = 0). WKB avoids text rounding.
#' Convert an sf geometry to threesf
#'
#' Accepts `sf`, `sfc`, or `sfg` objects and preserves Z coordinates. Two-dimensional
#' input receives zero Z coordinates.
#'
#' @param x An `sf`, `sfc`, or `sfg` object.
#' @return A single `threesf` geometry or a list of them.
#' @export
#' @examples
#' if (requireNamespace("sf", quietly = TRUE)) {
#'   g3_from_sf(sf::st_point(c(1, 2, 3)))
#' }
g3_from_sf <- function(x) {
  if (inherits(x, "sf")) x <- sf::st_geometry(x)
  if (inherits(x, "sfg")) x <- sf::st_sfc(x)
  out <- lapply(sf::st_as_binary(x), function(b) .Call(R_read_wkb, b))
  if (length(out) == 1L) out[[1L]] else out
}
#' Back to sf (POINT/LINESTRING/POLYGON families and their MULTI* only; sf has no SOLID).
#' Convert a threesf geometry to sf
#'
#' Conversion supports point, line, polygon, and corresponding multi-geometry
#' families. `sf` has no representation for SFCGAL solids.
#'
#' @param g A `threesf` geometry or a list of them.
#' @param crs A CRS passed to `sf::st_as_sfc()`.
#' @return An `sf` `sfc` geometry vector.
#' @export
#' @examples
#' if (requireNamespace("sf", quietly = TRUE)) {
#'   g3_to_sf(g3_point(1, 2, 3))
#' }
g3_to_sf <- function(g, crs = NA) {
  if (inherits(g, "threesf")) g <- list(g)
  sf::st_as_sfc(structure(lapply(g, st_asbinary), class = "WKB"), crs = crs)
}

# Coordinate constructors ----------------------------------------------------
.xyz <- function(m) {
  m <- as.matrix(m)
  storage.mode(m) <- "double"
  if (ncol(m) == 2L) m <- cbind(m, 0)
  if (ncol(m) != 3L) stop("expected an n x 2 or n x 3 coordinate matrix")
  m
}
.fmt <- function(m) paste(apply(.xyz(m), 1L, function(p) paste(sprintf("%.17g", p), collapse = " ")), collapse = ", ")
.ring <- function(m) {
  m <- .xyz(m)
  if (!all(m[1L, ] == m[nrow(m), ])) m <- rbind(m, m[1L, ])
  paste0("(", .fmt(m), ")")
}
.poly <- function(rings) paste0("(", paste(vapply(rings, .ring, ""), collapse = ", "), ")")
.norm_patch <- function(p) if (is.list(p)) p else list(p)
.surface <- function(patches) paste0("(", paste(vapply(lapply(patches, .norm_patch), .poly, ""), collapse = ", "), ")")

#' Construct a 3D point
#'
#' @param x,y Numeric X and Y coordinates.
#' @param z Numeric Z coordinate, defaulting to zero.
#' @return A `threesf` POINT geometry.
#' @export
#' @examples
#' g3_point(1, 2, 3)
g3_point <- function(x, y, z = 0) st_geomfromtext(sprintf("POINT Z (%.17g %.17g %.17g)", x, y, z))
#' Construct a 3D multipoint
#'
#' @param pts A numeric matrix with two or three columns and one point per row.
#' @return A `threesf` MULTIPOINT geometry.
#' @export
#' @examples
#' g3_multipoint(rbind(c(0, 0, 0), c(1, 1, 1)))
g3_multipoint <- function(pts) st_geomfromtext(paste0("MULTIPOINT Z (", .fmt(pts), ")"))
#' Construct a 3D linestring
#'
#' @param pts A numeric matrix with two or three columns and one vertex per row.
#' @return A `threesf` LINESTRING geometry.
#' @export
#' @examples
#' g3_linestring(rbind(c(0, 0, 0), c(1, 1, 1)))
g3_linestring <- function(pts) st_geomfromtext(paste0("LINESTRING Z (", .fmt(pts), ")"))
#' Construct 3D multilinestring geometry
#'
#' @param lines A list of numeric coordinate matrices.
#' @return A `threesf` MULTILINESTRING geometry.
#' @export
#' @examples
#' g3_multilinestring(list(rbind(c(0, 0, 0), c(1, 1, 1))))
g3_multilinestring <- function(lines) st_geomfromtext(paste0("MULTILINESTRING Z (", paste(vapply(lines, function(l) paste0("(", .fmt(l), ")"), ""), collapse = ", "), ")"))
#' Construct a 3D polygon
#'
#' @param exterior A numeric matrix defining the exterior ring.
#' @param holes A list of numeric matrices defining interior rings.
#' @return A `threesf` POLYGON geometry.
#' @export
#' @examples
#' g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0)))
g3_polygon <- function(exterior, holes = list()) st_geomfromtext(paste0("POLYGON Z ", .poly(c(list(exterior), holes))))
#' Construct a 3D multipolygon
#'
#' @param polys A list of exterior-ring matrices or lists containing an exterior
#'   ring followed by hole matrices.
#' @return A `threesf` MULTIPOLYGON geometry.
#' @export
#' @examples
#' g3_multipolygon(list(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
g3_multipolygon <- function(polys) st_geomfromtext(paste0("MULTIPOLYGON Z (", paste(vapply(lapply(polys, .norm_patch), .poly, ""), collapse = ", "), ")"))
#' Construct a 3D polyhedral surface
#'
#' @param patches A list of ring matrices or lists containing an exterior ring
#'   followed by hole matrices.
#' @return A `threesf` POLYHEDRALSURFACE geometry.
#' @export
#' @examples
#' g3_surface(list(rbind(c(0, 0, 0), c(1, 0, 0), c(0, 1, 0))))
g3_surface <- function(patches) st_geomfromtext(paste0("POLYHEDRALSURFACE Z ", .surface(patches)))
#' Construct a 3D triangulated irregular network
#'
#' @param triangles A list of triangular ring matrices or patch lists.
#' @return A `threesf` TIN geometry.
#' @export
#' @examples
#' g3_tin(list(rbind(c(0, 0, 0), c(1, 0, 0), c(0, 1, 0))))
g3_tin <- function(triangles) st_geomfromtext(paste0("TIN Z ", .surface(triangles)))
#' Construct a 3D solid
#'
#' Builds an SFCGAL-style SOLID Z from an outer shell and optional cavity shells.
#'
#' @param outer A list of outer-shell patches.
#' @param inner A list of cavity shells, each represented by a list of patches.
#' @return A `threesf` SOLID geometry.
#' @export
#' @examples
#' g3_solid(list(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
g3_solid <- function(outer, inner = list()) st_geomfromtext(paste0("SOLID Z (", paste(vapply(c(list(outer), inner), .surface, ""), collapse = ", "), ")"))
#' Construct a 3D geometry collection
#'
#' @param geoms A list of `threesf` geometries.
#' @return A `threesf` GEOMETRYCOLLECTION geometry.
#' @export
#' @examples
#' g3_collection(list(g3_point(0, 0, 0), g3_point(1, 1, 1)))
g3_collection <- function(geoms) st_geomfromtext(paste0("GEOMETRYCOLLECTION Z (", paste(vapply(geoms, st_astext, ""), collapse = ", "), ")"))

# Measures -------------------------------------------------------------------
#' Compute 3D length
#'
#' @param g A `threesf` geometry.
#' @return The 3D length as a numeric scalar.
#' @export
#' @examples
#' st_3dlength(g3_linestring(rbind(c(0, 0, 0), c(1, 2, 2))))
st_3dlength <- function(g) .Call(R_length, g)
#' Compute 3D perimeter
#'
#' @param g A `threesf` geometry.
#' @return The 3D perimeter as a numeric scalar.
#' @export
#' @examples
#' st_3dperimeter(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_3dperimeter <- function(g) .Call(R_perimeter, g)
#' Compute 3D area
#'
#' @param g A `threesf` geometry.
#' @return The area as a numeric scalar. Holes are subtracted for planar polygons.
#' @export
#' @examples
#' st_3darea(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_3darea <- function(g) .Call(R_area, g)
#' Compute tessellated 3D area
#'
#' @param g A `threesf` geometry.
#' @return The tessellated surface area as a numeric scalar.
#' @export
#' @examples
#' st_3darea_tessellated(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_3darea_tessellated <- function(g) .Call(R_area_tess, g)
#' Compute solid volume
#'
#' @param g A closed `threesf` surface or solid geometry.
#' @return The volume as a numeric scalar.
#' @export
#' @examples
#' st_volume(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1))
st_volume <- function(g) .Call(R_volume, g)
#' Compute signed solid volume
#'
#' @param g A closed `threesf` surface or solid geometry.
#' @return The signed volume as a numeric scalar; orientation determines its sign.
#' @export
#' @examples
#' st_signed_volume(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1))
st_signed_volume <- function(g) .Call(R_signed_volume, g)
#' Compute a 3D centroid
#'
#' @param g A `threesf` geometry.
#' @return A numeric vector of length three containing X, Y, and Z coordinates.
#' @export
#' @examples
#' st_3dcentroid(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_3dcentroid <- function(g) .Call(R_centroid, g)
#' Compute a 3D bounding extent
#'
#' @param g A `threesf` geometry.
#' @return A numeric vector `c(xmin, ymin, zmin, xmax, ymax, zmax)`.
#' @export
#' @examples
#' st_3dextent(g3_point(1, 2, 3))
st_3dextent <- function(g) .Call(R_bbox, g)

# Predicates -----------------------------------------------------------------
#' Test whether a geometry is planar
#'
#' @param g A `threesf` geometry.
#' @param tol Maximum vertex distance from the best-fit plane.
#' @return A logical scalar.
#' @export
#' @examples
#' st_isplanar(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_isplanar <- function(g, tol = 1e-9) .Call(R_is_planar, g, tol)
#' Test whether a surface is closed
#'
#' @param g A `threesf` surface geometry.
#' @return A logical scalar. A closed surface has every edge shared by two patches.
#' @export
#' @examples
#' st_isclosed(g3_linestring(rbind(c(0, 0, 0), c(1, 1, 1))))
st_isclosed <- function(g) .Call(R_is_closed, g)
#' Test whether a geometry is a solid
#'
#' @param g A `threesf` geometry.
#' @return A logical scalar indicating whether `g` is a consistently oriented solid.
#' @export
#' @examples
#' st_issolid(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1))
st_issolid <- function(g) .Call(R_is_solid, g)
#' Get polygon orientation
#'
#' @param g A `threesf` polygon geometry.
#' @return `1` for counterclockwise orientation, `-1` for clockwise orientation,
#'   or `0` when orientation is undefined.
#' @export
#' @examples
#' st_orientation(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_orientation <- function(g) .Call(R_orientation, g)

# Binary ---------------------------------------------------------------------
#' Compute 3D distance
#'
#' @param a,b `threesf` geometries.
#' @return The minimum 3D distance as a numeric scalar.
#' @export
#' @examples
#' st_3ddistance(g3_point(0.5, 0.5, 2), g3_point(0.5, 0.5, 0))
st_3ddistance <- function(a, b) .Call(R_distance, a, b)
#' Compute maximum 3D distance
#'
#' @param a,b `threesf` geometries.
#' @return The maximum 3D distance as a numeric scalar.
#' @export
#' @examples
#' st_3dmaxdistance(g3_point(0, 0, 0), g3_point(1, 1, 1))
st_3dmaxdistance <- function(a, b) .Call(R_max_distance, a, b)
#' Find the shortest connecting line
#'
#' @param a,b `threesf` geometries.
#' @return A 2 by 3 numeric matrix: the closest point on `a` in row one and
#'   the closest point on `b` in row two.
#' @export
#' @examples
#' st_3dshortestline(g3_point(0, 0, 2), g3_point(0, 0, 0))
st_3dshortestline <- function(a, b) matrix(.Call(R_closest_points, a, b), 2L, 3L, byrow = TRUE)
#' Find the closest point on the first geometry
#'
#' @param a,b `threesf` geometries.
#' @return A numeric vector of length three containing the closest point on `a`.
#' @export
#' @examples
#' st_3dclosestpoint(g3_point(0, 0, 2), g3_point(0, 0, 0))
st_3dclosestpoint <- function(a, b) st_3dshortestline(a, b)[1L, ]
#' Find the longest connecting line
#'
#' @param a,b `threesf` geometries.
#' @return A 2 by 3 numeric matrix containing the farthest points on `a` and `b`.
#' @export
#' @examples
#' st_3dlongestline(g3_point(0, 0, 2), g3_point(0, 0, 0))
st_3dlongestline <- function(a, b) matrix(.Call(R_farthest_points, a, b), 2L, 3L, byrow = TRUE)
#' Test whether geometries are within a distance
#'
#' @param a,b `threesf` geometries.
#' @param d Non-negative distance threshold.
#' @return A logical scalar.
#' @export
#' @examples
#' st_3ddwithin(g3_point(0, 0, 1), g3_point(0, 0, 0), 1)
st_3ddwithin <- function(a, b, d) .Call(R_dwithin, a, b, d)
#' Test whether all of one geometry is within a distance
#'
#' @param a,b `threesf` geometries.
#' @param d Non-negative distance threshold.
#' @return A logical scalar indicating whether `a` is fully within `d` of `b`.
#' @export
#' @examples
#' st_3ddfullywithin(g3_point(0, 0, 1), g3_point(0, 0, 0), 1)
st_3ddfullywithin <- function(a, b, d) .Call(R_dfully_within, a, b, d)
#' Test whether geometries intersect
#'
#' @param a,b `threesf` geometries.
#' @param eps Numeric intersection tolerance.
#' @return A logical scalar.
#' @export
#' @examples
#' st_3dintersects(g3_point(0, 0, 0), g3_point(0, 0, 0))
st_3dintersects <- function(a, b, eps = 1e-9) .Call(R_intersects, a, b, eps)

# Constructive ---------------------------------------------------------------
#' Tessellate a geometry
#'
#' Converts surface content into a TIN Z geometry.
#'
#' @param g A `threesf` surface or solid geometry.
#' @return A `threesf` TIN geometry.
#' @export
#' @examples
#' st_tesselate(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(0, 1, 0))))
st_tesselate <- function(g) .Call(R_tessellate, g)
#' Extrude a polygon into a solid
#'
#' Sweeps the first polygon of `g` along the vector `(dx, dy, dz)`.
#'
#' @param g A `threesf` polygon geometry.
#' @param dx,dy,dz Numeric extrusion distances along the three axes.
#' @return A `threesf` SOLID geometry.
#' @export
#' @examples
#' st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1)
st_extrude <- function(g, dx, dy, dz) .Call(R_extrude, g, c(dx, dy, dz))
#' Make a solid from a closed surface
#'
#' @param g A closed `threesf` surface geometry.
#' @return A `threesf` SOLID geometry.
#' @export
#' @examples
#' st_makesolid(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1))
st_makesolid <- function(g) .Call(R_make_solid, g)
#' Orient a solid outward
#'
#' @param g A `threesf` solid or closed surface geometry.
#' @return A `threesf` geometry with outward-oriented patches.
#' @export
#' @examples
#' st_force_outward(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1))
st_force_outward <- function(g) .Call(R_force_outward, g)
#' Force polygon rings counterclockwise
#'
#' @param g A `threesf` polygon geometry.
#' @return A `threesf` geometry with counterclockwise exterior orientation.
#' @export
#' @examples
#' st_forcepolygonccw(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_forcepolygonccw <- function(g) .Call(R_force_ccw, g)
#' Force polygon rings clockwise
#'
#' @param g A `threesf` polygon geometry.
#' @return A `threesf` geometry with clockwise exterior orientation.
#' @export
#' @examples
#' st_forcepolygoncw(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_forcepolygoncw <- function(g) .Call(R_force_cw, g)

#' Get the threesf native library version
#'
#' @return A character version string.
#' @export
#' @examples
#' threesf_version()
threesf_version <- function() .Call(R_version)

# Regularized 3D solid construction; tol is an absolute distance tolerance.
#' Test for self-intersections
#'
#' @param g A `threesf` geometry.
#' @param tol Absolute distance tolerance.
#' @return A logical scalar.
#' @export
#' @examples
#' st_selfintersects(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))))
st_selfintersects <- function(g, tol = 1e-9) .Call(R_self_intersects, g, as.double(tol))
#' Compute a 3D convex hull
#'
#' @param g A `threesf` geometry containing input points.
#' @param tol Absolute distance tolerance.
#' @return A `threesf` convex hull geometry.
#' @export
#' @examples
#' st_3dconvexhull(g3_multipoint(rbind(c(0, 0, 0), c(1, 0, 0), c(0, 1, 0), c(0, 0, 1))))
st_3dconvexhull <- function(g, tol = 1e-9) .Call(R_convex_hull, g, as.double(tol))
#' Intersect two 3D solids
#'
#' @param a,b `threesf` solid geometries.
#' @param tol Absolute distance tolerance.
#' @return A `threesf` geometry containing the intersection.
#' @export
#' @examples
#' st_3dintersection(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1), st_extrude(g3_polygon(rbind(c(0.5, 0.5, 0), c(1.5, 0.5, 0), c(1.5, 1.5, 0), c(0.5, 1.5, 0))), 0, 0, 1))
st_3dintersection <- function(a, b, tol = 1e-9) .Call(R_intersection, a, b, as.double(tol))
#' Union two 3D solids
#'
#' @param a,b `threesf` solid geometries.
#' @param tol Absolute distance tolerance.
#' @return A `threesf` geometry containing the union.
#' @export
#' @examples
#' st_3dunion(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1), st_extrude(g3_polygon(rbind(c(0.5, 0.5, 0), c(1.5, 0.5, 0), c(1.5, 1.5, 0), c(0.5, 1.5, 0))), 0, 0, 1))
st_3dunion <- function(a, b, tol = 1e-9) .Call(R_union, a, b, as.double(tol))
#' Subtract one 3D solid from another
#'
#' @param a,b `threesf` solid geometries.
#' @param tol Absolute distance tolerance.
#' @return A `threesf` geometry containing `a` minus `b`.
#' @export
#' @examples
#' st_3ddifference(st_extrude(g3_polygon(rbind(c(0, 0, 0), c(1, 0, 0), c(1, 1, 0), c(0, 1, 0))), 0, 0, 1), st_extrude(g3_polygon(rbind(c(0.25, 0.25, 0), c(0.75, 0.25, 0), c(0.75, 0.75, 0), c(0.25, 0.75, 0))), 0, 0, 1))
st_3ddifference <- function(a, b, tol = 1e-9) .Call(R_difference, a, b, as.double(tol))
