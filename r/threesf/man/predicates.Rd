\name{predicates}
\alias{st_isclosed}
\alias{st_isplanar}
\alias{st_issolid}
\alias{st_orientation}
\title{Topological predicates for surfaces}
\usage{
st_isplanar(g, tol = 1e-9)
st_isclosed(g)
st_issolid(g)
st_orientation(g)
}
\arguments{
\item{g}{a \code{threesf} geometry.}
\item{tol}{maximum vertex distance from the best-fit plane.}
}
\description{Topological predicates for surfaces.}
\details{\code{st_isclosed} requires every edge to be shared by exactly two patches; \code{st_issolid} additionally requires consistent orientation. \code{st_orientation} returns 1 (CCW), -1 (CW) or 0 for the first polygon.}
