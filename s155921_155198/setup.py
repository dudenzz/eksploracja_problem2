from setuptools import setup, Extension
import pybind11
import shutil
import os

ext_modules = [
    Extension(
        "eclat_155921_execute",
        ["eclat_155921.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["/O2", "/std:c++17"]
    ),
    Extension(
        "fpgrowth_155198_execute",
        ["fpgrowth_155198.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["/O2", "/std:c++17"]
    ),
]

setup(
    name="mining_cpp",
    ext_modules=ext_modules,
)

folders_to_remove = ['build', 'dist', 'mining_cpp.egg-info']
for folder in folders_to_remove:
    if os.path.exists(folder):
        shutil.rmtree(folder)
        print(f"Usunięto: {folder}")