from setuptools import setup, Extension
from Cython.Build import cythonize
import sys

ext = Extension(
    "solve_cy",
    sources=["solve_cy.pyx"],
    extra_compile_args=["/O2"] if sys.platform == "win32" else ["-O3", "-march=native"],
)

setup(
    name="solve_cy",
    ext_modules=cythonize(
        [ext],
        compiler_directives={
            "language_level": 3,
            "boundscheck": False,
            "wraparound": False,
            "cdivision": True,
            "nonecheck": False,
        },
    ),
)
