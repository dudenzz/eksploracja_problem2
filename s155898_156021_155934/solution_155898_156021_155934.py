import subprocess
import json
import config
import os

# NOTE: Setup solution according to SETUP.md

def compile_cpp():
    #Kompilacja c++ 
    if os.path.exists("main.exe"):
        return
    
    result = subprocess.run(
        ["g++", "-std=c++17", "-O2", "-fopenmp",
         "src/main.cpp", "src/fp_growth.cpp", "src/read_data.cpp",
         "-o", "main.exe"],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(result.stderr)
        raise RuntimeError(f"Kompilacja C++ nie powiodła się")
    
def solve(min_support, min_confidence, verbose=False):
    compile_cpp()
    
    result = subprocess.run(
        ["./main.exe", str(min_support), str(min_confidence), config.datapath, "1" if verbose else "0"],
        capture_output=True,
        text=True
    )

    rules = []
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout or f"main.exe exited with code {result.returncode}")

    for line in result.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        rules.append(json.loads(line))

    if verbose:
        for rule in rules:
            print(f"{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}")

    return rules


if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=False)
