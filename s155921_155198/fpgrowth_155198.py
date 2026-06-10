import fpgrowth_155198_execute
import config

def solve(supp, conf, verbose=False):
    path = config.datapath
    result = fpgrowth_155198_execute.solve(path, supp, conf)
    all_frequent = result.frequent_itemsets
    rules_list = result.rules

    res = [{ 'A': r.A, 'B': r.B, 'supp': r.supp, 'conf': r.conf } for r in rules_list ]

    if not verbose:
        print(f"Generowanie reguł z {len(all_frequent)} zbiorów częstych...")
        print(f"Wygenerowano {len(res)} reguł.")
        for rule in res:
            print(f"{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}")

    return res

if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=False)