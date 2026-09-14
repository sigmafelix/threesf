plot_fixture <- function() {
  p <- g3_polygon(rbind(c(0,2,0),c(4,2,0),c(4,2,4),c(0,2,4)),
    holes = list(rbind(c(1,2,1),c(1,2,3),c(3,2,3),c(3,2,1))))
  g3_collection(list(g3_point(5,3,6), g3_collection(list(
    g3_linestring(rbind(c(0,0,0),c(1,2,3))), p))))
}

test_that("plot primitives preserve XYZ, holes, curves and empty inputs", {
  p <- threesf:::.plot_data(plot_fixture(), edges = FALSE)
  expect_equal(unname(p$points), matrix(c(5,3,6), nrow=1))
  expect_equal(unname(p$segments), matrix(c(0,0,0,1,2,3), nrow=1))
  expect_equal(dim(p$triangles), c(8L,9L))
  expect_true(all(p$triangles[,c(2,5,8)] == 2))
  a <- p$triangles[,4:6] - p$triangles[,1:3]
  b <- p$triangles[,7:9] - p$triangles[,1:3]
  cross <- cbind(a[,2]*b[,3]-a[,3]*b[,2],a[,3]*b[,1]-a[,1]*b[,3],a[,1]*b[,2]-a[,2]*b[,1])
  expect_equal(sum(sqrt(rowSums(cross^2)))/2,12)
  expect_equal(nrow(threesf:::.plot_data(plot_fixture())$segments),9L)
  empty <- threesf:::.plot_data(st_geomfromtext("GEOMETRYCOLLECTION EMPTY"))
  expect_equal(vapply(empty,nrow,integer(1)),c(points=0L,segments=0L,triangles=0L))
})

test_that("static plotting works on a file device through S3", {
  file <- tempfile(fileext=".pdf")
  grDevices::pdf(file)
  on.exit({grDevices::dev.off(); unlink(file)}, add=TRUE)
  pmat <- plot(plot_fixture(), edges=FALSE, main="XYZ")
  expect_equal(dim(pmat),c(4L,4L))
  expect_true(all(is.finite(pmat)))
  expect_equal(dim(g3_plot(list())),c(4L,4L))
  expect_equal(dim(g3_plot(list(plot_fixture(),g3_point(9,8,7)),col=c("red","blue"))),c(4L,4L))
})

test_that("interactive plots have explicit face indices and separate curves", {
  skip_if_not_installed("plotly")
  p <- g3_plot(list(plot_fixture(),g3_point(9,8,7)),interactive=TRUE,edges=FALSE,col=c("red","blue"),alpha=.4)
  expect_s3_class(p,"plotly")
  built <- plotly::plotly_build(p)
  meshes <- Filter(function(t) identical(t$type,"mesh3d"),built$x$data)
  expect_length(meshes,1)
  expect_equal(as.numeric(meshes[[1]]$i),seq(0,21,3))
  expect_equal(meshes[[1]]$opacity,.4)
  expect_equal(built$x$layout$scene$aspectmode,"data")
  curves <- Filter(function(t) identical(t$mode,"lines"),built$x$data)
  expect_length(curves,1)
  expect_equal(as.numeric(curves[[1]]$x[1:2]),c(0,1))
  expect_s3_class(plot(g3_point(0,0,0),interactive=TRUE),"plotly")
  expect_s3_class(g3_plot(list(),interactive=TRUE),"plotly")
})

test_that("invalid plot options and unsafe tessellation are reported", {
  expect_error(g3_plot(1),"geometry or list")
  expect_error(g3_plot(g3_point(0,0),alpha=-1),"alpha")
  expect_error(g3_plot(g3_point(0,0),lwd=0),"positive")
  expect_error(g3_plot(g3_point(0,0),col=character()),"color")
  expect_warning(bad <- g3_polygon(rbind(c(0,0),c(2,2),c(0,2),c(2,0))),"self-intersection")
  expect_warning(expect_error(threesf:::.plot_data(bad),"self-intersection"),"self-intersection")
})
