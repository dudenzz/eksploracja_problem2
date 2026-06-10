import polars as pl
from itertools import combinations
import config

def solve(min_support, min_confidence, verbose=False):
    try:
        df = pl.read_csv(
            config.datapath,
            columns=["Invoice", "StockCode"],
            schema_overrides={"Invoice": pl.String, "StockCode": pl.String},
            ignore_errors=True 
        )
    except Exception:
        print("Błąd odczytu pliku CSV.")
        return []

    df = df.drop_nulls()
    df = df.filter(
        pl.col("Invoice").str.contains(r"^\d+$") & 
        (pl.col("StockCode").str.len_chars() > 0)
    )

    n_trans = df.select("Invoice").n_unique()
    min_supp_count = min_support * n_trans

    grouped = (
        df.group_by("StockCode")
        .agg(pl.col("Invoice").unique().alias("tids"))
        .filter(pl.col("tids").list.len() >= min_supp_count)
    )

    frequent_itemsets = {}
    items = []

    for row in grouped.iter_rows():
        item = row[0]
        # Invoices ładujemy do wbudowanego w pythona frozensetu
        tids = frozenset(row[1]) 
        froz_item = frozenset([item])
        items.append((froz_item, tids))
        frequent_itemsets[froz_item] = len(tids)

    items.sort(key=lambda x: len(x[1]))

    def eclat(prefix, items_to_add):
        for i, (item_a, tids_a) in enumerate(items_to_add):
            new_itemset = prefix.union(item_a)
            frequent_itemsets[new_itemset] = len(tids_a)
            
            new_items_to_add = []
            for item_b, tids_b in items_to_add[i+1:]:
                inter = tids_a.intersection(tids_b)
                if len(inter) >= min_supp_count:
                    new_items_to_add.append((item_b, inter))
            
            if new_items_to_add:
                eclat(new_itemset, new_items_to_add)

    for i, (item_a, tids_a) in enumerate(items):
        new_items_to_add = []
        for item_b, tids_b in items[i+1:]:
            inter = tids_a.intersection(tids_b)
            if len(inter) >= min_supp_count:
                new_items_to_add.append((item_b, inter))
        if new_items_to_add:
            eclat(item_a, new_items_to_add)

    rules = []
    for itemset, count in frequent_itemsets.items():
        if len(itemset) > 1:
            support = count / n_trans
            for i in range(1, len(itemset)):
                for antecedent in combinations(itemset, i):
                    antecedent = frozenset(antecedent)
                    supp_a = frequent_itemsets.get(antecedent)
                    if supp_a:
                        confidence = count / supp_a
                        if confidence >= min_confidence:
                            consequent = itemset - antecedent
                            # Zwraca poprawny słownik wg instrukcji
                            rules.append({
                                'A': list(antecedent),
                                'B': list(consequent),
                                'supp': support,
                                'conf': confidence
                            })
                            
    return rules