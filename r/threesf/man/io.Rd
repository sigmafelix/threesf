\name{io}
\alias{g3_from_sf}
\alias{g3_to_sf}
\alias{st_asbinary}
\alias{st_asewkb}
\alias{st_asewkt}
\alias{st_ashexewkb}
\alias{st_astext}
\alias{st_geometrytype}
\alias{st_geomfromtext}
\alias{st_geomfromwkb}
\alias{st_isempty}
\alias{st_npoints}
\alias{st_setsrid}
\alias{st_srid}
\title{Read and write Simple Features Z geometries}
\usage{
st_geomfromtext(wkt)
st_geomfromwkb(wkb)
st_astext(g)
st_asewkt(g)
st_asbinary(g)
st_asewkb(g)
st_ashexewkb(g)
st_geometrytype(g)
st_srid(g)
st_setsrid(g, srid)
st_isempty(g)
st_npoints(g)
g3_from_sf(x)
g3_to_sf(g, crs = NA)
}
\arguments{
\item{wkt}{WKT or EWKT string (\code{POINT Z (...)}, \code{SRID=n;...}, 2D input gets z = 0).}
\item{wkb}{raw vector (WKB/EWKB, either byte order) or hex string.}
\item{g}{a \code{threesf} geometry or, for \code{g3_to_sf}, a list of them.}
\item{srid}{integer SRID.}
\item{x}{an \code{sf}, \code{sfc} or \code{sfg} object with Z.}
\item{crs}{CRS passed to \code{sf::st_sfc}.}
}
\description{Read and write Simple Features Z geometries.}
\details{Geometries are external pointers to native Simple Features Z objects. All SF types are supported, plus SFCGAL \code{SOLID} and \code{MULTISOLID}. \code{g3_from_sf} converts via WKB; \code{g3_to_sf} works for the point/line/polygon families only (sf has no SOLID type).}
