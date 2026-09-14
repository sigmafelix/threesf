/* threesf C ABI — flat-array geometry encoding for FFI (Python ctypes, R .Call, ...)
 *
 * All coordinates are interleaved xyz doubles. Offsets are 0-based and use the
 * "starts" convention: an array of n_parts+1 indices where part i spans
 * [starts[i], starts[i+1]).
 *
 *   type            coords        ring_starts        patch_starts        shell_starts
 *   POINT(S)   0    points        -                  -                   -
 *   LINE(S)    1    vertices      part starts        -                   -
 *   POLYGON(S) 2    vertices      ring starts        polygon->rings      -
 *   SURFACE    3    vertices      ring starts        patch->rings        -
 *   SOLID      4    vertices      ring starts        patch->rings        shell->patches (shell 0 = outer)
 *
 * Numeric results return NaN on invalid input; predicates return -1.
 */
#ifndef THREESF_C_H
#define THREESF_C_H
#include <stddef.h>
#include <stdint.h>

#if defined(THREESF_STATIC)
#  define THREESF_API
#elif defined(_WIN32)
#  if defined(THREESF_BUILDING)
#    define THREESF_API __declspec(dllexport)
#  else
#    define THREESF_API __declspec(dllimport)
#  endif
#else
#  define THREESF_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum threesf_type { THREESF_POINT = 0, THREESF_LINESTRING = 1, THREESF_POLYGON = 2, THREESF_SURFACE = 3, THREESF_SOLID = 4 };

typedef struct threesf_geom {
    int type;
    const double* coords;      size_t n_coords;   /* number of xyz triples */
    const size_t* ring_starts; size_t n_rings;    /* array length n_rings+1 (for LINESTRING: n_parts) */
    const size_t* patch_starts; size_t n_patches; /* array length n_patches+1 */
    const size_t* shell_starts; size_t n_shells;  /* array length n_shells+1 */
} threesf_geom;

THREESF_API const char* threesf_version(void);

/* Measures */
THREESF_API double threesf_length(const threesf_geom* g);
THREESF_API double threesf_perimeter(const threesf_geom* g);
THREESF_API double threesf_area(const threesf_geom* g);
THREESF_API double threesf_area_tessellated(const threesf_geom* g);
THREESF_API double threesf_volume(const threesf_geom* g);          /* solids; SURFACE also accepted if closed */
THREESF_API double threesf_signed_volume(const threesf_geom* g);   /* SURFACE / SOLID outer shell */
THREESF_API int    threesf_centroid(const threesf_geom* g, double out[3]);
THREESF_API int    threesf_bbox(const threesf_geom* g, double out[6]); /* xmin ymin zmin xmax ymax zmax */

/* Predicates / topology */
THREESF_API int threesf_is_planar(const threesf_geom* g, double tol);
THREESF_API int threesf_is_closed(const threesf_geom* g);
THREESF_API int threesf_is_solid(const threesf_geom* g);
THREESF_API int threesf_orientation(const threesf_geom* g);       /* first polygon: +1 CCW, -1 CW, 0 degenerate */

/* Binary */
THREESF_API double threesf_distance(const threesf_geom* a, const threesf_geom* b);
THREESF_API double threesf_max_distance(const threesf_geom* a, const threesf_geom* b);
THREESF_API int    threesf_closest_points(const threesf_geom* a, const threesf_geom* b, double out[6]);
THREESF_API int    threesf_farthest_points(const threesf_geom* a, const threesf_geom* b, double out[6]);
THREESF_API int    threesf_dwithin(const threesf_geom* a, const threesf_geom* b, double d);
THREESF_API int    threesf_dfully_within(const threesf_geom* a, const threesf_geom* b, double d);
THREESF_API int    threesf_intersects(const threesf_geom* a, const threesf_geom* b, double eps);

/* Constructive — outputs are triangle soups: 9 doubles per triangle, freed with threesf_free */
THREESF_API int threesf_tessellate(const threesf_geom* g, double** tris, size_t* n_tris);
THREESF_API int threesf_extrude(const threesf_geom* polygon, double dx, double dy, double dz, double** tris, size_t* n_tris);
THREESF_API int threesf_force_outward(const threesf_geom* surface, double** tris, size_t* n_tris);
THREESF_API void threesf_free(void* p);

/* ------------------------------------------------------------------------
 * Handle API — opaque geometries created from WKT / WKB (any Simple Features
 * Z type, EWKT/EWKB, SFCGAL SOLID). Prefer this from language bindings.
 * Functions return NaN / -1 / NULL on error; threesf_last_error() explains.
 * ------------------------------------------------------------------------ */
typedef struct threesf_handle threesf_handle;

THREESF_API const char*   threesf_last_error(void);
/* Warning from the last operation on this thread; empty when none. */
THREESF_API const char*   threesf_last_warning(void);
THREESF_API threesf_handle* threesf_read_wkt(const char* wkt);
THREESF_API threesf_handle* threesf_read_wkb(const unsigned char* wkb, size_t len);
THREESF_API threesf_handle* threesf_read_hexwkb(const char* hex);
THREESF_API threesf_handle* threesf_from_flat(const threesf_geom* g);
THREESF_API void          threesf_h_free(threesf_handle* h);
THREESF_API threesf_handle* threesf_h_clone(const threesf_handle* h);

THREESF_API char*          threesf_h_wkt(const threesf_handle* h, int ewkt);                        /* free with threesf_free */
THREESF_API unsigned char* threesf_h_wkb(const threesf_handle* h, int ewkb, size_t* len);           /* free with threesf_free */
THREESF_API char*          threesf_h_hexwkb(const threesf_handle* h, int ewkb);                     /* free with threesf_free */
THREESF_API const char*    threesf_h_type(const threesf_handle* h);                                 /* "POLYGON", "TIN", ... */
THREESF_API int            threesf_h_srid(const threesf_handle* h);
THREESF_API void           threesf_h_set_srid(threesf_handle* h, int srid);
THREESF_API int            threesf_h_is_empty(const threesf_handle* h);
THREESF_API size_t         threesf_h_num_vertices(const threesf_handle* h);

THREESF_API double threesf_h_length(const threesf_handle* g);
THREESF_API double threesf_h_perimeter(const threesf_handle* g);
THREESF_API double threesf_h_area(const threesf_handle* g);
THREESF_API double threesf_h_area_tessellated(const threesf_handle* g);
THREESF_API double threesf_h_volume(const threesf_handle* g);
THREESF_API double threesf_h_signed_volume(const threesf_handle* g);
THREESF_API int    threesf_h_centroid(const threesf_handle* g, double out[3]);
THREESF_API int    threesf_h_bbox(const threesf_handle* g, double out[6]);
THREESF_API int    threesf_h_is_planar(const threesf_handle* g, double tol);
THREESF_API int    threesf_h_is_closed(const threesf_handle* g);
THREESF_API int    threesf_h_is_solid(const threesf_handle* g);
THREESF_API int    threesf_h_orientation(const threesf_handle* g);
THREESF_API double threesf_h_distance(const threesf_handle* a, const threesf_handle* b);
THREESF_API double threesf_h_max_distance(const threesf_handle* a, const threesf_handle* b);
THREESF_API int    threesf_h_closest_points(const threesf_handle* a, const threesf_handle* b, double out[6]);
THREESF_API int    threesf_h_farthest_points(const threesf_handle* a, const threesf_handle* b, double out[6]);
THREESF_API int    threesf_h_dwithin(const threesf_handle* a, const threesf_handle* b, double d);
THREESF_API int    threesf_h_dfully_within(const threesf_handle* a, const threesf_handle* b, double d);
THREESF_API int    threesf_h_intersects(const threesf_handle* a, const threesf_handle* b, double eps);

/* Plot primitives: one allocated xyz buffer containing points, segment pairs,
 * then triangle triples. counts[0..2] count primitives, not vertices.
 * edges=0 omits surface edges but preserves curves.
 * Free *data with threesf_free. Empty input returns NULL data and zero counts.
 */
THREESF_API int threesf_h_plot_data(const threesf_handle* g, int edges, double** data, size_t counts[3]);

/* 3D constructive operations. tol is a positive absolute distance tolerance.
 * Booleans accept closed solids/surfaces and return SOLID, MULTISOLID or EMPTY.
 * They are regularized: boundary-only intersections return EMPTY.
 * Self-intersecting inputs/results are refused, with last_warning populated.
 */
THREESF_API int threesf_h_self_intersects(const threesf_handle* g, double tol);
THREESF_API threesf_handle* threesf_h_convex_hull(const threesf_handle* g, double tol);
THREESF_API threesf_handle* threesf_h_intersection(const threesf_handle* a, const threesf_handle* b, double tol);
THREESF_API threesf_handle* threesf_h_union(const threesf_handle* a, const threesf_handle* b, double tol);
THREESF_API threesf_handle* threesf_h_difference(const threesf_handle* a, const threesf_handle* b, double tol);

/* Constructive — results are new handles (TIN Z / SOLID Z / same type) */
THREESF_API threesf_handle* threesf_h_tessellate(const threesf_handle* g);
THREESF_API threesf_handle* threesf_h_extrude(const threesf_handle* polygon, double dx, double dy, double dz);
THREESF_API threesf_handle* threesf_h_make_solid(const threesf_handle* surface);
THREESF_API threesf_handle* threesf_h_force_outward(const threesf_handle* surface);
THREESF_API threesf_handle* threesf_h_force_ccw(const threesf_handle* g);
THREESF_API threesf_handle* threesf_h_force_cw(const threesf_handle* g);

#ifdef __cplusplus
}
#endif
#endif
