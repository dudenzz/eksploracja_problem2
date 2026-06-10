def solve(min_support, min_confidence, verbose=False):
    import config
    import os
    import subprocess

    d = os.path.dirname(__file__)
    cpp = os.path.join(d, "solution_158058_155077.cpp")
    exe = os.path.join(d, "solution_158058_155077.exe" if os.name == "nt" else "solution_158058_155077")
    data = getattr(config, "datapath", os.path.join(d, "data", "online_retail_II.csv"))

    if not os.path.exists(data):
        data = os.path.join(d, "data", "online_retail_II.csv")

    if not os.path.exists(exe) or os.path.getmtime(cpp) > os.path.getmtime(exe):
        r = subprocess.run(["g++", "-O3", "-std=c++17", cpp, "-o", exe], cwd=d, capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(r.stderr)

    r = subprocess.run([exe, data, str(min_support), str(min_confidence)], cwd=d, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr or f"C++ zakonczyl sie kodem {r.returncode}.")

    rules = []
    for line in r.stdout.splitlines():
        a, b, s, c = line.split("\t")
        rules.append({"A": a.split("|") if a else [], "B": b.split("|") if b else [], "supp": float(s), "conf": float(c)})

    if verbose:
        print(f"Wygenerowano {len(rules)} regul.")
    return rules


if __name__ == "__main__":
    import config
    solve(config.min_support, config.min_confidence, True)
