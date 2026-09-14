\name{plot.threesf}
\alias{plot.threesf}
\alias{g3_plot}
\title{Static and interactive 3D geometry plots}
\usage{
\method{plot}{threesf}(x, y = NULL, ..., interactive = FALSE)
g3_plot(g, interactive = FALSE, col = "#4477aa", alpha = .7,
        edges = TRUE, lwd = 1, point_size = 5, main = NULL,
        theta = 40, phi = 25)
}
\arguments{
\item{x, g}{A \code{threesf} geometry. \code{g3_plot} also accepts a list of geometries.}
\item{y}{Unused; must be \code{NULL}.}
\item{...}{Arguments forwarded to \code{g3_plot}.}
\item{interactive}{Use Plotly for an interactive 3D widget instead of base R graphics. Requires the optional \pkg{plotly} package.}
\item{col}{One R color or one color per geometry. Static faces are shaded using this color.}
\item{alpha}{Face opacity between zero and one. Curves and points stay opaque.}
\item{edges}{Draw polygon and mesh edges. Curves are always drawn.}
\item{lwd}{Positive line width, in the backend's units.}
\item{point_size}{Positive marker size. Static plots use \code{cex = point_size / 5}; interactive plots use Plotly marker-size units.}
\item{main}{Optional title.}
\item{theta, phi}{Finite viewing angles in degrees for static plots, as in \code{graphics::persp}. Interactive plots use Plotly's camera controls.}
}
\description{Visualize points, curves, polygons with holes, TINs, surfaces, solids (including cavities), and nested geometry collections in XYZ coordinates.}
\details{
Static plots use base R \code{persp} and \code{trans3d}, with depth-sorted shaded triangles, segments and points. No additional package or OpenGL display is needed; use a PNG or PDF graphics device to export.
Interactive plots return a Plotly widget with explicit mesh triangles and separate point/line traces. Print the widget to view it, or save it using \code{htmlwidgets::saveWidget}. Plotly is only loaded when requested.

Coordinates remain in their original units, with equal spatial scaling and padded bounds. No CRS transformation is performed. An empty geometry or list produces an empty scene. Multiple geometries can be assigned separate colors.
Tessellation comes from the geometry core, preserving holes and all solid shells. Invalid geometry warnings/errors are propagated; plotting does not repair self-intersections.

Static depth sorting and transparent surfaces are approximate; overlapping faces and lines can have rendering artifacts. Use the interactive view to inspect from different angles. Edges include all stored patch boundaries, including triangulation edges in a TIN or Boolean result.
}
\value{Static mode draws on the current graphics device and invisibly returns a 4 by 4 projection matrix. Interactive mode returns a \code{plotly} htmlwidget for customization, printing or export.}
\examples{
p <- g3_polygon(rbind(c(0,0),c(2,0),c(2,2),c(0,2)))
s <- st_extrude(p, 0, 0, 3)
plot(s, main = "Extruded polygon")
g3_plot(list(s, g3_point(3,3,4)), col = c("steelblue", "tomato"))
if (requireNamespace("plotly", quietly = TRUE)) {
  widget <- g3_plot(s, interactive = TRUE)
}
}
