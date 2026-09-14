import builtins
import subprocess
import sys

import numpy as np
import pytest

import threesf as g3
from threesf.plotting import _plot_data


def mixed():
    polygon = g3.polygon([(0, 2, 0), (4, 2, 0), (4, 2, 4), (0, 2, 4)],
                         holes=[[(1, 2, 1), (1, 2, 3), (3, 2, 3), (3, 2, 1)]])
    return g3.collection([g3.point(5, 3, 6), g3.collection([
        g3.linestring([(0, 0, 0), (1, 2, 3)]), polygon])])


def test_native_primitives_preserve_holes_and_curves():
    points, segments, triangles = _plot_data(mixed(), edges=False)
    np.testing.assert_array_equal(points, [[5, 3, 6]])
    np.testing.assert_array_equal(segments, [[[0, 0, 0], [1, 2, 3]]])
    assert triangles.shape == (8, 3, 3)
    assert np.all(triangles[:, :, 1] == 2)
    assert np.linalg.norm(np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0]), axis=1).sum()/2 == pytest.approx(12)
    centers = triangles.mean(axis=1)
    assert not np.any((centers[:, 0] > 1) & (centers[:, 0] < 3) & (centers[:, 2] > 1) & (centers[:, 2] < 3))
    assert len(_plot_data(mixed())[1]) == 9
    assert [len(part) for part in _plot_data(g3.read_wkt("GEOMETRYCOLLECTION EMPTY"))] == [0, 0, 0]


def test_hollow_and_disconnected_primitives():
    def box(x, size):
        return g3.st_extrude(g3.polygon([(x, x, x), (x+size, x, x), (x+size, x+size, x), (x, x+size, x)]), 0, 0, size)
    hollow = g3.st_3ddifference(box(0, 4), box(1, 2))
    ts = _plot_data(hollow)[2]
    assert np.linalg.norm(np.cross(ts[:, 1]-ts[:, 0], ts[:, 2]-ts[:, 0]), axis=1).sum()/2 == pytest.approx(120)
    assert len(_plot_data(g3.st_3dunion(box(0, 1), box(3, 1)))[2]) > 0


def test_static_export_and_existing_axes(tmp_path):
    pytest.importorskip("matplotlib")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    ax = g3.plot(mixed(), edges=False, title="XYZ")
    assert ax.name == "3d" and len(ax.collections) == 3
    assert ax.get_zlim()[1] > 6
    assert g3.point(20, 20, 20).plot(ax=ax) is ax
    assert ax.get_xlim()[0] < 0 and ax.get_xlim()[1] > 20
    output = tmp_path / "geometry.png"
    ax.figure.savefig(output)
    assert output.stat().st_size > 1000
    plt.close(ax.figure)
    fig, ax2 = plt.subplots()
    with pytest.raises(ValueError, match="3D axes"):
        g3.plot(mixed(), ax=ax2)
    plt.close(fig)


def test_interactive_mesh_indices_and_html(tmp_path):
    pytest.importorskip("plotly")
    fig = g3.plot([mixed(), g3.point(8, 9, 10)], interactive=True, edges=False, color=["red", "blue"], alpha=.4)
    mesh = next(t for t in fig.data if t.type == "mesh3d")
    assert len(mesh.i) == 8 and mesh.opacity == .4
    np.testing.assert_array_equal(mesh.i, np.arange(0, 24, 3))
    assert fig.layout.scene.aspectmode == "data"
    curve = next(t for t in fig.data if t.type == "scatter3d" and t.mode == "lines")
    assert len(curve.x) == 3 and np.isnan(curve.x[-1])
    assert fig.data[-1].marker.color == "blue"
    output = tmp_path / "geometry.html"
    fig.write_html(output, include_plotlyjs=True)
    assert "mesh3d" in output.read_text()
    assert not g3.plot([], interactive=True).data


def test_dependencies_are_optional():
    code = "import sys, threesf; assert 'matplotlib' not in sys.modules; assert 'plotly' not in sys.modules"
    subprocess.run([sys.executable, "-c", code], check=True)


@pytest.mark.parametrize("interactive,package", [(False, "matplotlib"), (True, "plotly")])
def test_missing_dependency_message(monkeypatch, interactive, package):
    original = builtins.__import__
    def blocked(name, *args, **kwargs):
        if name.split(".")[0] == package:
            raise ImportError("missing backend")
        return original(name, *args, **kwargs)
    monkeypatch.setattr(builtins, "__import__", blocked)
    with pytest.raises(ImportError, match=r"threesf\[plot\]"):
        g3.point(0, 0, 0).plot(interactive=interactive)


@pytest.mark.parametrize("options", [{"alpha": -1}, {"linewidth": 0}, {"point_size": float("nan")}, {"color": []}])
def test_invalid_options(options):
    with pytest.raises(ValueError):
        g3.plot(g3.point(0, 0, 0), **options)


def test_self_intersection_error_is_not_hidden():
    with pytest.warns(RuntimeWarning):
        bad = g3.polygon([(0, 0), (2, 2), (0, 2), (2, 0)])
    with pytest.warns(RuntimeWarning, match="self-intersection"), pytest.raises(ValueError, match="self-intersection"):
        _plot_data(bad)
