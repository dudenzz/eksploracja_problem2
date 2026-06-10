import time
import pandas as pd
from mlxtend.frequent_patterns import fpgrowth
from mlxtend.preprocessing import TransactionEncoder
from itertools import combinations
import config

def solve(min_support, min_confidence, verbose=True):
    start_time = time.perf_counter()

    try:
        df_raw = pd.read_csv(config.datapath, encoding='ISO-8859-1',
                             usecols=[0, 1], dtype={0: str, 1: str})

        mask = df_raw.iloc[:, 0].str.isdigit() & df_raw.iloc[:, 1].notna()
        df_clean = df_raw[mask]

        transactions = df_clean.groupby(df_clean.columns[0])[df_clean.columns[1]].apply(list).tolist()
    except Exception as e:
        if verbose: print(f"Błąd danych: {e}")
        return []

    te = TransactionEncoder()
    te_ary = te.fit(transactions).transform(transactions, sparse=True)
    df_bin = pd.DataFrame.sparse.from_spmatrix(te_ary, columns=te.columns_)

    frequent_itemsets = fpgrowth(df_bin, min_support=min_support, use_colnames=True)

    support_map = {frozenset(item): supp for item, supp in
                   zip(frequent_itemsets['itemsets'], frequent_itemsets['support'])}

    rules = []

    for itemset, support in support_map.items():
        if len(itemset) < 2:
            continue

        items = list(itemset)
        for r in range(1, len(items)):
            for a_tuple in combinations(items, r):
                antecedent = frozenset(a_tuple)
                consequent = itemset - antecedent

                support_a = support_map.get(antecedent)

                if support_a:
                    confidence = support / support_a
                    if confidence >= min_confidence:
                        rules.append({
                            'A': list(antecedent),
                            'B': list(consequent),
                            'supp': float(support),
                            'conf': float(confidence)
                        })

    execution_time = time.perf_counter() - start_time

    if verbose:
        print("\n" + "=" * 50)
        print(f"RAPORT: FAST SOLUTION")
        print(f"Czas wykonania: {execution_time:.4f} sekund")
        print(f"Liczba reguł: {len(rules)}")
        print("-" * 50)
        for rule in rules[:10]:
            ant = "{" + ", ".join(rule['A']) + "}"
            cons = "{" + ", ".join(rule['B']) + "}"
            print(f"{ant:30} => {cons:30} | supp: {rule['supp']:.3f} | conf: {rule['conf']:.3f}")
        if len(rules) > 10: print(f"... i {len(rules)-10} więcej.")
        print("=" * 50 + "\n")

    return rules

if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=False)