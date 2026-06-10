import subprocess
import config


def solve(min_support, min_confidence, verbose=False):
    command = [
        "rozwiazaniec++.exe",
        config.datapath,
        str(min_support),
        str(min_confidence),
        str(int(verbose))
    ]

    result = subprocess.run(
        command,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print("Błąd programu C++:")
        print(result.stderr)
        return []

    rules = []

    for line in result.stdout.splitlines():
        if line.startswith("RULE;"):
            parts = line.split(";")

            A = parts[1].split("|") if parts[1] else []
            B = parts[2].split("|") if parts[2] else []
            supp = float(parts[3])
            conf = float(parts[4])

            rules.append({
                "A": A,
                "B": B,
                "supp": supp,
                "conf": conf
            })

    if not verbose:
        print(f"Znaleziono {len(rules)} reguł.")
        for rule in rules:
            print(
                f"{rule['A']} => {rule['B']} "
                f"Support: {rule['supp']}, "
                f"Confidence: {rule['conf']}"
            )

    return rules