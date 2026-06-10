from setuptools import setup, Extension
import pybind11
import sys
import platform

pybind11_inc = pybind11.get_include()

_compiler = platform.python_compiler().lower()
_is_msvc = sys.platform == "win32" and "msc" in _compiler

if _is_msvc:
    compile_args = [
        "/O2",
        "/GL",
        "/fp:fast",
        "/arch:AVX2",
        "/std:c++17",
    ]
    link_args = ["/LTCG"]
else:
    compile_args = [
        "-O3",
        "-march=native",
        "-funroll-loops",
        "-ffast-math",
        "-std=c++17",
    ]
    link_args = []

ext = Extension(
    "assoc_rules_engine",
    sources=["cpp_solution/eclat_engine.cpp"],
    include_dirs=[pybind11_inc],
    extra_compile_args=compile_args,
    extra_link_args=link_args,
    language="c++",
)

setup(
    name="assoc_rules_engine",
    version="1.0",
    ext_modules=[ext],
)
