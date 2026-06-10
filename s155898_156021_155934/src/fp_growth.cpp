#include "fp_growth.hpp"
#include <algorithm>
#include <iostream>
#include <omp.h>
#include <sstream>

namespace {
void print_json_string(std::ostream &out, const std::string &value) {
    out << '"';
    for (char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    out << '"';
}

void print_json_array(std::ostream &out, const Itemset &items, const std::vector<std::string> &item_names) {
    out << '[';
    bool first = true;
    for (Item item : items) {
        if (!first) out << ',';
        print_json_string(out, item_names[static_cast<size_t>(item)]);
        first = false;
    }
    out << ']';
}

Itemset with_item(Itemset items, Item item) {
    auto pos = std::lower_bound(items.begin(), items.end(), item);
    if (pos == items.end() || *pos != item) {
        items.insert(pos, item);
    }
    return items;
}

} // namespace

void FPGrowth::count_items(std::vector<int> &item_counts) {
    // Każdy wątek zlicza do własnego vectora, więc nie ma atomików w pętli po transakcjach
    int thread_count = omp_get_max_threads();
    std::vector<std::vector<int>> local_counts(
        static_cast<size_t>(thread_count),
        std::vector<int>(item_counts.size(), 0)
    );

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        auto &local = local_counts[static_cast<size_t>(thread_id)];

        #pragma omp for schedule(static)
        for (long long i = 0; i < static_cast<long long>(transactions.size()); ++i) {
            for (Item item : transactions[static_cast<size_t>(i)]) {
                ++local[static_cast<size_t>(item)];
            }
        }
    }

    for (const auto &local : local_counts) {
        for (size_t i = 0; i < local.size(); ++i) {
            item_counts[i] += local[i];
        }
    }
}

void FPGrowth::build_tree(const Transaction &sorted_items,
                          const std::vector<int> &item_counts) {
    header_table.clear();
    header_table.reserve(item_counts.size());

    for (Item item = 0; item < static_cast<Item>(item_counts.size()); ++item) {
        int count = item_counts[static_cast<size_t>(item)];
        if (count >= min_support_count) {
            HeaderEntry entry;
            entry.count = count;
            header_table.emplace(item, entry);
        }
    }

    // rank[item] określa kolejność w transakcji po odfiltrowaniu itemów nieczęstych
    std::vector<int> rank(item_counts.size(), -1);
    int current_rank = 0;
    for (Item item : sorted_items) {
        if (header_table.find(item) != header_table.end()) {
            rank[static_cast<size_t>(item)] = current_rank++;
        }
    }

    for (const auto &trans : transactions) {
        Transaction filtered;
        filtered.reserve(trans.size());

        for (Item item : trans) {
            if (rank[static_cast<size_t>(item)] >= 0) {
                filtered.push_back(item);
            }
        }

        std::sort(filtered.begin(), filtered.end(), [&](const auto &a, const auto &b) {
            return rank[static_cast<size_t>(a)] < rank[static_cast<size_t>(b)];
        });

        if (!filtered.empty()) {
            insert_tree(filtered, root, header_table);
        }
    }
}

void FPGrowth::insert_tree(const Transaction &items, NodePointer node,
                           HeaderTable &table, size_t idx) {
    // Iteracyjnie zamiast rekurencji: mniej narzutu dla długich transakcji
    for (size_t pos = idx; pos < items.size(); ++pos) {
        Item item = items[pos];
        NodePointer child;

        auto child_it = node->children.find(item);
        if (child_it != node->children.end()) {
            child = child_it->second;
            child->count += 1;
        } else {
            child = std::make_shared<Node>(item, 1, node);
            node->children.emplace(item, child);

            auto &entry = table[item];
            if (!entry.head) {
                entry.head = child;
                entry.tail = child;
            } else {
                entry.tail->next_link = child;
                entry.tail = child;
            }
        }

        node = child;
    }
}

void FPGrowth::mine_tree(HeaderTable &table, Itemset prefix) {
    for (auto &[item, entry] : table) {
        Itemset new_prefix = with_item(prefix, item);

        frequent_itemsets[new_prefix] = entry.count;

        // Baza wzorców warunkowych: ścieżki od wystąpień itemu do korzenia
        std::vector<std::pair<Transaction, int>> cond_patterns;

        auto node = entry.head;
        while (node) {
            Transaction path;
            auto p = node->parent.lock();

            while (p && p->item != -1) {
                path.push_back(p->item);
                p = p->parent.lock();
            }

            if (!path.empty()) {
                cond_patterns.emplace_back(std::move(path), node->count);
            }

            node = node->next_link.lock();
        }

        // Itemy są gęstymi intami, więc vector jest szybszy niż unordered_map
        std::vector<int> cond_counts(item_names.size(), 0);
        std::vector<Item> touched_items;
        for (auto &[path, c] : cond_patterns) {
            for (auto &x : path) {
                size_t idx = static_cast<size_t>(x);
                if (cond_counts[idx] == 0) {
                    touched_items.push_back(x);
                }
                cond_counts[idx] += c;
            }
        }

        HeaderTable cond_table;
        cond_table.reserve(touched_items.size());
        for (Item k : touched_items) {
            int v = cond_counts[static_cast<size_t>(k)];
            if (v >= min_support_count) {
                HeaderEntry e;
                e.count = v;
                cond_table.emplace(k, e);
            }
        }

        if (!cond_table.empty()) {
            mine_tree(cond_table, new_prefix);
        }
    }
}

void FPGrowth::generate_rules() {
    // Przeniesienie do vectora daje OpenMP dostęp indeksowy bez chodzenia po unordered_map
    std::vector<std::pair<Itemset, int>> itemsets;
    itemsets.reserve(frequent_itemsets.size());
    for (const auto &[itemset, count] : frequent_itemsets) {
        if (itemset.size() >= 2) {
            itemsets.emplace_back(itemset, count);
        }
    }

    #pragma omp parallel
    {
        // Każdy wątek buforuje swój JSON, a stdout jest użyty tylko raz na wątek
        std::ostringstream local_output;

        #pragma omp for schedule(dynamic)
        for (long long idx = 0; idx < static_cast<long long>(itemsets.size()); ++idx) {
            const auto &[itemset, count] = itemsets[static_cast<size_t>(idx)];
            const Itemset &items = itemset;
            int n = static_cast<int>(items.size());
            if (n >= 31) continue; // zabezpieczenie przed overflow maski int

            for (int mask = 1; mask < (1 << n) - 1; ++mask) {
                Itemset A, B;
                A.reserve(static_cast<size_t>(n));
                B.reserve(static_cast<size_t>(n));

                for (int i = 0; i < n; ++i) {
                    if (mask & (1 << i)) A.push_back(items[i]);
                    else B.push_back(items[i]);
                }

                auto antecedent_it = frequent_itemsets.find(A);
                if (antecedent_it == frequent_itemsets.end()) continue;

                double conf = static_cast<double>(count) / antecedent_it->second;
                double supp = static_cast<double>(count) / transactions.size();

                if (conf >= min_confidence) {
                    local_output << "{\"A\":";
                    print_json_array(local_output, A, item_names);
                    local_output << ",\"B\":";
                    print_json_array(local_output, B, item_names);
                    local_output << ",\"supp\":" << supp;
                    local_output << ",\"conf\":" << conf;
                    local_output << "}\n";
                }
            }
        }

        #pragma omp critical
        {
            std::cout << local_output.str();
        }
    }
}

void FPGrowth::solve() {
    std::vector<int> item_counts(item_names.size(), 0);
    count_items(item_counts);

    // Globalna kolejność malejąco po częstości jest podstawą kompaktowego FP-tree
    Transaction sorted_items;
    sorted_items.reserve(item_counts.size());
    for (Item item = 0; item < static_cast<Item>(item_counts.size()); ++item) {
        sorted_items.push_back(item);
    }

    std::sort(sorted_items.begin(), sorted_items.end(), [&](const auto &a, const auto &b) {
        return item_counts[a] > item_counts[b];
    });

    build_tree(sorted_items, item_counts);

    mine_tree(header_table, {});

    generate_rules();
}
