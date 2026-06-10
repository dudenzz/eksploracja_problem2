from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools
import pybind11
import sysconfig

class get_pybind_include:
    def __str__(self):
        return pybind11.get_include()

ext_modules = [
    Extension(
        'fpgrowth_fast',
        ['wrapper.cpp'],
        include_dirs=[
            get_pybind_include(),
            sysconfig.get_path("include"),
        ],
        language='c++',
        extra_compile_args=[
            '-std=c++17',
            '-O3',
            '-D_GNU_SOURCE',
            '-fpermissive'
        ],
        extra_link_args=[
            '-static',
            '-static-libstdc++',
            '-static-libgcc'
        ]
    ),
]

setup(
    name='fpgrowth_fast',
    description='Fast FP-Growth algorithm in C++ with pybind11',
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.6.0'],
    cmdclass={'build_ext': build_ext},
    zip_safe=False,
)
