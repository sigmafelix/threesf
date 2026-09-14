"""Optional 3D plotting backends; importing threesf does not import either one."""
import ctypes

import numpy as np


def _plot_data(g, edges=True):
    from . import _lib, _err, _warn
    data = ctypes.POINTER(ctypes.c_double)()
    counts = (ctypes.c_size_t * 3)()
    rc = _lib.threesf_h_plot_data(g._h, int(edges), ctypes.byref(data), counts)
    try:
        _warn()
        if rc != 0:
            raise ValueError(_err() or "plot: invalid geometry")
        size = sum(n * width for n, width in zip(counts, (3, 6, 9)))
        values = np.ctypeslib.as_array(data, shape=(size,)).copy() if size else np.empty(0)
    finally:
        _lib.threesf_free(data)
    p, e, t = counts
    return (values[:3*p].reshape(-1, 3), values[3*p:3*p+6*e].reshape(-1, 2, 3),
            values[3*p+6*e:].reshape(-1, 3, 3))


def _limits(parts):
    vertices = [a.reshape(-1, 3) for part in parts for a in part if a.size]
    if not vertices:
        return np.array([[-.5, .5]] * 3)
    vertices = np.concatenate(vertices)
    lo, hi = vertices.min(axis=0), vertices.max(axis=0)
    span = hi - lo
    padding = np.maximum(span, max(span.max(), 1e-12) * .1) * .05
    # A single point has no natural extent.
    if not np.any(span):
        padding[:] = .5
    return np.column_stack([lo-padding, hi+padding])


def plot(g, *, interactive=False, ax=None, color="#4477aa", alpha=.7,
         edges=True, linewidth=1, point_size=5, title=None):
    """Plot a Geometry or iterable of geometries in XYZ coordinates.

    Static mode requires Matplotlib and returns an Axes3D (optionally ``ax``).
    Interactive mode requires Plotly and returns a Figure; call ``show()`` or
    ``write_html()`` on it. Nothing is displayed automatically. Color is a named
    or hex color, or one color per geometry. ``alpha`` controls face opacity;
    ``edges`` toggles polygon/mesh edges (curves are always drawn). Point sizes
    use the backend's marker-size units. Holes use the core tessellation.
    Empty inputs produce an empty scene. Invalid tessellation raises ValueError
    with the core's self-intersection warnings preserved.
    """
    from . import Geometry
    if not isinstance(interactive, bool) or not isinstance(edges, bool):
        raise TypeError("interactive and edges must be bools")
    geoms = [g] if isinstance(g, Geometry) else list(g)
    if not all(isinstance(item, Geometry) for item in geoms):
        raise TypeError("plot expects a Geometry or iterable of Geometry objects")
    colors = [color] * len(geoms) if isinstance(color, str) else list(color)
    if len(colors) != len(geoms):
        raise ValueError("provide one color or one color per geometry")
    if not np.isfinite(alpha) or not 0 <= alpha <= 1:
        raise ValueError("alpha must be between 0 and 1")
    if not np.isfinite(linewidth) or linewidth <= 0 or not np.isfinite(point_size) or point_size <= 0:
        raise ValueError("linewidth and point_size must be finite and positive")
    if interactive and ax is not None:
        raise ValueError("ax is only supported for static plotting")
    if interactive:
        try:
            import plotly.graph_objects as go
        except ImportError as exc:
            raise ImportError("Interactive plotting requires plotly; install threesf[plot].") from exc
    else:
        try:
            import matplotlib.pyplot as plt
            from matplotlib.colors import to_rgba
            from mpl_toolkits.mplot3d.art3d import Poly3DCollection, Line3DCollection
        except ImportError as exc:
            raise ImportError("Static plotting requires matplotlib; install threesf[plot].") from exc
        if ax is not None and getattr(ax, "name", None) != "3d":
            raise ValueError("ax must be a Matplotlib 3D axes")
    parts = [_plot_data(item, edges) for item in geoms]
    limits = _limits(parts)
    if interactive:
        fig = go.Figure()
        for idx, ((points, segments, triangles), col) in enumerate(zip(parts, colors)):
            group = str(idx)
            if len(triangles):
                verts = triangles.reshape(-1, 3)
                indices = np.arange(len(verts)).reshape(-1, 3)
                fig.add_trace(go.Mesh3d(x=verts[:, 0], y=verts[:, 1], z=verts[:, 2],
                    i=indices[:, 0], j=indices[:, 1], k=indices[:, 2], color=col,
                    opacity=alpha, flatshading=True, name=f"Geometry {idx+1}", legendgroup=group))
            if len(segments):
                joined = np.concatenate([segments, np.full((len(segments), 1, 3), np.nan)], axis=1).reshape(-1, 3)
                fig.add_trace(go.Scatter3d(x=joined[:, 0], y=joined[:, 1], z=joined[:, 2], mode="lines",
                    line=dict(color=col, width=linewidth), showlegend=False, legendgroup=group))
            if len(points):
                fig.add_trace(go.Scatter3d(x=points[:, 0], y=points[:, 1], z=points[:, 2], mode="markers",
                    marker=dict(color=col, size=point_size), name=f"Geometry {idx+1}", legendgroup=group))
        fig.update_layout(title=title, scene=dict(aspectmode="data", **{
            axis+"axis": dict(title=axis.upper(), range=limits[i].tolist()) for i, axis in enumerate("xyz")}))
        return fig
    if ax is None:
        ax = plt.figure().add_subplot(111, projection="3d")
    elif ax.has_data():
        existing = np.array(ax.get_w_lims()).reshape(3, 2)
        limits[:, 0] = np.minimum(limits[:, 0], existing[:, 0])
        limits[:, 1] = np.maximum(limits[:, 1], existing[:, 1])
    for (points, segments, triangles), col in zip(parts, colors):
        if len(triangles):
            normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
            lengths = np.linalg.norm(normals, axis=1)
            normals = normals / np.where(lengths > 0, lengths, 1)[:, None]
            shade = .4 + .6 * np.abs(normals @ (np.array([1, -2, 3])/np.sqrt(14)))
            rgba = np.tile(to_rgba(col), (len(triangles), 1))
            rgba[:, :3] *= shade[:, None]
            ax.add_collection3d(Poly3DCollection(triangles, facecolors=rgba, alpha=alpha, edgecolors="none"))
        if len(segments):
            ax.add_collection3d(Line3DCollection(segments, colors=col, linewidths=linewidth))
        if len(points):
            ax.scatter(*points.T, color=col, s=point_size**2)
    ax.set(xlim=limits[0], ylim=limits[1], zlim=limits[2], xlabel="X", ylabel="Y", zlabel="Z")
    ax.set_box_aspect(limits[:, 1]-limits[:, 0])
    if title is not None:
        ax.set_title(title)
    return ax
