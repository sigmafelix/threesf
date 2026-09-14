"""Builds the threesf C ABI as a ctypes-loadable shared library inside the package.

The library is compiled from ../src and ../include when building from the repo,
or from the copies vendored into threesf/_native/ in an sdist.
"""
import os
import shutil
import sys
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

HERE = Path(__file__).resolve().parent
REPO_SRC, REPO_INC = HERE.parent / "core" / "src", HERE.parent / "core" / "include"
VENDOR = HERE / "threesf" / "_native"


def vendor_sources() -> Path:
    """Copy C++ sources next to the package so sdists are self-contained."""
    if REPO_SRC.exists():
        if VENDOR.exists():
            shutil.rmtree(VENDOR)
        shutil.copytree(REPO_INC, VENDOR / "include")
        shutil.copytree(REPO_SRC, VENDOR / "src")
    return VENDOR


native = vendor_sources()
rel = lambda p: os.path.relpath(p, HERE)  # setuptools rejects absolute source paths
extra = ["/std:c++17", "/O2", "/EHsc"] if sys.platform == "win32" else ["-std=c++17", "-O2", "-fvisibility=hidden"]


class CtypesExtension(Extension):
    pass


class build_ctypes(build_ext):
    """Produce a plain shared library (no PyInit symbol) that ctypes loads."""

    def get_export_symbols(self, ext):
        return ext.export_symbols if isinstance(ext, CtypesExtension) else super().get_export_symbols(ext)

    def get_ext_filename(self, ext_name):
        if ext_name.endswith("_libthreesf"):
            base = ext_name.replace(".", os.sep)
            return base + (".dll" if sys.platform == "win32" else ".dylib" if sys.platform == "darwin" else ".so")
        return super().get_ext_filename(ext_name)


setup(
    ext_modules=[
        CtypesExtension(
            "threesf._libthreesf",
            sources=[rel(native / "src" / "threesf_c.cpp")],
            include_dirs=[rel(native / "include")],
            define_macros=[("THREESF_BUILDING", "1")],
            extra_compile_args=extra,
            language="c++",
        )
    ],
    cmdclass={"build_ext": build_ctypes},
)
