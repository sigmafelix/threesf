# Shared native primitives keep holes, mixed collections and cavity shells in 3D.
.plot_data <- function(g, edges = TRUE) .Call(R_plot_data, g, edges)

.plot_limits <- function(parts) {
  vertices <- lapply(parts, function(p) do.call(rbind, lapply(p, function(m) matrix(t(m), ncol = 3, byrow = TRUE))))
  vertices <- do.call(rbind, vertices)
  if (is.null(vertices) || !nrow(vertices)) return(matrix(rep(c(-.5, .5), 3), ncol = 2, byrow = TRUE))
  bounds <- t(apply(vertices, 2, range))
  span <- bounds[, 2] - bounds[, 1]
  padding <- pmax(span, max(span, 1e-12) * .1) * .05
  if (!any(span > 0)) padding[] <- .5
  cbind(bounds[, 1] - padding, bounds[, 2] + padding)
}

#' Static or interactive 3D plotting (see man/plot.Rd).
g3_plot <- function(g, interactive = FALSE, col = "#4477aa", alpha = .7,
                    edges = TRUE, lwd = 1, point_size = 5, main = NULL,
                    theta = 40, phi = 25) {
  if (!is.logical(interactive) || length(interactive) != 1L || is.na(interactive)) stop("interactive must be TRUE or FALSE")
  if (!is.logical(edges) || length(edges) != 1L || is.na(edges)) stop("edges must be TRUE or FALSE")
  geoms <- if (inherits(g, "threesf")) list(g) else g
  if (!is.list(geoms) || !all(vapply(geoms, inherits, logical(1), "threesf"))) stop("g must be a threesf geometry or list of geometries")
  if (length(col) == 1L) col <- rep(col, length(geoms))
  if (length(col) != length(geoms)) stop("provide one color or one color per geometry")
  if (length(alpha) != 1L || !is.finite(alpha) || alpha < 0 || alpha > 1) stop("alpha must be between 0 and 1")
  if (length(lwd) != 1L || !is.finite(lwd) || lwd <= 0 || length(point_size) != 1L || !is.finite(point_size) || point_size <= 0) stop("lwd and point_size must be finite and positive")
  if (length(theta) != 1L || !is.finite(theta) || length(phi) != 1L || !is.finite(phi)) stop("theta and phi must be finite numbers")
  if (interactive && !requireNamespace("plotly", quietly = TRUE)) stop("Interactive plotting requires plotly; install.packages('plotly').")
  parts <- lapply(geoms, .plot_data, edges = edges)
  limits <- .plot_limits(parts)
  if (interactive) {
    fig <- plotly::plot_ly()
    for (i in seq_along(parts)) {
      p <- parts[[i]]
      rgb <- grDevices::col2rgb(col[i])
      color <- grDevices::rgb(rgb[1], rgb[2], rgb[3], maxColorValue = 255)
      if (nrow(p$triangles)) {
        vertices <- matrix(t(p$triangles), ncol = 3, byrow = TRUE)
        idx <- seq.int(0L, nrow(vertices) - 1L, by = 3L)
        fig <- plotly::add_trace(fig, type = "mesh3d", x = vertices[, 1], y = vertices[, 2], z = vertices[, 3],
          i = idx, j = idx + 1L, k = idx + 2L, color = I(color), opacity = alpha, flatshading = TRUE,
          name = paste("Geometry", i), legendgroup = as.character(i), inherit = FALSE)
      }
      if (nrow(p$segments)) {
        vertices <- matrix(t(cbind(p$segments, NA_real_, NA_real_, NA_real_)), ncol = 3, byrow = TRUE)
        fig <- plotly::add_trace(fig, type = "scatter3d", mode = "lines", x = vertices[, 1], y = vertices[, 2], z = vertices[, 3],
          line = list(color = color, width = lwd), showlegend = FALSE, legendgroup = as.character(i), inherit = FALSE)
      }
      if (nrow(p$points)) fig <- plotly::add_trace(fig, type = "scatter3d", mode = "markers",
        x = p$points[, 1], y = p$points[, 2], z = p$points[, 3], marker = list(color = color, size = point_size),
        name = paste("Geometry", i), legendgroup = as.character(i), inherit = FALSE)
    }
    return(plotly::layout(fig, title = main, scene = list(aspectmode = "data",
      xaxis = list(title = "X", range = limits[1, ]), yaxis = list(title = "Y", range = limits[2, ]),
      zaxis = list(title = "Z", range = limits[3, ]))))
  }
  pmat <- graphics::persp(limits[1, ], limits[2, ], matrix(limits[3, 1], 2, 2),
    xlim = limits[1, ], ylim = limits[2, ], zlim = limits[3, ], theta = theta, phi = phi,
    scale = FALSE, col = NA, border = NA, ticktype = "detailed", xlab = "X", ylab = "Y", zlab = "Z", main = main)
  # Painter's algorithm: draw each primitive from far to near. This is an
  # approximate static projection, not a per-pixel hidden-surface renderer.
  draw <- list()
  for (i in seq_along(parts)) {
    for (kind in names(parts[[i]])) {
      m <- parts[[i]][[kind]]
      for (j in seq_len(nrow(m))) {
        vertices <- matrix(m[j, ], ncol = 3, byrow = TRUE)
        projected <- grDevices::trans3d(vertices[, 1], vertices[, 2], vertices[, 3], pmat)
        depth <- c(colMeans(vertices), 1) %*% pmat
        color <- col[i]
        if (kind == "triangles") {
          a <- vertices[2, ] - vertices[1, ]; b <- vertices[3, ] - vertices[1, ]
          normal <- c(a[2]*b[3]-a[3]*b[2], a[3]*b[1]-a[1]*b[3], a[1]*b[2]-a[2]*b[1])
          size <- sqrt(sum(normal^2))
          shade <- .4 + if (size > 0) .6 * abs(sum(normal * c(1,-2,3)) / (size * sqrt(14))) else 0
          rgb <- grDevices::col2rgb(color) / 255 * shade
          color <- grDevices::rgb(rgb[1], rgb[2], rgb[3])
        }
        draw[[length(draw) + 1L]] <- list(xy = projected, depth = depth[4], kind = kind, col = color)
      }
    }
  }
  for (i in order(vapply(draw, function(p) p$depth, 0), decreasing = TRUE)) {
    item <- draw[[i]]
    if (item$kind == "triangles") graphics::polygon(item$xy, col = grDevices::adjustcolor(item$col, alpha.f = alpha), border = NA)
    else if (item$kind == "segments") graphics::lines(item$xy, col = item$col, lwd = lwd)
    else graphics::points(item$xy, col = item$col, pch = 16, cex = point_size / 5)
  }
  invisible(pmat)
}

#' @export
plot.threesf <- function(x, y = NULL, ..., interactive = FALSE) {
  if (!is.null(y)) stop("y is unused; pass a list to g3_plot() to plot multiple geometries")
  g3_plot(x, interactive = interactive, ...)
}
