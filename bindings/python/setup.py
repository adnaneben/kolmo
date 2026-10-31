"""Builds the kolmo native library and bundles it beside the Python module.

`pip install .` from a full checkout compiles `libkolmo` with the project Makefile and ships it
inside the wheel, so the installed package has no runtime build dependency. Distributable
manylinux/macOS wheels are produced the same way in CI (one wheel per platform)."""
import os, shutil, subprocess, sys
from setuptools import setup
from setuptools.command.build_py import build_py
from setuptools.dist import Distribution

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LIBNAME = "libkolmo.dylib" if sys.platform == "darwin" else "libkolmo.so"

class BinaryDistribution(Distribution):
    def has_ext_modules(self):          # force a platform-tagged wheel (it bundles a .so)
        return True

class build_py_with_lib(build_py):
    def run(self):
        built = os.path.join(ROOT, "build", LIBNAME)
        if not os.path.exists(built):
            subprocess.check_call(["make", "-j", "lib"], cwd=ROOT)
        super().run()
        shutil.copyfile(built, os.path.join(self.build_lib, LIBNAME))  # sits next to kolmo.py

setup(distclass=BinaryDistribution, cmdclass={"build_py": build_py_with_lib})
