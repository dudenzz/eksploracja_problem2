#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <omp.h>
#include <iterator>
#include <functional>
#include <mutex>

using namespace std;

void calculateTime(
    std::chrono::high_resolution_clock::time_point start,
    std::chrono::high_resolution_clock::time_point end,
    std::string message = "Czas"
) {
    std::chrono::duration<double> duration = end - start;
    std::cerr << message << ": " << duration.count() << " s" << std::endl;
}

struct FPNode {
    string item;
    int count;
    FPNode* parent;
    unordered_map<string, FPNode*> children;
    FPNode* next_link;

    FPNode(const string& item, int count, FPNode* parent)
        : item(item), count(count), parent(parent), next_link(nullptr) {}
};

struct HeaderEntry {
    int count;
    FPNode* first;
};

struct Rule {
    vector<string> A;
    vector<string> B;
    double supp;
    double conf;
};

using HeaderTable = unordered_map<string, HeaderEntry>;
using Itemset = set<string>;
using FrequentItemsets = map<Itemset, int>;

vector<string> split(const string& line, char delimiter) {
    vector<string> parts;
    string part;
    stringstream ss(line);

    while (getline(ss, part, delimiter)) {
        parts.push_back(part);
    }

    return parts;
}

bool is_number(const string& s) {
    if (s.empty()) return false;

    for (char c : s) {
        if (!isdigit(c)) return false;
    }

    return true;
}

void insert_tree(const vector<string>& items, int index, FPNode* node, HeaderTable& header_table) {
    if (index >= static_cast<int>(items.size())) {
        return;
    }

    const string& current_item = items[index];

    if (node->children.find(current_item) != node->children.end()) {
        node->children[current_item]->count += 1;
    } else {
        FPNode* new_node = new FPNode(current_item, 1, node);
        node->children[current_item] = new_node;

        if (header_table[current_item].first == nullptr) {
            header_table[current_item].first = new_node;
        } else {
            FPNode* current = header_table[current_item].first;
            while (current->next_link != nullptr) {
                current = current->next_link;
            }
            current->next_link = new_node;
        }
    }

    insert_tree(items, index + 1, node->children[current_item], header_table);
}

namespace frequent_itemsets_library {

    void mine_tree(
        HeaderTable& header_table,
        double min_supp_count,
        Itemset prefix,
        FrequentItemsets& frequent_itemsets
    ) {
        vector<pair<string, HeaderEntry>> sorted_items;

        for (const auto& entry : header_table) {
            sorted_items.push_back(entry);
        }

        sort(sorted_items.begin(), sorted_items.end(),
            [](const auto& a, const auto& b) {
                return a.second.count < b.second.count;
            });

        for (const auto& entry : sorted_items) {
            string item = entry.first;

            Itemset new_frequent_set = prefix;
            new_frequent_set.insert(item);

            frequent_itemsets[new_frequent_set] = header_table[item].count;

            vector<pair<vector<string>, int>> cond_patterns;

            FPNode* node = header_table[item].first;

            while (node != nullptr) {
                vector<string> prefix_path;

                FPNode* parent = node->parent;
                while (parent != nullptr && !parent->item.empty()) {
                    prefix_path.push_back(parent->item);
                    parent = parent->parent;
                }

                if (!prefix_path.empty()) {
                    cond_patterns.push_back({prefix_path, node->count});
                }

                node = node->next_link;
            }

            unordered_map<string, int> cond_header_counts;

            for (const auto& pattern : cond_patterns) {
                const vector<string>& path = pattern.first;
                int count = pattern.second;

                for (const string& p_item : path) {
                    cond_header_counts[p_item] += count;
                }
            }

            HeaderTable cond_header;

            for (const auto& ch : cond_header_counts) {
                if (ch.second >= min_supp_count) {
                    cond_header[ch.first] = {ch.second, nullptr};
                }
            }

            if (!cond_header.empty()) {
                FPNode* cond_root = new FPNode("", 0, nullptr);

                for (const auto& pattern : cond_patterns) {
                    const vector<string>& path = pattern.first;
                    int count = pattern.second;

                    vector<string> filtered_path;

                    for (const string& p_item : path) {
                        if (cond_header.find(p_item) != cond_header.end()) {
                            filtered_path.push_back(p_item);
                        }
                    }

                    if (!filtered_path.empty()) {
                        FPNode* curr = cond_root;

                        for (auto it = filtered_path.rbegin(); it != filtered_path.rend(); ++it) {
                            const string& p_item = *it;

                            if (curr->children.find(p_item) != curr->children.end()) {
                                curr->children[p_item]->count += count;
                            } else {
                                FPNode* new_node = new FPNode(p_item, count, curr);
                                curr->children[p_item] = new_node;

                                if (cond_header[p_item].first == nullptr) {
                                    cond_header[p_item].first = new_node;
                                } else {
                                    FPNode* tmp = cond_header[p_item].first;
                                    while (tmp->next_link != nullptr) {
                                        tmp = tmp->next_link;
                                    }
                                    tmp->next_link = new_node;
                                }
                            }

                            curr = curr->children[p_item];
                        }
                    }
                }

                mine_tree(cond_header, min_supp_count, new_frequent_set, frequent_itemsets);
            }
        }
    }

    FrequentItemsets generate(HeaderTable& header_table, double min_supp_count) {
        FrequentItemsets frequent_itemsets;
        Itemset empty_prefix;

        mine_tree(header_table, min_supp_count, empty_prefix, frequent_itemsets);

        return frequent_itemsets;
    }

    FrequentItemsets generate(
    const vector<vector<string>>& transactions,
    double min_supp_count
    ) {
        using TidList = vector<int>;

        int thread_count = omp_get_max_threads();
        vector<unordered_map<string, TidList>> local_indexes(thread_count);

        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            auto& local_index = local_indexes[thread_id];

            #pragma omp for schedule(static)
            for (int tid = 0; tid < static_cast<int>(transactions.size()); ++tid) {
                unordered_set<string> seen;
                seen.reserve(transactions[tid].size());

                for (const string& item : transactions[tid]) {
                    if (seen.insert(item).second) {
                        local_index[item].push_back(tid);
                    }
                }
            }
        }

        unordered_map<string, TidList> vertical_index;
        vertical_index.reserve(4096);

        for (auto& local_index : local_indexes) {
            for (auto& [item, tids] : local_index) {
                auto& dest = vertical_index[item];

                dest.insert(
                    dest.end(),
                    tids.begin(),
                    tids.end()
                );
            }
        }

        for (auto& [item, tids] : vertical_index) {
            sort(tids.begin(), tids.end());
        }

        vector<pair<string, TidList>> items;
        items.reserve(vertical_index.size());

        for (auto& entry : vertical_index) {
            if (static_cast<double>(entry.second.size()) >= min_supp_count) {
                items.push_back({entry.first, std::move(entry.second)});
            }
        }

        sort(items.begin(), items.end(),
            [](const auto& a, const auto& b) {
                if (a.second.size() != b.second.size()) {
                    return a.second.size() < b.second.size();
                }
                return a.first < b.first;
            });

        auto intersect_tids = [](const TidList& left, const TidList& right) {
            TidList result;
            result.reserve(min(left.size(), right.size()));

            size_t i = 0;
            size_t j = 0;

            while (i < left.size() && j < right.size()) {
                if (left[i] == right[j]) {
                    result.push_back(left[i]);
                    ++i;
                    ++j;
                } else if (left[i] < right[j]) {
                    ++i;
                } else {
                    ++j;
                }
            }

            return result;
        };

        FrequentItemsets frequent_itemsets;

        int n = static_cast<int>(items.size());

        vector<FrequentItemsets> local_results(omp_get_max_threads());

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            FrequentItemsets& local_itemsets = local_results[tid];

            function<void(Itemset, vector<pair<string, TidList>>)> extend =
                [&](Itemset prefix, vector<pair<string, TidList>> suffix) {
                    for (size_t i = 0; i < suffix.size(); ++i) {
                        Itemset next_prefix = prefix;
                        next_prefix.insert(suffix[i].first);

                        local_itemsets[next_prefix] =
                            static_cast<int>(suffix[i].second.size());

                        vector<pair<string, TidList>> next_suffix;

                        for (size_t j = i + 1; j < suffix.size(); ++j) {
                            TidList intersection =
                                intersect_tids(suffix[i].second, suffix[j].second);

                            if (static_cast<double>(intersection.size()) >= min_supp_count) {
                                next_suffix.push_back({
                                    suffix[j].first,
                                    std::move(intersection)
                                });
                            }
                        }

                        if (!next_suffix.empty()) {
                            extend(next_prefix, std::move(next_suffix));
                        }
                    }
                };

            #pragma omp for schedule(dynamic)
            for (int i = 0; i < n; ++i) {
                Itemset prefix;
                prefix.insert(items[i].first);

                local_itemsets[prefix] =
                    static_cast<int>(items[i].second.size());

                vector<pair<string, TidList>> suffix;

                for (int j = i + 1; j < n; ++j) {
                    TidList intersection =
                        intersect_tids(items[i].second, items[j].second);

                    if (static_cast<double>(intersection.size()) >= min_supp_count) {
                        suffix.push_back({
                            items[j].first,
                            std::move(intersection)
                        });
                    }
                }

                if (!suffix.empty()) {
                    extend(prefix, std::move(suffix));
                }
            }
        }

        for (auto& local : local_results) {
            for (auto& entry : local) {
                frequent_itemsets.insert(std::move(entry));
            }
        }

        return frequent_itemsets;
    }

}  // namespace frequent_itemsets_library

void generate_combinations_recursive(
    const vector<string>& items,
    int start,
    int target_size,
    vector<string>& current,
    vector<vector<string>>& result
) {
    if (static_cast<int>(current.size()) == target_size) {
        result.push_back(current);
        return;
    }

    for (int i = start; i < static_cast<int>(items.size()); i++) {
        current.push_back(items[i]);
        generate_combinations_recursive(items, i + 1, target_size, current, result);
        current.pop_back();
    }
}

vector<vector<string>> combinations(const vector<string>& items, int size) {
    vector<vector<string>> result;
    vector<string> current;

    generate_combinations_recursive(items, 0, size, current, result);

    return result;
}

vector<Rule> solve(
    const string& datapath,
    double min_support,
    double min_confidence,
    bool verbose = false
) {
    vector<vector<string>> raw_transactions;

    try {
        ifstream file(datapath, ios::binary);

        if (!file.is_open()) {
            throw runtime_error("Nie mozna otworzyc pliku.");
        }

        // 1. Wczytanie całego pliku do jednego bufora
        // auto startReadingCSV = std::chrono::high_resolution_clock::now();

        file.seekg(0, ios::end);
        size_t file_size = static_cast<size_t>(file.tellg());
        file.seekg(0, ios::beg);

        string buffer(file_size, '\0');
        file.read(buffer.data(), file_size);

        // auto endReadingCSV = std::chrono::high_resolution_clock::now();
        // calculateTime(startReadingCSV, endReadingCSV, "Czas wczytywania danych");

        size_t data_start = buffer.find('\n');
        if (data_start == string::npos) {
            raw_transactions.clear();
            return {};
        }
        data_start++;
        // Podział bufora na linie i przetwarzanie ich równolegle
        // auto start = std::chrono::high_resolution_clock::now();

        int thread_count = omp_get_max_threads();
        vector<unordered_map<int, vector<string>>> local_maps(thread_count);

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            auto& local_map = local_maps[tid];
            local_map.reserve(10000);

            size_t chunk_size = (buffer.size() - data_start) / thread_count;

            size_t begin = data_start + tid * chunk_size;
            size_t end = (tid == thread_count - 1)
                ? buffer.size()
                : data_start + (tid + 1) * chunk_size;

            if (tid != 0) {
                while (begin < buffer.size() && buffer[begin - 1] != '\n') {
                    begin++;
                }
            }

            if (tid != thread_count - 1) {
                while (end < buffer.size() && buffer[end] != '\n') {
                    end++;
                }
            }

            size_t pos = begin;

            while (pos < end) {
                size_t line_end = buffer.find('\n', pos);
                if (line_end == string::npos || line_end > end) {
                    line_end = end;
                }

                size_t comma1 = buffer.find(',', pos);
                if (comma1 == string::npos || comma1 >= line_end) {
                    pos = line_end + 1;
                    continue;
                }

                size_t comma2 = buffer.find(',', comma1 + 1);
                if (comma2 == string::npos || comma2 >= line_end) {
                    pos = line_end + 1;
                    continue;
                }

                int invoice = 0;
                bool valid_invoice = comma1 > pos;

                for (size_t i = pos; i < comma1; ++i) {
                    unsigned char c = static_cast<unsigned char>(buffer[i]);

                    if (!isdigit(c)) {
                        valid_invoice = false;
                        break;
                    }

                    invoice = invoice * 10 + (buffer[i] - '0');
                }

                if (!valid_invoice) {
                    pos = line_end + 1;
                    continue;
                }

                size_t stock_start = comma1 + 1;
                size_t stock_len = comma2 - stock_start;

                if (stock_len == 0) {
                    pos = line_end + 1;
                    continue;
                }

                string stock_code = buffer.substr(stock_start, stock_len);
                local_map[invoice].push_back(std::move(stock_code));

                pos = line_end + 1;
            }
        }

        unordered_map<int, vector<string>> global_map;
        global_map.reserve(50000);

        for (auto& local_map : local_maps) {
            for (auto& [invoice, items] : local_map) {
                auto& dest = global_map[invoice];

                dest.insert(
                    dest.end(),
                    make_move_iterator(items.begin()),
                    make_move_iterator(items.end())
                );
            }
        }

        raw_transactions.clear();
        raw_transactions.reserve(global_map.size());

        for (auto& [invoice, items] : global_map) {
            sort(items.begin(), items.end());
            items.erase(unique(items.begin(), items.end()), items.end());

            raw_transactions.push_back(std::move(items));
        }

        // auto end = std::chrono::high_resolution_clock::now();
        // calculateTime(start, end, "Czas przetwarzania danych i budowania struktury transakcji");  

    } catch (...) {
        raw_transactions.clear();
    }

    int n_trans = static_cast<int>(raw_transactions.size());
    double min_supp_count = min_support * n_trans;

    
    // Pomiar czasu liczenia wsparcia pojedynczych elementow
    unordered_map<string, int> item_counts;
    // {
    // auto start = std::chrono::high_resolution_clock::now();
    for (const auto& trans : raw_transactions) {
        for (const string& item : trans) {
            item_counts[item] += 1;
        }
    }
    // auto end = std::chrono::high_resolution_clock::now();
    // calculateTime(start, end, "Czas liczenia wsparcia pojedynczych elementow");
    // }
    
    // Pomiar czasu filtrowania elementow niespelniajacych wsparcia
    unordered_map<string, int> frequent_items;
    // {
    // auto start = std::chrono::high_resolution_clock::now();
    for (const auto& entry : item_counts) {
        if (entry.second >= min_supp_count) {
            frequent_items[entry.first] = entry.second;
        }
    }
    // auto end = std::chrono::high_resolution_clock::now();
    // calculateTime(start, end, "Czas filtrowania elementow niespelniajacych wsparcia");
    // }

    vector<string> sorted_items;

    for (const auto& entry : frequent_items) {
        sorted_items.push_back(entry.first);
    }

    // Pomiar czasu sortowania elementow wg wsparcia
    // {
    // auto start = std::chrono::high_resolution_clock::now();
    sort(sorted_items.begin(), sorted_items.end(),
         [&frequent_items](const string& a, const string& b) {
             return frequent_items[a] > frequent_items[b];
         });
    // auto end = std::chrono::high_resolution_clock::now();
    // calculateTime(start, end, "Czas sortowania elementow wedlug wsparcia");
    // }

    FrequentItemsets frequent_itemsets;
    // Pomiar czasu generowania częstych itemsetów przez moduł biblioteczny
    // {
    // auto start = std::chrono::high_resolution_clock::now();
    frequent_itemsets = frequent_itemsets_library::generate(raw_transactions, min_supp_count);
    // auto end = std::chrono::high_resolution_clock::now();
    // calculateTime(start, end, "Czas generowania czestych itemsetow przez modul biblioteczny");
    // }

    vector<Rule> rules;

    // Pomiar czasu generowania reguł asocjacyjnych z częstych itemsetów
    // {
    // auto start = std::chrono::high_resolution_clock::now();
    for (const auto& entry : frequent_itemsets) {
        const Itemset& itemset = entry.first;
        int count = entry.second;

        if (itemset.size() > 1) {
            double support = static_cast<double>(count) / n_trans;

            vector<string> itemset_vec(itemset.begin(), itemset.end());

            for (int i = 1; i < static_cast<int>(itemset_vec.size()); i++) {
                vector<vector<string>> antecedents = combinations(itemset_vec, i);

                for (const auto& antecedent_vec : antecedents) {
                    Itemset antecedent(antecedent_vec.begin(), antecedent_vec.end());

                    Itemset consequent;

                    for (const string& item : itemset) {
                        if (antecedent.find(item) == antecedent.end()) {
                            consequent.insert(item);
                        }
                    }

                    auto found = frequent_itemsets.find(antecedent);

                    if (found != frequent_itemsets.end()) {
                        int supp_a = found->second;
                        double confidence = static_cast<double>(count) / supp_a;

                        if (confidence >= min_confidence) {
                            Rule rule;

                            rule.A = vector<string>(antecedent.begin(), antecedent.end());
                            rule.B = vector<string>(consequent.begin(), consequent.end());
                            rule.supp = support;
                            rule.conf = confidence;

                            rules.push_back(rule);
                        }
                    }
                }
            }
        }
    }
    // auto end = std::chrono::high_resolution_clock::now();
    // calculateTime(start, end, "Czas generowania regul asocjacyjnych z czestych itemsetow");
    // }



    return rules;
}

string join_items(const vector<string>& items) {
    string result;

    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            result += '\x1f';
        }

        result += items[i];
    }

    return result;
}

int main(int argc, char* argv[]) {
    string datapath = "C:\\Users\\pawma\\.cache\\kagglehub\\datasets\\mashlyn\\online-retail-ii-uci\\versions\\3\\online_retail_II.csv";
    double min_support = 0.4;
    double min_confidence = 0.7;
    bool verbose = false;

    if (argc >= 2) {
        datapath = argv[1];
    }

    if (argc >= 3) {
        min_support = atof(argv[2]);
    }

    if (argc >= 4) {
        min_confidence = atof(argv[3]);
    }

    if (argc >= 5) {
        string verbose_arg = argv[4];
        verbose = verbose_arg == "1" || verbose_arg == "true" || verbose_arg == "True";
    }

    vector<Rule> rules = solve(datapath, min_support, min_confidence, verbose);

    if (!verbose) {
        for (const Rule& rule : rules) {
            cout << "RULE\t"
                 << join_items(rule.A) << "\t"
                 << join_items(rule.B) << "\t"
                 << rule.supp << "\t"
                 << rule.conf << endl;
        }
    }

    return 0;
}
