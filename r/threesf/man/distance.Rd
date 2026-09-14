\name{distance}
\alias{st_3dclosestpoint}
\alias{st_3ddfullywithin}
\alias{st_3ddistance}
\alias{st_3ddwithin}
\alias{st_3dintersects}
\alias{st_3dlongestline}
\alias{st_3dmaxdistance}
\alias{st_3dshortestline}
\title{3D distance family}
\usage{
st_3ddistance(a, b)
st_3dmaxdistance(a, b)
st_3dshortestline(a, b)
st_3dclosestpoint(a, b)
st_3dlongestline(a, b)
st_3ddwithin(a, b, d)
st_3ddfullywithin(a, b, d)
st_3dintersects(a, b, eps = 1e-9)
}
\arguments{
\item{a, b}{\code{threesf} geometries.}
\item{d}{distance threshold.}
\item{eps}{tolerance for intersection.}
}
\description{3D distance family.}
\details{Distances are computed between point/segment/triangle primitives (closed-form segment-segment and point-triangle routines); a point strictly inside a solid has distance 0. \code{st_3dshortestline} and \code{st_3dlongestline} return a 2 x 3 matrix (row 1 on \code{a}, row 2 on \code{b}).}
