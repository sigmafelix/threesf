// .Call entry points for the threesf R package (external-pointer handles).
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <cstring>
#include <climits>
#include "threesf/threesf_c.h"

namespace {
void finalize(SEXP xp) { auto* h = static_cast<threesf_handle*>(R_ExternalPtrAddr(xp)); if (h) { threesf_h_free(h); R_ClearExternalPtr(xp); } }
void report_warning() {
    const char* message = threesf_last_warning();
    if (*message) Rf_warning("threesf: %s", message);
}
SEXP wrap(threesf_handle* h) {
    if (!h) { report_warning(); Rf_error("threesf: %s", threesf_last_error()); }
    SEXP xp = PROTECT(R_MakeExternalPtr(h, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xp, finalize, TRUE);
    report_warning();
    Rf_setAttrib(xp, R_ClassSymbol, Rf_mkString("threesf"));
    UNPROTECT(1);
    return xp;
}
threesf_handle* get(SEXP xp) {
    if (TYPEOF(xp) != EXTPTRSXP) Rf_error("not a threesf geometry");
    auto* h = static_cast<threesf_handle*>(R_ExternalPtrAddr(xp));
    if (!h) Rf_error("threesf geometry has been freed");
    return h;
}
SEXP str_result(char* s) { if (!s) Rf_error("threesf: allocation failed"); SEXP r = PROTECT(Rf_mkString(s)); threesf_free(s); UNPROTECT(1); return r; }
SEXP vec_result(int rc, const double* v, int n) {
    report_warning();
    if (rc != 0) Rf_error("threesf: %s", threesf_last_error());
    SEXP r = PROTECT(Rf_allocVector(REALSXP, n)); for (int i = 0; i < n; ++i) REAL(r)[i] = v[i]; UNPROTECT(1); return r;
}
SEXP num_result(double v) { report_warning(); if (v != v) Rf_error("threesf: %s", threesf_last_error()); return Rf_ScalarReal(v); }
SEXP pred_result(int v) { report_warning(); if (v < 0) Rf_error("threesf: %s", threesf_last_error()); return Rf_ScalarLogical(v == 1); }
}  // namespace

extern "C" {
SEXP R_read_wkt(SEXP s) { return wrap(threesf_read_wkt(CHAR(STRING_ELT(s, 0)))); }
SEXP R_read_hexwkb(SEXP s) { return wrap(threesf_read_hexwkb(CHAR(STRING_ELT(s, 0)))); }
SEXP R_read_wkb(SEXP raw) { return wrap(threesf_read_wkb(RAW(raw), Rf_xlength(raw))); }
SEXP R_wkt(SEXP g, SEXP ewkt) { return str_result(threesf_h_wkt(get(g), Rf_asLogical(ewkt))); }
SEXP R_hexwkb(SEXP g, SEXP ewkb) { return str_result(threesf_h_hexwkb(get(g), Rf_asLogical(ewkb))); }
SEXP R_wkb(SEXP g, SEXP ewkb) {
    size_t n = 0; unsigned char* b = threesf_h_wkb(get(g), Rf_asLogical(ewkb), &n);
    if (!b) Rf_error("threesf: allocation failed");
    SEXP r = PROTECT(Rf_allocVector(RAWSXP, static_cast<R_xlen_t>(n))); std::memcpy(RAW(r), b, n); threesf_free(b); UNPROTECT(1); return r;
}
SEXP R_type(SEXP g) { return Rf_mkString(threesf_h_type(get(g))); }
SEXP R_srid(SEXP g) { return Rf_ScalarInteger(threesf_h_srid(get(g))); }
SEXP R_set_srid(SEXP g, SEXP s) { threesf_h_set_srid(get(g), Rf_asInteger(s)); return g; }
SEXP R_is_empty(SEXP g) { return Rf_ScalarLogical(threesf_h_is_empty(get(g)) == 1); }
SEXP R_num_vertices(SEXP g) { return Rf_ScalarInteger(static_cast<int>(threesf_h_num_vertices(get(g)))); }

#define UN(name, fn) SEXP name(SEXP g) { return num_result(fn(get(g))); }
UN(R_length, threesf_h_length) UN(R_perimeter, threesf_h_perimeter) UN(R_area, threesf_h_area)
UN(R_area_tess, threesf_h_area_tessellated) UN(R_volume, threesf_h_volume) UN(R_signed_volume, threesf_h_signed_volume)
#define UP(name, fn) SEXP name(SEXP g) { return pred_result(fn(get(g))); }
UP(R_is_closed, threesf_h_is_closed) UP(R_is_solid, threesf_h_is_solid)
SEXP R_is_planar(SEXP g, SEXP tol) { return pred_result(threesf_h_is_planar(get(g), Rf_asReal(tol))); }
SEXP R_orientation(SEXP g) { return Rf_ScalarInteger(threesf_h_orientation(get(g))); }
SEXP R_centroid(SEXP g) { double v[3]; return vec_result(threesf_h_centroid(get(g), v), v, 3); }
SEXP R_bbox(SEXP g) { double v[6]; return vec_result(threesf_h_bbox(get(g), v), v, 6); }
#define BN(name, fn) SEXP name(SEXP a, SEXP b) { return num_result(fn(get(a), get(b))); }
BN(R_distance, threesf_h_distance) BN(R_max_distance, threesf_h_max_distance)
#define BP(name, fn) SEXP name(SEXP a, SEXP b, SEXP d) { return pred_result(fn(get(a), get(b), Rf_asReal(d))); }
BP(R_dwithin, threesf_h_dwithin) BP(R_dfully_within, threesf_h_dfully_within) BP(R_intersects, threesf_h_intersects)
#define BPAIR(name, fn) SEXP name(SEXP a, SEXP b) { double v[6]; return vec_result(fn(get(a), get(b), v), v, 6); }
BPAIR(R_closest_points, threesf_h_closest_points) BPAIR(R_farthest_points, threesf_h_farthest_points)
#define UH(name, fn) SEXP name(SEXP g) { return wrap(fn(get(g))); }
UH(R_tessellate, threesf_h_tessellate) UH(R_make_solid, threesf_h_make_solid) UH(R_force_outward, threesf_h_force_outward)
UH(R_force_ccw, threesf_h_force_ccw) UH(R_force_cw, threesf_h_force_cw)
SEXP R_extrude(SEXP g, SEXP d) { SEXP dd = PROTECT(Rf_coerceVector(d, REALSXP)); SEXP r = wrap(threesf_h_extrude(get(g), REAL(dd)[0], REAL(dd)[1], REAL(dd)[2])); UNPROTECT(1); return r; }
SEXP R_self_intersects(SEXP g, SEXP tol) { return pred_result(threesf_h_self_intersects(get(g), Rf_asReal(tol))); }
SEXP R_convex_hull(SEXP g, SEXP tol) { return wrap(threesf_h_convex_hull(get(g), Rf_asReal(tol))); }
SEXP R_intersection(SEXP a, SEXP b, SEXP tol) { return wrap(threesf_h_intersection(get(a), get(b), Rf_asReal(tol))); }
SEXP R_union(SEXP a, SEXP b, SEXP tol) { return wrap(threesf_h_union(get(a), get(b), Rf_asReal(tol))); }
SEXP R_difference(SEXP a, SEXP b, SEXP tol) { return wrap(threesf_h_difference(get(a), get(b), Rf_asReal(tol))); }
SEXP R_plot_data(SEXP g, SEXP edges) {
    double* data = nullptr; size_t counts[3] = {0, 0, 0};
    int rc = threesf_h_plot_data(get(g), Rf_asLogical(edges), &data, counts);
    // Release the native buffer before reporting warnings (which R can turn
    // into errors). The external pointer also owns it during R allocations.
    SEXP owner = PROTECT(R_MakeExternalPtr(data, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(owner, [](SEXP p) { threesf_free(R_ExternalPtrAddr(p)); }, TRUE);
    if (rc != 0) { UNPROTECT(1); report_warning(); Rf_error("threesf: %s", threesf_last_error()); }
    SEXP out = PROTECT(Rf_allocVector(VECSXP, 3));
    size_t offset = 0;
    for (int k = 0; k < 3; ++k) {
        int width = 3 * (k + 1);
        if (counts[k] > INT_MAX) Rf_error("plot: too many primitives");
        SEXP m = PROTECT(Rf_allocMatrix(REALSXP, static_cast<int>(counts[k]), width));
        for (size_t i = 0; i < counts[k]; ++i)
            for (int j = 0; j < width; ++j) REAL(m)[i + counts[k] * j] = data[offset + i * width + j];
        offset += counts[k] * width;
        SET_VECTOR_ELT(out, k, m); UNPROTECT(1);
    }
    threesf_free(data); R_ClearExternalPtr(owner);
    SEXP names = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(names, 0, Rf_mkChar("points")); SET_STRING_ELT(names, 1, Rf_mkChar("segments"));
    SET_STRING_ELT(names, 2, Rf_mkChar("triangles")); Rf_setAttrib(out, R_NamesSymbol, names);
    report_warning();
    UNPROTECT(3); return out;
}
SEXP R_version(void) { return Rf_mkString(threesf_version()); }

static const R_CallMethodDef CallEntries[] = {
    {"R_read_wkt", (DL_FUNC)&R_read_wkt, 1}, {"R_read_hexwkb", (DL_FUNC)&R_read_hexwkb, 1}, {"R_read_wkb", (DL_FUNC)&R_read_wkb, 1},
    {"R_wkt", (DL_FUNC)&R_wkt, 2}, {"R_hexwkb", (DL_FUNC)&R_hexwkb, 2}, {"R_wkb", (DL_FUNC)&R_wkb, 2},
    {"R_type", (DL_FUNC)&R_type, 1}, {"R_srid", (DL_FUNC)&R_srid, 1}, {"R_set_srid", (DL_FUNC)&R_set_srid, 2},
    {"R_is_empty", (DL_FUNC)&R_is_empty, 1}, {"R_num_vertices", (DL_FUNC)&R_num_vertices, 1},
    {"R_length", (DL_FUNC)&R_length, 1}, {"R_perimeter", (DL_FUNC)&R_perimeter, 1}, {"R_area", (DL_FUNC)&R_area, 1},
    {"R_area_tess", (DL_FUNC)&R_area_tess, 1}, {"R_volume", (DL_FUNC)&R_volume, 1}, {"R_signed_volume", (DL_FUNC)&R_signed_volume, 1},
    {"R_is_closed", (DL_FUNC)&R_is_closed, 1}, {"R_is_solid", (DL_FUNC)&R_is_solid, 1}, {"R_is_planar", (DL_FUNC)&R_is_planar, 2},
    {"R_orientation", (DL_FUNC)&R_orientation, 1}, {"R_centroid", (DL_FUNC)&R_centroid, 1}, {"R_bbox", (DL_FUNC)&R_bbox, 1},
    {"R_distance", (DL_FUNC)&R_distance, 2}, {"R_max_distance", (DL_FUNC)&R_max_distance, 2},
    {"R_dwithin", (DL_FUNC)&R_dwithin, 3}, {"R_dfully_within", (DL_FUNC)&R_dfully_within, 3}, {"R_intersects", (DL_FUNC)&R_intersects, 3},
    {"R_closest_points", (DL_FUNC)&R_closest_points, 2}, {"R_farthest_points", (DL_FUNC)&R_farthest_points, 2},
    {"R_tessellate", (DL_FUNC)&R_tessellate, 1}, {"R_make_solid", (DL_FUNC)&R_make_solid, 1}, {"R_force_outward", (DL_FUNC)&R_force_outward, 1},
    {"R_force_ccw", (DL_FUNC)&R_force_ccw, 1}, {"R_force_cw", (DL_FUNC)&R_force_cw, 1}, {"R_extrude", (DL_FUNC)&R_extrude, 2},
    {"R_self_intersects", (DL_FUNC)&R_self_intersects, 2}, {"R_convex_hull", (DL_FUNC)&R_convex_hull, 2},
    {"R_intersection", (DL_FUNC)&R_intersection, 3}, {"R_union", (DL_FUNC)&R_union, 3}, {"R_difference", (DL_FUNC)&R_difference, 3},
    {"R_plot_data", (DL_FUNC)&R_plot_data, 2},
    {"R_version", (DL_FUNC)&R_version, 0}, {NULL, NULL, 0}
};
void R_init_threesf(DllInfo* dll) { R_registerRoutines(dll, NULL, CallEntries, NULL, NULL); R_useDynamicSymbols(dll, FALSE); }
}  // extern "C"
