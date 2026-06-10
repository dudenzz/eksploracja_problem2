import polars as pl
import config
from itertools import combinations


def solve(min_support, min_confidence, verbose=False):
    try:
        # Odczyt danych
        df = pl.read_csv(
            config.datapath,
            has_header=True,
            encoding='iso-8859-1',
            columns=["Invoice", "StockCode"],
            schema_overrides={"Invoice": pl.String, "StockCode": pl.String},
            ignore_errors=True
        )
    except Exception as e:
        if verbose:
            print(f"Błąd ładowania danych: {e}")
        return []

    df = df.drop_nulls()
    df = df.filter(pl.col("Invoice").str.contains(r"^\d+$"))
    df = df.with_columns(
        pl.col("Invoice").cast(pl.Categorical).to_physical().alias("TID")
    )

    n_trans = df.select(pl.col("TID").n_unique()).item()
    if n_trans == 0:
        return []

    min_supp_count = min_support * n_trans
    grouped = df.group_by("StockCode").agg(pl.col("TID"))

    raw_counts = {}
    item_tids = {}

    for row in grouped.iter_rows():
        item = row[0]
        tid_list = row[1]
        raw_counts[item] = len(tid_list)
        tid_set = set(tid_list)
        if len(tid_set) >= min_supp_count:
            item_tids[item] = tid_set

    sorted_items = sorted(item_tids.items(), key=lambda x: len(x[1]))
    frequent_itemsets = {}

    def mine_eclat(prefix, tids, conditional_items):
        frequent_itemsets[frozenset(prefix)] = len(tids)
        for i, (item_a, tids_a) in enumerate(conditional_items):
            new_tids = tids.intersection(tids_a)
            if len(new_tids) >= min_supp_count:
                new_prefix = prefix + (item_a,)
                new_conditional_items = []

                for item_b, tids_b in conditional_items[i + 1:]:
                    new_tids_b = new_tids.intersection(tids_b)
                    if len(new_tids_b) >= min_supp_count:
                        new_conditional_items.append((item_b, new_tids_b))

                mine_eclat(new_prefix, new_tids, new_conditional_items)

    for i, (item_a, tids_a) in enumerate(sorted_items):
        conditional_items = []
        for item_b, tids_b in sorted_items[i + 1:]:
            new_tids_b = tids_a.intersection(tids_b)
            if len(new_tids_b) >= min_supp_count:
                conditional_items.append((item_b, tids_b))
        mine_eclat((item_a,), tids_a, conditional_items)

    rules = []
    inv_n_trans = 1.0 / n_trans

    for itemset, count in frequent_itemsets.items():
        if len(itemset) > 1:
            support = count * inv_n_trans
            for i in range(1, len(itemset)):
                for antecedent in combinations(itemset, i):
                    antecedent = frozenset(antecedent)
                    consequent = itemset - antecedent

                    if len(antecedent) == 1:
                        item = next(iter(antecedent))
                        supp_a = raw_counts.get(item, 0)
                    else:
                        supp_a = frequent_itemsets.get(antecedent)

                    if supp_a:
                        confidence = count / supp_a
                        if confidence >= min_confidence:
                            rules.append({
                                'A': list(antecedent),
                                'B': list(consequent),
                                'supp': support,
                                'conf': confidence,
                            })

    if not verbose:
        print(f'Wygenerowano {len(rules)} reguł.')
        for rule in rules:
            print(f"{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}")

    return rules