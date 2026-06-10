#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>

#ifdef _MSC_VER
#  include <intrin.h>
#  pragma intrinsic(__popcnt64)
   static inline int popcnt64(uint64_t x) { return (int)__popcnt64(x); }
#else
   static inline int popcnt64(uint64_t x) { return __builtin_popcountll(x); }
#endif

namespace py = pybind11;

struct TidSet {
    std::vector<uint64_t> words;

    TidSet() = default;
    explicit TidSet(int n_words) : words(n_words, 0ULL) {}

    void set_bit(int idx) {
        words[idx >> 6] |= (1ULL << (idx & 63));
    }

    int popcount() const {
        int c = 0;
        for (uint64_t w : words) c += popcnt64(w);
        return c;
    }

    void intersect_into(const TidSet& other, TidSet& result) const {
        const int n = (int)words.size();
        for (int i = 0; i < n; i++)
            result.words[i] = words[i] & other.words[i];
    }
};

struct FreqItemset {
    std::vector<int> items;
    int count;
};

struct VecIntHash {
    size_t operator()(const std::vector<int>& v) const noexcept {
        size_t h = 14695981039346656037ULL;
        for (int x : v) {
            h ^= (size_t)(unsigned)x;
            h *= 1099511628211ULL;
        }
        return h;
    }
};

static void eclat(
    std::vector<int>& prefix,
    std::vector<std::pair<int, TidSet>>& items,
    int min_count,
    int n_words,
    std::vector<FreqItemset>& out
) {
    for (int i = 0, n = (int)items.size(); i < n; i++) {
        auto& [item_i, tids_i] = items[i];

        prefix.push_back(item_i);
        out.push_back({prefix, tids_i.popcount()});

        std::vector<std::pair<int, TidSet>> Q;
        Q.reserve(n - i - 1);

        for (int j = i + 1; j < n; j++) {
            auto& [item_j, tids_j] = items[j];
            TidSet inter(n_words);
            tids_i.intersect_into(tids_j, inter);
            int cnt = inter.popcount();
            if (cnt >= min_count)
                Q.emplace_back(item_j, std::move(inter));
        }

        if (!Q.empty())
            eclat(prefix, Q, min_count, n_words, out);

        prefix.pop_back();
    }
}

py::list find_rules(
    const std::string& filepath,
    double min_support,
    double min_confidence,
    bool verbose
) {
    // Read CSV
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open: " + filepath);

    std::vector<char> io_buf(1 << 20);  // 1 MB read buffer
    file.rdbuf()->pubsetbuf(io_buf.data(), (std::streamsize)io_buf.size());

    std::unordered_map<std::string, std::vector<std::string>> trans_map;
    trans_map.reserve(65536);

    std::string line;
    bool first_line = true;

    while (std::getline(file, line)) {
        if (first_line) { first_line = false; continue; }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        const char* p     = line.c_str();
        const char* p_end = p + line.size();
        const char* c1    = (const char*)std::memchr(p, ',', p_end - p);
        if (!c1) continue;

        bool numeric = (c1 > p);
        for (const char* q = p; q < c1 && numeric; q++)
            if (*q < '0' || *q > '9') numeric = false;
        if (!numeric) continue;

        std::string invoice(p, c1);

        const char* c2         = (const char*)std::memchr(c1 + 1, ',', p_end - c1 - 1);
        const char* item_start = c1 + 1;
        const char* item_end   = c2 ? c2 : p_end;

        while (item_start < item_end && *item_start == ' ') item_start++;
        while (item_end > item_start && *(item_end - 1) == ' ') item_end--;

        if (item_start >= item_end) continue;

        trans_map[invoice].emplace_back(item_start, item_end);
    }
    file.close();

    // Flatten transactions and count items
    std::vector<std::vector<std::string>> transactions;
    transactions.reserve(trans_map.size());

    std::unordered_map<std::string, int> item_counts;
    item_counts.reserve(4096);

    for (auto& [inv, items] : trans_map) {
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        for (const auto& it : items) item_counts[it]++;
        transactions.push_back(std::move(items));
    }
    trans_map.clear();

    const int n_trans   = (int)transactions.size();
    const int min_count = std::max(1, (int)(min_support * n_trans));

    if (verbose)
        py::print("Transactions:", n_trans, "| min_count:", min_count);

    // Filter frequent items, sort ascending
    std::vector<std::pair<int, std::string>> freq_items;
    freq_items.reserve(item_counts.size());
    for (auto& [item, cnt] : item_counts)
        if (cnt >= min_count)
            freq_items.push_back({cnt, item});

    std::sort(freq_items.begin(), freq_items.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    const int n_freq = (int)freq_items.size();
    if (n_freq == 0) return py::list();

    std::unordered_map<std::string, int> item_to_id;
    item_to_id.reserve(n_freq * 2);
    std::vector<std::string> id_to_item(n_freq);

    for (int i = 0; i < n_freq; i++) {
        item_to_id[freq_items[i].second] = i;
        id_to_item[i] = std::move(freq_items[i].second);
    }

    if (verbose) py::print("Frequent 1-items:", n_freq);

    // Build TID-sets
    const int n_words = (n_trans + 63) / 64;
    std::vector<TidSet> tid_sets(n_freq, TidSet(n_words));

    for (int t = 0; t < n_trans; t++)
        for (const auto& item : transactions[t]) {
            auto it = item_to_id.find(item);
            if (it != item_to_id.end())
                tid_sets[it->second].set_bit(t);
        }
    transactions.clear();

    // ECLAT
    std::vector<FreqItemset> frequent_itemsets;
    frequent_itemsets.reserve(8192);

    std::vector<std::pair<int, TidSet>> initial_items;
    initial_items.reserve(n_freq);
    for (int i = 0; i < n_freq; i++)
        initial_items.emplace_back(i, std::move(tid_sets[i]));
    tid_sets.clear();

    std::vector<int> prefix;
    prefix.reserve(16);
    eclat(prefix, initial_items, min_count, n_words, frequent_itemsets);

    if (verbose) py::print("Frequent itemsets:", frequent_itemsets.size());

    // Build support lookup
    std::unordered_map<std::vector<int>, int, VecIntHash> supp_map;
    supp_map.reserve(frequent_itemsets.size() * 2);
    for (const auto& fi : frequent_itemsets)
        supp_map[fi.items] = fi.count;

    // Generate rules
    py::list rules;
    std::vector<int> ante, cons;

    for (const auto& fi : frequent_itemsets) {
        const int k = (int)fi.items.size();
        if (k < 2 || k > 62) continue;

        const double  support   = (double)fi.count / n_trans;
        const int64_t full_mask = (1LL << k) - 1;

        for (int64_t mask = 1; mask < full_mask; mask++) {
            ante.clear(); cons.clear();
            for (int b = 0; b < k; b++) {
                if (mask & (1LL << b)) ante.push_back(fi.items[b]);
                else                   cons.push_back(fi.items[b]);
            }

            auto it = supp_map.find(ante);
            if (it == supp_map.end()) continue;

            const double conf = (double)fi.count / it->second;
            if (conf < min_confidence) continue;

            py::list A, B;
            for (int id : ante) A.append(id_to_item[id]);
            for (int id : cons) B.append(id_to_item[id]);

            py::dict rule;
            rule["A"]    = std::move(A);
            rule["B"]    = std::move(B);
            rule["supp"] = support;
            rule["conf"] = conf;
            rules.append(std::move(rule));
        }
    }

    if (verbose) py::print("Rules:", rules.size());
    return rules;
}

PYBIND11_MODULE(assoc_rules_engine, m) {
    m.doc() = "ECLAT bitset association rule engine";
    m.def("find_rules", &find_rules,
          py::arg("filepath"),
          py::arg("min_support"),
          py::arg("min_confidence"),
          py::arg("verbose") = false);
}
