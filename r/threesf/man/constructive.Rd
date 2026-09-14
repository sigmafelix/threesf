\name{constructive}
\alias{st_extrude}
\alias{st_force_outward}
\alias{st_forcepolygonccw}
\alias{st_forcepolygoncw}
\alias{st_makesolid}
\alias{st_tesselate}
\title{Tessellation, extrusion and orientation repair}
\usage{
st_tesselate(g)
st_extrude(g, dx, dy, dz)
st_makesolid(g)
st_force_outward(g)
st_forcepolygonccw(g)
st_forcepolygoncw(g)
}
\arguments{
\item{g}{a \code{threesf} geometry.}
\item{dx, dy, dz}{extrusion vector.}
}
\description{Tessellation, extrusion and orientation repair.}
\details{\code{st_tesselate} returns a \code{TIN Z}; \code{st_extrude} sweeps the first polygon into a \code{SOLID Z} with outward normals; \code{st_makesolid} wraps a closed surface as a solid; \code{st_force_outward} flips patches so the signed volume is positive.}
