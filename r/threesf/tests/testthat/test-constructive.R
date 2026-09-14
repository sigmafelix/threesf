box3 <- function(x=0, y=0, z=0, size=1) {
  p <- g3_polygon(rbind(c(x,y,z),c(x+size,y,z),c(x+size,y+size,z),c(x,y+size,z)))
  st_extrude(p,0,0,size)
}

test_that("hulls retain dimensions and SRID", {
  p <- st_geomfromtext("SRID=5179;MULTIPOINT Z ((0 0 0),(1 0 0),(0 1 0),(0 0 1))")
  h <- st_3dconvexhull(p)
  expect_equal(st_geometrytype(h),"SOLID")
  expect_equal(st_srid(h),5179)
  expect_equal(st_volume(h),1/6)
  expect_false(st_selfintersects(h))
  expect_equal(st_geometrytype(st_3dconvexhull(st_geomfromtext("POINT Z (1 2 3)"))),"POINT")
})

test_that("Booleans handle overlap, cavities and disconnected results", {
  a <- box3(); b <- box3(.5,.5,.5)
  expect_equal(st_volume(st_3dintersection(a,b)),.125)
  expect_equal(st_volume(st_3dunion(a,b)),1.875)
  expect_equal(st_volume(st_3ddifference(a,b)),.875)
  cavity <- st_3ddifference(box3(size=4),box3(1,1,1,2))
  expect_equal(st_volume(cavity),56)
  expect_false(st_selfintersects(cavity))
  expect_true(st_issolid(cavity))
  expect_equal(st_astext(st_geomfromwkb(st_asbinary(cavity))),st_astext(cavity))
  expect_equal(st_geometrytype(st_3dunion(a,box3(2))),"MULTISOLID")
  expect_true(st_isempty(st_3dintersection(a,box3(1))))
  expect_true(st_isempty(st_3ddifference(a,a)))
})

test_that("self intersections warn and unsafe construction fails", {
  expect_warning(bad <- st_geomfromtext("POLYGON Z ((0 0 0,2 2 0,0 2 0,2 0 0,0 0 0))"),"self-intersection")
  expect_warning(expect_true(st_selfintersects(bad)),"self-intersection")
  expect_warning(expect_error(st_tesselate(bad),"self-intersection"),"self-intersection")
  expect_warning(h <- st_3dconvexhull(bad),"self-intersection")
  expect_equal(st_3darea(h),4)
  expect_false(st_selfintersects(h))
  expect_error(st_3dconvexhull(h,0),"tolerance")
  expect_error(st_3dunion(h,box3()),"solids or closed")
})
