#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iomanip>
#include <cctype>

using namespace std;

const char KEY_SEPARATOR = '\x1F';

struct ItemBits {
    vector<string> items;
    vector<unsigned long long> bits;
    int support;
};

struct Rule {
    vector<string> A;
    vector<string> B;
    double support;
    double confidence;
};


bool is_digits(const string& s) {
    if (s.empty()) return false;

    for (char c : s) {
        if (!isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}


string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");

    if (start == string::npos) return "";

    return s.substr(start, end - start + 1);
}


vector<string> split_csv_simple(const string& line) {
    vector<string> result;
    stringstream ss(line);
    string part;

    while (getline(ss, part, ',')) {
        result.push_back(trim(part));
    }

    return result;
}


int popcount_ull(unsigned long long x) {
#if defined(__GNUG__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    int count = 0;
    while (x) {
        x &= (x - 1);
        count++;
    }
    return count;
#endif
}


int count_bits(const vector<unsigned long long>& bits) {
    int result = 0;

    for (unsigned long long x : bits) {
        result += popcount_ull(x);
    }

    return result;
}


vector<unsigned long long> intersect_bits(
    const vector<unsigned long long>& a,
    const vector<unsigned long long>& b
) {
    vector<unsigned long long> result(a.size());

    for (size_t i = 0; i < a.size(); i++) {
        result[i] = a[i] & b[i];
    }

    return result;
}


int intersect_bits_with_count(
    const vector<unsigned long long>& a,
    const vector<unsigned long long>& b,
    vector<unsigned long long>& result
) {
    int support = 0;

    for (size_t i = 0; i < a.size(); i++) {
        result[i] = a[i] & b[i];
        support += popcount_ull(result[i]);
    }

    return support;
}


string make_key(vector<string> items) {
    sort(items.begin(), items.end());

    string key;

    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) key += KEY_SEPARATOR;
        key += items[i];
    }

    return key;
}


vector<string> key_to_items(const string& key) {
    vector<string> items;
    string current;

    for (char c : key) {
        if (c == KEY_SEPARATOR) {
            items.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        items.push_back(current);
    }

    return items;
}


string join_items(const vector<string>& items, const string& sep) {
    string result;

    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) result += sep;
        result += items[i];
    }

    return result;
}


vector<unordered_set<string>> load_transactions(const string& datapath) {
    vector<unordered_set<string>> transactions;
    unordered_map<string, int> tid_to_index;

    ifstream file(datapath);

    if (!file.is_open()) {
        cerr << "Nie można otworzyć pliku: " << datapath << endl;
        return transactions;
    }

    string line;

    getline(file, line); // nagłówek

    while (getline(file, line)) {
        vector<string> parts = split_csv_simple(line);

        if (parts.size() >= 2 && is_digits(parts[0])) {
            string tid = parts[0];
            string item = parts[1];

            if (item.empty()) continue;

            if (tid_to_index.find(tid) == tid_to_index.end()) {
                int new_index = static_cast<int>(transactions.size());
                tid_to_index[tid] = new_index;
                transactions.push_back(unordered_set<string>());
            }

            int index = tid_to_index[tid];
            transactions[index].insert(item);
        }
    }

    return transactions;
}


void eclat_recursive(
    const vector<string>& prefix,
    const vector<ItemBits>& items,
    int min_supp_count,
    unordered_map<string, int>& frequent_itemsets
) {
    for (size_t i = 0; i < items.size(); i++) {
        vector<string> new_prefix = prefix;

        for (const string& item : items[i].items) {
            new_prefix.push_back(item);
        }

        sort(new_prefix.begin(), new_prefix.end());

        frequent_itemsets[make_key(new_prefix)] = items[i].support;

        vector<ItemBits> suffix_items;

        for (size_t j = i + 1; j < items.size(); j++) {
            vector<unsigned long long> intersection(items[i].bits.size());

            int support = intersect_bits_with_count(
                items[i].bits,
                items[j].bits,
                intersection
            );

            if (support >= min_supp_count) {
                ItemBits new_item;
                new_item.items = items[j].items;
                new_item.bits = move(intersection);
                new_item.support = support;

                suffix_items.push_back(move(new_item));
            }
        }

        if (!suffix_items.empty()) {
            eclat_recursive(
                new_prefix,
                suffix_items,
                min_supp_count,
                frequent_itemsets
            );
        }
    }
}


void generate_subsets_recursive(
    const vector<string>& items,
    int index,
    vector<string>& current,
    vector<vector<string>>& subsets
) {
    if (index == static_cast<int>(items.size())) {
        if (!current.empty() && current.size() < items.size()) {
            subsets.push_back(current);
        }
        return;
    }

    generate_subsets_recursive(items, index + 1, current, subsets);

    current.push_back(items[index]);
    generate_subsets_recursive(items, index + 1, current, subsets);
    current.pop_back();
}


vector<string> difference_items(
    const vector<string>& itemset,
    const vector<string>& subset
) {
    unordered_set<string> subset_set(subset.begin(), subset.end());
    vector<string> result;

    for (const string& item : itemset) {
        if (subset_set.find(item) == subset_set.end()) {
            result.push_back(item);
        }
    }

    return result;
}


vector<Rule> generate_rules(
    const unordered_map<string, int>& frequent_itemsets,
    int n_trans,
    double min_confidence
) {
    vector<Rule> rules;

    for (const auto& pair : frequent_itemsets) {
        vector<string> itemset = key_to_items(pair.first);
        int itemset_support_count = pair.second;

        if (itemset.size() < 2) {
            continue;
        }

        vector<vector<string>> subsets;
        vector<string> current;

        generate_subsets_recursive(itemset, 0, current, subsets);

        for (vector<string>& antecedent : subsets) {
            sort(antecedent.begin(), antecedent.end());

            vector<string> consequent = difference_items(itemset, antecedent);
            sort(consequent.begin(), consequent.end());

            string antecedent_key = make_key(antecedent);

            auto it = frequent_itemsets.find(antecedent_key);

            if (it == frequent_itemsets.end()) {
                continue;
            }

            int antecedent_support_count = it->second;

            double confidence =
                static_cast<double>(itemset_support_count) /
                static_cast<double>(antecedent_support_count);

            if (confidence >= min_confidence) {
                double support =
                    static_cast<double>(itemset_support_count) /
                    static_cast<double>(n_trans);

                Rule rule;
                rule.A = antecedent;
                rule.B = consequent;
                rule.support = support;
                rule.confidence = confidence;

                rules.push_back(move(rule));
            }
        }
    }

    return rules;
}


int main(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Użycie: eclat_fast.exe datapath min_support min_confidence verbose" << endl;
        return 1;
    }

    string datapath = argv[1];
    double min_support = stod(argv[2]);
    double min_confidence = stod(argv[3]);
    bool verbose = stoi(argv[4]) != 0;

    vector<unordered_set<string>> transactions = load_transactions(datapath);

    int n_trans = static_cast<int>(transactions.size());

    if (n_trans == 0) {
        cerr << "Brak transakcji." << endl;
        return 1;
    }

    int min_supp_count = static_cast<int>(min_support * n_trans);

    if (min_supp_count < 1) {
        min_supp_count = 1;
    }

    int words_count = (n_trans + 63) / 64;

    unordered_map<string, vector<unsigned long long>> vertical_data;

    for (int tid = 0; tid < n_trans; tid++) {
        int word_index = tid / 64;
        int bit_index = tid % 64;

        unsigned long long mask = 1ULL << bit_index;

        for (const string& item : transactions[tid]) {
            if (vertical_data.find(item) == vertical_data.end()) {
                vertical_data[item] = vector<unsigned long long>(words_count, 0ULL);
            }

            vertical_data[item][word_index] |= mask;
        }
    }

    vector<ItemBits> initial_items;

    for (auto& pair : vertical_data) {
        int support = count_bits(pair.second);

        if (support >= min_supp_count) {
            ItemBits item_bits;
            item_bits.items.push_back(pair.first);
            item_bits.bits = move(pair.second);
            item_bits.support = support;

            initial_items.push_back(move(item_bits));
        }
    }

    sort(
        initial_items.begin(),
        initial_items.end(),
        [](const ItemBits& a, const ItemBits& b) {
            if (a.support != b.support) {
                return a.support < b.support;
            }

            return a.items[0] < b.items[0];
        }
    );

    unordered_map<string, int> frequent_itemsets;

    vector<string> empty_prefix;

    eclat_recursive(
        empty_prefix,
        initial_items,
        min_supp_count,
        frequent_itemsets
    );

    vector<Rule> rules = generate_rules(
        frequent_itemsets,
        n_trans,
        min_confidence
    );

    if (verbose) {
        cerr << "Liczba transakcji: " << n_trans << endl;
        cerr << "Minimalny support: " << min_support << endl;
        cerr << "Minimalny support count: " << min_supp_count << endl;
        cerr << "Liczba częstych zbiorów: " << frequent_itemsets.size() << endl;
        cerr << "Liczba reguł: " << rules.size() << endl;
    }

    cout << fixed << setprecision(10);

    for (const Rule& rule : rules) {
        cout << "RULE;"
             << join_items(rule.A, "|") << ";"
             << join_items(rule.B, "|") << ";"
             << rule.support << ";"
             << rule.confidence << endl;
    }

    return 0;
}