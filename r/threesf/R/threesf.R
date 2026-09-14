# threesf R API. A "threesf" object is an external pointer to a native Simple
# Features Z geometry; build one from WKT/WKB, sf, or the g3_* constructors.

# I/O ------------------------------------------------------------------------
st_geomfromtext <- function(wkt) .Call(R_read_wkt, as.character(wkt))
st_geomfromwkb <- function(wkb) {
  if (is.character(wkb)) .Call(R_read_hexwkb, wkb) else .Call(R_read_wkb, as.raw(wkb))
}
st_astext <- function(g) .Call(R_wkt, g, FALSE)
st_asewkt <- function(g) .Call(R_wkt, g, TRUE)
st_asbinary <- function(g) .Call(R_wkb, g, FALSE)
st_asewkb <- function(g) .Call(R_wkb, g, TRUE)
st_ashexewkb <- function(g) .Call(R_hexwkb, g, TRUE)
st_geometrytype <- function(g) .Call(R_type, g)
st_srid <- function(g) .Call(R_srid, g)
st_setsrid <- function(g, srid) .Call(R_set_srid, g, as.integer(srid))
st_isempty <- function(g) .Call(R_is_empty, g)
st_npoints <- function(g) .Call(R_num_vertices, g)

#' @export
print.threesf <- function(x, ...) {
  w <- st_astext(x)
  if (nchar(w) > 100) w <- paste0(substr(w, 1, 97), "...")
  cat("<threesf ", w, ">\n", sep = "")
  invisible(x)
}

# sf interop: any sfg/sfc/sf with Z (2D gets z = 0). WKB avoids text rounding.
g3_from_sf <- function(x) {
  if (inherits(x, "sf")) x <- sf::st_geometry(x)
  if (inherits(x, "sfg")) x <- sf::st_sfc(x)
  out <- lapply(sf::st_as_binary(x), function(b) .Call(R_read_wkb, b))
  if (length(out) == 1L) out[[1L]] else out
}
#' Back to sf (POINT/LINESTRING/POLYGON families and their MULTI* only; sf has no SOLID).
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

g3_point <- function(x, y, z = 0) st_geomfromtext(sprintf("POINT Z (%.17g %.17g %.17g)", x, y, z))
g3_multipoint <- function(pts) st_geomfromtext(paste0("MULTIPOINT Z (", .fmt(pts), ")"))
g3_linestring <- function(pts) st_geomfromtext(paste0("LINESTRING Z (", .fmt(pts), ")"))
g3_multilinestring <- function(lines) st_geomfromtext(paste0("MULTILINESTRING Z (", paste(vapply(lines, function(l) paste0("(", .fmt(l), ")"), ""), collapse = ", "), ")"))
g3_polygon <- function(exterior, holes = list()) st_geomfromtext(paste0("POLYGON Z ", .poly(c(list(exterior), holes))))
#' polys: list of list(exterior, hole, ...) or plain n x 3 matrices
g3_multipolygon <- function(polys) st_geomfromtext(paste0("MULTIPOLYGON Z (", paste(vapply(lapply(polys, .norm_patch), .poly, ""), collapse = ", "), ")"))
#' patches: list; each an n x 3 ring or list(exterior, hole, ...)
g3_surface <- function(patches) st_geomfromtext(paste0("POLYHEDRALSURFACE Z ", .surface(patches)))
g3_tin <- function(triangles) st_geomfromtext(paste0("TIN Z ", .surface(triangles)))
#' SOLID Z (SFCGAL): outer shell patches plus optional list of cavity shells
g3_solid <- function(outer, inner = list()) st_geomfromtext(paste0("SOLID Z (", paste(vapply(c(list(outer), inner), .surface, ""), collapse = ", "), ")"))
g3_collection <- function(geoms) st_geomfromtext(paste0("GEOMETRYCOLLECTION Z (", paste(vapply(geoms, st_astext, ""), collapse = ", "), ")"))

# Measures -------------------------------------------------------------------
st_3dlength <- function(g) .Call(R_length, g)
st_3dperimeter <- function(g) .Call(R_perimeter, g)
st_3darea <- function(g) .Call(R_area, g)
st_3darea_tessellated <- function(g) .Call(R_area_tess, g)
st_volume <- function(g) .Call(R_volume, g)
st_signed_volume <- function(g) .Call(R_signed_volume, g)
st_3dcentroid <- function(g) .Call(R_centroid, g)
#' c(xmin, ymin, zmin, xmax, ymax, zmax)
st_3dextent <- function(g) .Call(R_bbox, g)

# Predicates -----------------------------------------------------------------
st_isplanar <- function(g, tol = 1e-9) .Call(R_is_planar, g, tol)
st_isclosed <- function(g) .Call(R_is_closed, g)
st_issolid <- function(g) .Call(R_is_solid, g)
st_orientation <- function(g) .Call(R_orientation, g)

# Binary ---------------------------------------------------------------------
st_3ddistance <- function(a, b) .Call(R_distance, a, b)
st_3dmaxdistance <- function(a, b) .Call(R_max_distance, a, b)
#' 2 x 3 matrix: row 1 on a, row 2 on b
st_3dshortestline <- function(a, b) matrix(.Call(R_closest_points, a, b), 2L, 3L, byrow = TRUE)
st_3dclosestpoint <- function(a, b) st_3dshortestline(a, b)[1L, ]
st_3dlongestline <- function(a, b) matrix(.Call(R_farthest_points, a, b), 2L, 3L, byrow = TRUE)
st_3ddwithin <- function(a, b, d) .Call(R_dwithin, a, b, d)
st_3ddfullywithin <- function(a, b, d) .Call(R_dfully_within, a, b, d)
st_3dintersects <- function(a, b, eps = 1e-9) .Call(R_intersects, a, b, eps)

# Constructive ---------------------------------------------------------------
#' TIN Z of all surface content
st_tesselate <- function(g) .Call(R_tessellate, g)
#' SOLID Z swept from the first polygon of g along c(dx, dy, dz)
st_extrude <- function(g, dx, dy, dz) .Call(R_extrude, g, c(dx, dy, dz))
st_makesolid <- function(g) .Call(R_make_solid, g)
st_force_outward <- function(g) .Call(R_force_outward, g)
st_forcepolygonccw <- function(g) .Call(R_force_ccw, g)
st_forcepolygoncw <- function(g) .Call(R_force_cw, g)

threesf_version <- function() .Call(R_version)


# Regularized 3D solid construction; tol is an absolute distance tolerance.
st_selfintersects <- function(g, tol = 1e-9) .Call(R_self_intersects, g, as.double(tol))
st_3dconvexhull <- function(g, tol = 1e-9) .Call(R_convex_hull, g, as.double(tol))
st_3dintersection <- function(a, b, tol = 1e-9) .Call(R_intersection, a, b, as.double(tol))
st_3dunion <- function(a, b, tol = 1e-9) .Call(R_union, a, b, as.double(tol))
st_3ddifference <- function(a, b, tol = 1e-9) .Call(R_difference, a, b, as.double(tol))
