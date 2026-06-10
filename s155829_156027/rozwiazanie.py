import config
from collections import defaultdict
from itertools import combinations

def solve(min_support, min_confidence, verbose=False):
    # 1. Ładowanie i przygotowanie danych (pionowa baza danych)
    # item_tidsets: { produkt: {id_transakcji1, id_transakcji2, ...} }
    item_tidsets = defaultdict(set)
    n_trans = 0

    try:
        with open(config.datapath, 'r', encoding='ISO-8859-1') as f:
            next(f)  # Pominiecie nagłówka
            trans_dict = defaultdict(list)
            
            # Grupowanie produktów po ID transakcji
            for line in f:
                parts = line.strip().split(',')
                if len(parts) >= 2 and parts[0].isdigit():
                    trans_dict[parts[0]].append(parts[1])
            
            n_trans = len(trans_dict)
            if n_trans == 0: return []

            # Budowanie formatu pionowego (klucz do szybkości ECLAT)
            for tid, (t_id, items) in enumerate(trans_dict.items()):
                for item in set(items):
                    item_tidsets[item].add(tid)

    except Exception as e:
        print(f"Błąd ładowania danych: {e}")
        return []

    min_supp_count = min_support * n_trans
    frequent_itemsets = {}

    # 2. Rekurencyjna funkcja ECLAT
    def eclat(prefix, items_dict, frequent_itemsets):
        # Sortujemy produkty, aby zachować porządek (opcjonalnie dla stabilności)
        sorted_items = sorted(items_dict.items(), key=lambda x: len(x[1]), reverse=True)
        
        for i, (item, tidset) in enumerate(sorted_items):
            new_itemset = prefix | frozenset([item])
            frequent_itemsets[new_itemset] = len(tidset)

            # Budujemy nową listę produktów do sprawdzenia w głąb
            suffix_items = {}
            for next_item, next_tidset in sorted_items[i + 1:]:
                # KLUCZ: Część wspólna zbiorów ID transakcji
                intersection = tidset & next_tidset
                if len(intersection) >= min_supp_count:
                    suffix_items[next_item] = intersection

            if suffix_items:
                eclat(new_itemset, suffix_items, frequent_itemsets)

    # Filtrowanie produktów spełniających min_support na starcie
    initial_items = {
        item: tids for item, tids in item_tidsets.items() 
        if len(tids) >= min_supp_count
    }

    # Uruchomienie algorytmu
    eclat(frozenset(), initial_items, frequent_itemsets)

    # 3. Generowanie reguł
    rules = []
    for itemset, count in frequent_itemsets.items():
        if len(itemset) > 1:
            support = count / n_trans
            for i in range(1, len(itemset)):
                for antecedent in combinations(itemset, i):
                    antecedent = frozenset(antecedent)
                    consequent = itemset - antecedent
                    
                    supp_a_count = frequent_itemsets.get(antecedent)
                    if supp_a_count:
                        confidence = count / supp_a_count
                        if confidence >= min_confidence:
                            rules.append({
                                'A': list(antecedent),
                                'B': list(consequent),
                                'supp': support,
                                'conf': confidence
                            })

    # Wyświetlanie wyników
    print(f"Znaleziono {len(frequent_itemsets)} częstych zbiorów.")
    print(f"Wygenerowano {len(rules)} reguł.")
    
    if not verbose:
        for rule in rules:
            print(f"{rule['A']} => {rule['B']} | Conf: {rule['conf']:.2f}, Supp: {rule['supp']:.2f}")

    return rules

if __name__ == "__main__":
    # Upewnij się, że plik config istnieje i ma te pola
    solve(config.min_support, config.min_confidence)