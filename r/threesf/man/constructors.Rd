\name{constructors}
\alias{g3_collection}
\alias{g3_linestring}
\alias{g3_multilinestring}
\alias{g3_multipoint}
\alias{g3_multipolygon}
\alias{g3_point}
\alias{g3_polygon}
\alias{g3_solid}
\alias{g3_surface}
\alias{g3_tin}
\title{Build geometries from coordinate matrices}
\usage{
g3_point(x, y, z = 0)
g3_multipoint(pts)
g3_linestring(pts)
g3_multilinestring(lines)
g3_polygon(exterior, holes = list())
g3_multipolygon(polys)
g3_surface(patches)
g3_tin(triangles)
g3_solid(outer, inner = list())
g3_collection(geoms)
}
\arguments{
\item{x, y, z}{coordinates.}
\item{pts, exterior}{an n x 3 (or n x 2) numeric matrix; rings are closed automatically.}
\item{holes}{list of ring matrices.}
\item{lines, polys, patches, triangles}{lists of matrices, or for polygons lists of \code{list(exterior, hole, ...)}.}
\item{outer, inner}{outer shell patches and a list of cavity shells.}
\item{geoms}{list of \code{threesf} geometries.}
}
\description{Build geometries from coordinate matrices.}
\details{Coordinates are formatted with 17 significant digits, so doubles round-trip exactly.}
