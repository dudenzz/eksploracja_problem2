#!/usr/bin/env python3
"""
Build script for fpgrowth_fast Python module using pybind11.
This avoids Make shell issues on Windows by using Python directly.
"""

import subprocess
import sys
import os
import pybind11
import sysconfig

def build_module():
    """Compile the fpgrowth_fast module with static libraries."""
    
    # Get paths
    py_include = sysconfig.get_path("include")
    pybind_include = pybind11.get_include()
    py_libpath = os.path.join(sys.base_prefix, 'libs')
    py_version = f"{sys.version_info.major}{sys.version_info.minor}"
    
    # Files
    wrapper_src = "wrapper.cpp"
    output_file = "fpgrowth_fast.pyd"
    
    # Build command with static linking to avoid DLL load errors
    cmd = [
        "g++",
        "-std=c++17",
        "-Wall",
        "-O3",
        "-D_GNU_SOURCE",
        "-shared",
        "-static",
        "-static-libstdc++",
        "-static-libgcc",
        "-fPIC",
        f"-I{py_include}",
        f"-I{pybind_include}",
        wrapper_src,
        "-o", output_file,
        f"-L{py_libpath}",
        f"-lpython{py_version}"
    ]
    
    print(f"Building {output_file}...")
    print(f"Python include: {py_include}")
    print(f"Pybind11: {pybind_include}")
    print(f"Python libs: {py_libpath}")
    print()
    
    result = subprocess.run(cmd)
    
    if result.returncode == 0:
        print(f"\n✓ Successfully built {output_file}")
        print("Module is ready to import!")
        return 0
    else:
        print(f"\n✗ Compilation failed")
        return 1

if __name__ == "__main__":
    sys.exit(build_module())
