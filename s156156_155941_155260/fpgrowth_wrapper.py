from __future__ import annotations

import subprocess
import os
from pathlib import Path
from shutil import which

import config


ROOT = Path(__file__).resolve().parent
CPP_PATH = ROOT / "fpgrowth.cpp"
BUILD_DIR = ROOT / "build"
EXE_PATH = BUILD_DIR / "fpgrowth.exe"
ITEM_SEPARATOR = "\x1f"


def _compiler() -> tuple[str, str]:
    configured_compiler = os.environ.get("CXX")
    if configured_compiler:
        configured_path = Path(configured_compiler)
        if configured_path.exists():
            name = configured_path.name.lower()
            if name == "cl.exe":
                return "cl", str(configured_path)
            if name in {"g++.exe", "clang++.exe", "g++", "clang++"}:
                return configured_path.stem.lower(), str(configured_path)

    for compiler in ("g++", "clang++"):
        path = which(compiler)
        if path is not None:
            return compiler, path

    path = which("cl")
    if path is not None:
        return "cl", path

    raise RuntimeError(
        "Nie znaleziono kompilatora C++ w PATH. Najprosciej zainstaluj MSYS2/MinGW i dodaj "
        "C:\\msys64\\ucrt64\\bin do PATH albo ustaw CXX na pelna sciezke do g++.exe/clang++.exe."
    )


def _compile_if_needed() -> None:
    if EXE_PATH.exists() and EXE_PATH.stat().st_mtime >= CPP_PATH.stat().st_mtime:
        return

    BUILD_DIR.mkdir(exist_ok=True)

    compiler_name, compiler_path = _compiler()

    if compiler_name == "cl":
        command = [
            compiler_path,
            "/std:c++17",
            "/O2",
            "/DNDEBUG",
            "/EHsc",
            "/openmp",
            str(CPP_PATH),
            f"/Fe:{EXE_PATH}",
        ]
    else:
        command = [
            compiler_path,
            "-std=c++17",
            "-O3",
            "-DNDEBUG",
            "-fopenmp",
            str(CPP_PATH),
            "-o",
            str(EXE_PATH),
        ]

    try:
        subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        details = error.stderr.strip() or error.stdout.strip() or str(error)
        raise RuntimeError(f"Kompilacja fpgrowth.cpp nie powiodla sie: {details}") from error


def _split_items(raw: str) -> list[str]:
    if not raw:
        return []
    return raw.split(ITEM_SEPARATOR)


def solve(min_support: float, min_confidence: float, verbose: bool = False) -> list[dict]:
    _compile_if_needed()

    command = [
        str(EXE_PATH),
        config.datapath,
        str(min_support),
        str(min_confidence),
        "0",
    ]

    try:
        completed = subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        details = error.stderr.strip() or error.stdout.strip() or str(error)
        raise RuntimeError(f"Uruchomienie fpgrowth.exe nie powiodlo sie: {details}") from error

    if completed.stderr:
        print(completed.stderr, end="")

    rules = []

    for line in completed.stdout.splitlines():
        if not line.startswith("RULE\t"):
            continue

        _, raw_a, raw_b, raw_supp, raw_conf = line.split("\t", 4)
        rules.append(
            {
                "A": _split_items(raw_a),
                "B": _split_items(raw_b),
                "supp": float(raw_supp),
                "conf": float(raw_conf),
            }
        )

    if not verbose:
        print(f'Wygenerowano {len(rules)} reguł.')
        for rule in rules:
            print(f'{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}')
    
    return rules
