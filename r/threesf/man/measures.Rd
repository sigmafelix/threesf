\name{measures}
\alias{st_3darea}
\alias{st_3darea_tessellated}
\alias{st_3dcentroid}
\alias{st_3dextent}
\alias{st_3dlength}
\alias{st_3dperimeter}
\alias{st_signed_volume}
\alias{st_volume}
\title{3D measures (PostGIS ST_3D* / ST_Volume equivalents)}
\usage{
st_3dlength(g)
st_3dperimeter(g)
st_3darea(g)
st_3darea_tessellated(g)
st_volume(g)
st_signed_volume(g)
st_3dcentroid(g)
st_3dextent(g)
}
\arguments{
\item{g}{a \code{threesf} geometry.}
}
\description{3D measures (PostGIS ST_3D* / ST_Volume equivalents).}
\details{\code{st_3darea} uses Newell's method (exact for planar polygons, holes subtracted); \code{st_3darea_tessellated} triangulates first and is preferred for non-planar patches. \code{st_volume} uses the divergence theorem over the tessellated boundary; inner shells are subtracted. \code{st_3dextent} returns \code{c(xmin, ymin, zmin, xmax, ymax, zmax)}.}
