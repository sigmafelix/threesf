\name{boolean}
\alias{st_3dconvexhull}
\alias{st_3dintersection}
\alias{st_3dunion}
\alias{st_3ddifference}
\alias{st_selfintersects}
\title{3D convex hull, solid Boolean operations and self-intersection detection}
\usage{
st_3dconvexhull(g, tol = 1e-9)
st_3dintersection(a, b, tol = 1e-9)
st_3dunion(a, b, tol = 1e-9)
st_3ddifference(a, b, tol = 1e-9)
st_selfintersects(g, tol = 1e-9)
}
\arguments{
\item{g}{A \code{threesf} geometry.}
\item{a, b}{Solid geometries or closed, consistently oriented polyhedral surfaces. Operands must have the same SRID.}
\item{tol}{Finite positive absolute distance tolerance in coordinate units.}
}
\description{Construct convex hulls and regularized intersections, unions and differences of 3D solids. Detect self-intersections in rings and mesh faces.}
\details{
The hull uses all input vertices, returning a solid or a lower-dimensional polygon, line, point or empty geometry.
Boolean operations preserve cavities and disconnected components, returning \code{SOLID}, \code{MULTISOLID}, or an empty geometry collection.
They are regularized volume operations: intersections consisting only of faces, edges or points are empty.
Open surfaces and nonempty point, line or polygon operands are unsupported.
Self-intersecting input produces a warning. Boolean operations, tessellation and extrusion refuse unsafe input;
a hull can still be constructed from its vertices. Self-intersecting or non-manifold Boolean output is refused.
The algorithms use floating-point predicates, local coordinate normalization and vertex welding; this is not an exact-arithmetic kernel.
}
\value{A new \code{threesf} geometry, or a logical value for \code{st_selfintersects}.}
\examples{
p <- st_geomfromtext("POLYGON Z ((0 0 0,2 0 0,2 2 0,0 2 0,0 0 0))")
a <- st_extrude(p, 0, 0, 2)
st_volume(st_3dconvexhull(a))
st_isempty(st_3ddifference(a, a))
}
