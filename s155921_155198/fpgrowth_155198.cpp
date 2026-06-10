#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <memory>

using namespace std;
namespace py = pybind11;

struct AssociationRule {
    vector<string> A;
    vector<string> B;
    double supp;
    double conf;
};

struct FrequentItemset {
    vector<string> items;
    double supp;
};

struct MiningResult {
    vector<FrequentItemset> frequent_itemsets;
    vector<AssociationRule> rules;
};

map<vector<string>, double> supp_cache;


struct FPNode {
    string item;
    int count;
    FPNode* parent;
    FPNode* next_sibling;   // lista rodzeństwa (dzieci węzła parent)
    FPNode* node_link;      // łańcuch nagłówkowy (header table)
    vector<FPNode*> children;

    FPNode(const string& item, int count, FPNode* parent)
        : item(item), count(count), parent(parent),
          next_sibling(nullptr), node_link(nullptr) {}
};

// ──────────────────────────────────────────────
// Węzeł korzenia (sentinel)
// ──────────────────────────────────────────────

struct FPTree {
    FPNode* root;
    // header table: item -> pierwszy węzeł w łańcuchu
    unordered_map<string, FPNode*> header;
    // kolejność itemów wg malejącej częstości (globalna)
    vector<string> item_order;

    FPTree() : root(new FPNode("", 0, nullptr)) {}

    ~FPTree() { destroy(root); }

    void destroy(FPNode* node) {
        for (auto* c : node->children) destroy(c);
        delete node;
    }
};

// ──────────────────────────────────────────────
// Wstawianie transakcji do FP-Tree
// ──────────────────────────────────────────────

void insert_transaction(FPTree& tree, const vector<string>& trans) {
    FPNode* cur = tree.root;
    for (const string& item : trans) {
        FPNode* child = nullptr;
        for (auto* c : cur->children) {
            if (c->item == item) { child = c; break; }
        }
        if (!child) {
            child = new FPNode(item, 0, cur);
            cur->children.push_back(child);
            // dodaj do łańcucha nagłówkowego
            if (tree.header.count(item)) {
                // dołącz na koniec łańcucha
                FPNode* tail = tree.header[item];
                while (tail->node_link) tail = tail->node_link;
                tail->node_link = child;
            } else {
                tree.header[item] = child;
            }
        }
        child->count++;
        cur = child;
    }
}

// ──────────────────────────────────────────────
// Budowanie FP-Tree z listy transakcji
// ──────────────────────────────────────────────

unique_ptr<FPTree> build_fptree(
    const vector<vector<string>>& transactions,
    const unordered_map<string, int>& freq,
    int min_supp_cnt,
    const vector<string>& item_order)
{
    auto tree = make_unique<FPTree>();
    tree->item_order = item_order;

    // Mapa pozycji dla szybkiego sortowania
    unordered_map<string, int> order_map;
    for (int i = 0; i < (int)item_order.size(); i++)
        order_map[item_order[i]] = i;

    for (const auto& trans : transactions) {
        // Odfiltruj rzadkie, posortuj wg item_order (malejąca częstość)
        vector<string> filtered;
        for (const string& it : trans)
            if (freq.count(it) && freq.at(it) >= min_supp_cnt)
                filtered.push_back(it);

        sort(filtered.begin(), filtered.end(), [&](const string& a, const string& b) {
            return order_map.at(a) < order_map.at(b);
        });

        insert_transaction(*tree, filtered);
    }
    return tree;
}

// ──────────────────────────────────────────────
// Generowanie reguł asocjacyjnych
// ──────────────────────────────────────────────

void generate_rules(
    const vector<string>& itemset,
    double supp,
    double min_conf,
    vector<AssociationRule>& rules)
{
    int n = itemset.size();
    if (n < 2) return;

    for (int mask = 1; mask < (1 << n) - 1; mask++) {
        vector<string> left, right;
        for (int j = 0; j < n; j++) {
            if ((mask >> j) & 1) left.push_back(itemset[j]);
            else                 right.push_back(itemset[j]);
        }
        sort(left.begin(), left.end());

        auto it = supp_cache.find(left);
        if (it == supp_cache.end()) continue; 

        double conf = supp / it->second;
        if (conf >= min_conf) {
            AssociationRule r;
            r.A = left;
            r.B = right;
            r.supp = supp;
            r.conf = conf;
            rules.push_back(r);
        }
    }
}

// ──────────────────────────────────────────────
// Wydobycie ścieżek prefiksowych (conditional pattern base)
// ──────────────────────────────────────────────

vector<pair<vector<string>, int>> get_conditional_patterns(
    FPTree& tree, const string& item)
{
    vector<pair<vector<string>, int>> patterns;
    FPNode* node = tree.header.count(item) ? tree.header.at(item) : nullptr;

    while (node) {
        vector<string> path;
        FPNode* cur = node->parent;
        while (cur && !cur->item.empty()) {
            path.push_back(cur->item);
            cur = cur->parent;
        }
        if (!path.empty())
            patterns.push_back({path, node->count});
        node = node->node_link;
    }
    return patterns;
}

// ──────────────────────────────────────────────
// Rekurencyjna eksploracja FP-Growth
// ──────────────────────────────────────────────

void fpgrowth_recursive(
    FPTree& tree,
    const vector<string>& suffix,
    int min_supp_cnt,
    int total_n,
    double min_conf,
    MiningResult& result)
{
    // Iterujemy po itemach w odwrotnej kolejności item_order
    // (od najmniej do najbardziej częstych) – taki jest standard FP-Growth
    vector<string> items = tree.item_order;
    reverse(items.begin(), items.end());

    for (const string& item : items) {
        if (!tree.header.count(item)) continue;

        // Zlicz wsparcie: suma count'ów w łańcuchu
        int item_supp = 0;
        FPNode* node = tree.header.at(item);
        while (node) { item_supp += node->count; node = node->node_link; }

        if (item_supp < min_supp_cnt) continue;

        // Nowy zbiór częsty = {item} ∪ suffix
        vector<string> new_itemset = {item};
        new_itemset.insert(new_itemset.end(), suffix.begin(), suffix.end());
        sort(new_itemset.begin(), new_itemset.end());

        double supp = (double)item_supp / total_n;
        supp_cache[new_itemset] = supp;

        FrequentItemset fi;
        fi.items = new_itemset;
        fi.supp = supp;
        result.frequent_itemsets.push_back(fi);

        generate_rules(new_itemset, supp, min_conf, result.rules);

        // Zbuduj conditional pattern base i nowy FP-Tree
        auto cond_patterns = get_conditional_patterns(tree, item);

        // Częstości itemów w conditional pattern base
        unordered_map<string, int> cond_freq;
        for (auto& [path, cnt] : cond_patterns)
            for (const string& it : path)
                cond_freq[it] += cnt;

        // Filtruj i sortuj – nowy item_order dla conditional tree
        vector<string> cond_order;
        for (auto& [it, cnt] : cond_freq)
            if (cnt >= min_supp_cnt)
                cond_order.push_back(it);

        if (cond_order.empty()) continue;

        sort(cond_order.begin(), cond_order.end(), [&](const string& a, const string& b) {
            return cond_freq.at(a) > cond_freq.at(b);
        });

        // Zbuduj conditional FP-Tree
        auto cond_tree = make_unique<FPTree>();
        cond_tree->item_order = cond_order;

        unordered_map<string, int> order_map;
        for (int i = 0; i < (int)cond_order.size(); i++)
            order_map[cond_order[i]] = i;

        for (auto& [path, cnt] : cond_patterns) {
            vector<string> filtered;
            for (const string& it : path)
                if (cond_freq.count(it) && cond_freq.at(it) >= min_supp_cnt)
                    filtered.push_back(it);

            sort(filtered.begin(), filtered.end(), [&](const string& a, const string& b) {
                return order_map.at(a) < order_map.at(b);
            });

            // Wstaw path cnt razy (lub raz z wagą – implementacja z wagą)
            if (filtered.empty()) continue;
            FPNode* cur = cond_tree->root;
            for (const string& it : filtered) {
                FPNode* child = nullptr;
                for (auto* c : cur->children)
                    if (c->item == it) { child = c; break; }
                if (!child) {
                    child = new FPNode(it, 0, cur);
                    cur->children.push_back(child);
                    if (cond_tree->header.count(it)) {
                        FPNode* tail = cond_tree->header[it];
                        while (tail->node_link) tail = tail->node_link;
                        tail->node_link = child;
                    } else {
                        cond_tree->header[it] = child;
                    }
                }
                child->count += cnt;
                cur = child;
            }
        }

        fpgrowth_recursive(*cond_tree, new_itemset, min_supp_cnt, total_n, min_conf, result);
    }
}

// ──────────────────────────────────────────────
// Parsowanie CSV + główna funkcja
// ──────────────────────────────────────────────

MiningResult solve_fpgrowth(string path, double min_supp, double min_conf) {
    supp_cache.clear();
    MiningResult result;

    unordered_map<string, vector<int>> inv_to_items_str;
    vector<string> inv_order;  // kolejność transakcji (dla spójności)

    ifstream file(path);
    string line;
    getline(file, line);  // nagłówek

    // Wstępna obróbka CSV (identyczna logika jak w dEclat)
    while (getline(file, line)) {
        if (line.empty()) continue;

        size_t c1 = line.find(',');
        if (c1 == string::npos) continue;

        string inv = line.substr(0, c1);
        size_t c2 = line.find(',', c1 + 1);
        string stock = line.substr(c1 + 1, c2 - c1 - 1);

        if (!inv.empty() && isdigit(inv[0]) && !stock.empty()) {
            if (!inv_to_items_str.count(inv))
                inv_order.push_back(inv);
            inv_to_items_str[inv];  // inicjuj jeśli nie istnieje
            inv_to_items_str[inv].push_back(0);  // placeholder
            // Przechowujemy stock jako string bezpośrednio
            // (poniżej przebudujemy jako vector<string>)
        }
    }

    // ── Drugie podejście: przechowuj stringi bezpośrednio ──
    // Przebuduj mapę transakcji jako vector<string>
    file.clear();
    file.seekg(0);
    getline(file, line);  // nagłówek

    unordered_map<string, vector<string>> trans_map;

    while (getline(file, line)) {
        if (line.empty()) continue;
        size_t c1 = line.find(',');
        if (c1 == string::npos) continue;
        string inv = line.substr(0, c1);
        size_t c2 = line.find(',', c1 + 1);
        string stock = line.substr(c1 + 1, c2 - c1 - 1);
        if (!inv.empty() && isdigit(inv[0]) && !stock.empty())
            trans_map[inv].push_back(stock);
    }

    vector<vector<string>> transactions;
    for (auto& [inv, items] : trans_map) {
        sort(items.begin(), items.end());
        items.erase(unique(items.begin(), items.end()), items.end());
        transactions.push_back(items);
    }

    int n_trans = transactions.size();
    int min_supp_cnt = max(1, (int)(min_supp * n_trans));

    unordered_map<string, int> freq;
    for (const auto& trans : transactions)
        for (const string& it : trans)
            freq[it]++;

    vector<string> item_order;
    for (auto& [item, cnt] : freq) {
        if (cnt >= min_supp_cnt) {
            item_order.push_back(item);
            double s = (double)cnt / n_trans;
            supp_cache[{item}] = s;

            FrequentItemset fi;
            fi.items = {item};
            fi.supp = s;
            result.frequent_itemsets.push_back(fi);
        }
    }

    sort(item_order.begin(), item_order.end(), [&](const string& a, const string& b) {
        return freq.at(a) > freq.at(b);
    });

    if (item_order.empty()) return result;

    auto tree = build_fptree(transactions, freq, min_supp_cnt, item_order);

    fpgrowth_recursive(*tree, {}, min_supp_cnt, n_trans, min_conf, result);

    return result;
}

// ──────────────────────────────────────────────
// Pybind11
// ──────────────────────────────────────────────
PYBIND11_MODULE(fpgrowth_155198_execute, m) {
    py::class_<AssociationRule>(m, "AssociationRule", py::module_local())
        .def_readonly("A",    &AssociationRule::A)
        .def_readonly("B",    &AssociationRule::B)
        .def_readonly("supp", &AssociationRule::supp)
        .def_readonly("conf", &AssociationRule::conf);

    py::class_<FrequentItemset>(m, "FrequentItemset", py::module_local())
        .def_readonly("items", &FrequentItemset::items)
        .def_readonly("supp",  &FrequentItemset::supp);

    py::class_<MiningResult>(m, "MiningResult", py::module_local())
        .def_readonly("frequent_itemsets", &MiningResult::frequent_itemsets)
        .def_readonly("rules",             &MiningResult::rules);

    m.def("solve", &solve_fpgrowth,
          py::arg("path"),
          py::arg("min_supp"),
          py::arg("min_conf"),
          "FP-Growth: znajdź częste zbiory i reguły asocjacyjne z pliku CSV.");
}