cube_wkt <- paste0(
  "SOLID Z ((((0 0 0, 0 1 0, 1 1 0, 1 0 0, 0 0 0)), ((0 0 1, 1 0 1, 1 1 1, 0 1 1, 0 0 1)), ",
  "((0 0 0, 1 0 0, 1 0 1, 0 0 1, 0 0 0)), ((0 1 0, 0 1 1, 1 1 1, 1 1 0, 0 1 0)), ",
  "((0 0 0, 0 0 1, 0 1 1, 0 1 0, 0 0 0)), ((1 0 0, 1 1 0, 1 1 1, 1 0 1, 1 0 0))))")

test_that("measures from WKT match analytic values", {
  cube <- st_geomfromtext(cube_wkt)
  expect_equal(st_geometrytype(cube), "SOLID")
  expect_equal(st_volume(cube), 1)
  expect_equal(st_3darea(cube), 6)
  expect_true(st_issolid(cube))
  expect_equal(st_3dcentroid(cube), c(0.5, 0.5, 0.5))
  expect_equal(st_3dlength(st_geomfromtext("LINESTRING Z (0 0 0, 3 4 0, 3 4 12)")), 17)
})

test_that("constructors, holes and extrusion", {
  holed <- g3_polygon(rbind(c(0, 2, 0), c(10, 2, 0), c(10, 2, 10), c(0, 2, 10)),
                      holes = list(rbind(c(4, 2, 4), c(4, 2, 6), c(6, 2, 6), c(6, 2, 4))))
  expect_equal(st_3darea(holed), 96)
  expect_equal(st_3dperimeter(holed), 48)
  prism <- st_extrude(holed, 0, 3, 0)
  expect_equal(st_geometrytype(prism), "SOLID")
  expect_equal(st_volume(prism), 288)
  expect_true(st_isclosed(prism))
  expect_equal(st_geometrytype(st_tesselate(prism)), "TIN")
})

test_that("distance family", {
  cube <- st_geomfromtext(cube_wkt); p <- g3_point(0.5, 0.5, 3)
  expect_equal(st_3ddistance(p, cube), 2)
  expect_equal(st_3dshortestline(p, cube)[2, ], c(0.5, 0.5, 1))
  expect_equal(st_3ddistance(g3_point(0.5, 0.5, 0.5), cube), 0)
  expect_true(st_3ddwithin(p, cube, 2)); expect_false(st_3ddwithin(p, cube, 1.99))
  expect_equal(st_3dmaxdistance(p, cube), sqrt(9.5))
})

test_that("WKT/WKB round-trips and PostGIS fixtures", {
  cases <- c("POINT Z (1 2 3)" = "POINT Z (1 2 3)", "POINTZ(1 2 3)" = "POINT Z (1 2 3)",
             "POINT (1 2)" = "POINT Z (1 2 0)", "POINT ZM (1 2 3 9)" = "POINT Z (1 2 3)",
             "POINT Z EMPTY" = "POINT Z EMPTY",
             "TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)))" = "TIN Z (((0 0 0, 1 0 0, 0 1 0, 0 0 0)))",
             "GEOMETRYCOLLECTION Z (POINT Z (1 2 3), GEOMETRYCOLLECTION Z (LINESTRING Z (0 0 0, 1 1 1)))" =
               "GEOMETRYCOLLECTION Z (POINT Z (1 2 3), GEOMETRYCOLLECTION Z (LINESTRING Z (0 0 0, 1 1 1)))")
  for (w in names(cases)) {
    g <- st_geomfromtext(w)
    expect_equal(st_astext(g), unname(cases[[w]]))
    expect_equal(st_astext(st_geomfromwkb(st_asbinary(g))), unname(cases[[w]]))
    g <- st_setsrid(g, 5179L)
    back <- st_geomfromwkb(st_asewkb(g))
    expect_equal(st_srid(back), 5179L)
  }
  pg <- st_geomfromwkb("01010000A0E6100000000000000000F03F00000000000000400000000000000840")
  expect_equal(st_asewkt(pg), "SRID=4326;POINT Z (1 2 3)")
})

test_that("errors are R errors", {
  expect_error(st_geomfromtext("POLYGON Z ((0 0 0, 1 1 1)"), "expected")
  expect_error(st_geomfromtext("NOPE (1 2 3)"), "unknown geometry type")
})

test_that("sf interop", {
  skip_if_not_installed("sf")
  sq <- sf::st_polygon(list(rbind(c(0, 0, 0), c(4, 0, 0), c(4, 3, 0), c(0, 3, 0), c(0, 0, 0))), dim = "XYZ")
  g <- g3_from_sf(sq)
  expect_equal(st_3darea(g), 12)
  back <- g3_to_sf(g)
  expect_true(sf::st_equals(back, sf::st_sfc(sq), sparse = FALSE)[1, 1])
})
