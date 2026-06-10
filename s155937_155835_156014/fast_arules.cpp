#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <intrin.h>
#include <omp.h>
// #ifndef _WIN64
// #include <nmmintrin.h>
// inline unsigned long long __popcnt64(unsigned long long value) {
//     return __popcnt((unsigned int)(value & 0xFFFFFFFF)) + __popcnt((unsigned int)(value >> 32));
// }
// #endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int G_NWORDS = 0;

struct BitsetPool {
    std::vector<uint64_t*> blocks; uint64_t* cur; int rem;
    static constexpr int BLK = 4096;
    BitsetPool() : cur(0), rem(0) {}
    ~BitsetPool() { for (auto b : blocks) _aligned_free(b); }
    uint64_t* alloc() {
        if (rem <= 0) {
            size_t sz = (size_t)BLK * G_NWORDS * sizeof(uint64_t);
            uint64_t* b = (uint64_t*)_aligned_malloc(sz, 64);
            memset(b, 0, sz); blocks.push_back(b); cur = b; rem = BLK;
        }
        uint64_t* p = cur; cur += G_NWORDS; --rem; return p;
    }
};

static inline int intersect_count(const uint64_t* a, const uint64_t* b, uint64_t* o) {
    int c = 0; for (int i = 0; i < G_NWORDS; ++i) { o[i] = a[i] & b[i]; c += (int)__popcnt64(o[i]); }
    return c;
}

struct MmFile {
    HANDLE hf, hm; const char* d; size_t sz;
    MmFile() : hf(INVALID_HANDLE_VALUE), hm(0), d(0), sz(0) {}
    ~MmFile() { close(); }
    bool open(const char* p) {
        hf = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
        if (hf == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER li; GetFileSizeEx(hf, &li); sz = (size_t)li.QuadPart;
        hm = CreateFileMappingA(hf, 0, PAGE_READONLY, 0, 0, 0);
        if (!hm) return false;
        d = (const char*)MapViewOfFile(hm, FILE_MAP_READ, 0, 0, 0);
        return d != 0;
    }
    void close() { if (d) UnmapViewOfFile(d); if (hm) CloseHandle(hm); if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf); d = 0; hm = 0; hf = INVALID_HANDLE_VALUE; }
};

struct StringHash {
    size_t operator()(std::string_view s) const {
        size_t h = 2166136261u; for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; } return h;
    }
};

struct EclatItem { int id; uint64_t* ts; int sup; };
struct FIS { std::vector<int> items; int sup; };
struct Rule { std::vector<int> A, B; double supp, conf; };

static void eclat_rec(const std::vector<int>& pfx, std::vector<EclatItem>& items, int msup, std::vector<FIS>& out, BitsetPool& pool) {
    for (int i = 0; i < (int)items.size(); ++i) {
        std::vector<int> np = pfx; np.push_back(items[i].id); out.push_back({np, items[i].sup});
        std::vector<EclatItem> ni;
        for (int j = i + 1; j < (int)items.size(); ++j) {
            uint64_t* x = pool.alloc(); int c = intersect_count(items[i].ts, items[j].ts, x);
            if (c >= msup) ni.push_back({items[j].id, x, c});
        }
        if (!ni.empty()) eclat_rec(np, ni, msup, out, pool);
    }
}

static inline uint64_t hash_is(const int* a, int n) {
    uint64_t h = 14695981039346656037ULL; for (int i = 0; i < n; ++i) { h ^= (uint64_t)a[i]; h *= 1099511628211ULL; } return h;
}

static void gen_rules(const std::vector<FIS>& freq, int nt, double mconf, std::vector<Rule>& rules) {
    std::unordered_map<uint64_t, int> sm;
    for (auto& f : freq) {
        std::vector<int> s = f.items; std::sort(s.begin(), s.end());
        sm[hash_is(s.data(), (int)s.size())] = f.sup;
    }
    for (auto& f : freq) {
        int sz = (int)f.items.size(); if (sz < 2) continue;
        std::vector<int> si = f.items; std::sort(si.begin(), si.end());
        double sab = (double)f.sup / nt;
        for (int m = 1; m < (1 << sz) - 1; ++m) {
            int ant[16], an = 0; for (int b = 0; b < sz; ++b) if (m & (1 << b)) ant[an++] = si[b];
            auto it = sm.find(hash_is(ant, an)); if (it == sm.end()) continue;
            double co = (double)f.sup / it->second;
            if (co >= mconf) {
                Rule r; r.A.assign(ant, ant + an); for (int b = 0; b < sz; ++b) if (!(m & (1 << b))) r.B.push_back(si[b]);
                r.supp = sab; r.conf = co; rules.push_back(std::move(r));
            }
        }
    }
}

static struct { char* json; double t; } g_res = {};

extern "C" {
__declspec(dllexport) int solve_arules(const char* datapath, double min_support, double min_confidence, int verbose) {
    auto T0 = std::chrono::high_resolution_clock::now();
    MmFile mf; if (!mf.open(datapath)) return 0;
    const char *p = mf.d, *end = mf.d + mf.sz;
    while (p < end && *p != '\n') ++p; if (p < end) ++p;
    std::unordered_map<std::string_view, int, StringHash> item_map, inv_map;
    std::vector<std::string_view> item_names;
    struct TransData { std::vector<int> items; };
    std::vector<TransData> transactions;
    while (p < end) {
        const char* inv_s = p; while (p < end && *p != ',') ++p; int inv_len = (int)(p - inv_s);
        if (inv_len == 0 || inv_s[0] < '0' || inv_s[0] > '9') { while (p < end && *p != '\n') ++p; if (p < end) ++p; continue; }
        if (p < end) ++p;
        const char* sc_s = p; while (p < end && *p != ',') ++p; int sc_len = (int)(p - sc_s);
        while (p < end && *p != '\n') ++p; if (p < end) ++p;
        if (sc_len == 0) continue;
        std::string_view inv_sv(inv_s, inv_len), sc_sv(sc_s, sc_len);
        int tid; auto it_inv = inv_map.find(inv_sv);
        if (it_inv == inv_map.end()) { tid = (int)transactions.size(); inv_map[inv_sv] = tid; transactions.push_back({}); } else tid = it_inv->second;
        int iid; auto it_sc = item_map.find(sc_sv);
        if (it_sc == item_map.end()) { iid = (int)item_names.size(); item_map[sc_sv] = iid; item_names.push_back(sc_sv); } else iid = it_sc->second;
        transactions[tid].items.push_back(iid);
    }
    int num_trans = (int)transactions.size(), num_items = (int)item_names.size();
    int msup = (int)std::ceil(min_support * num_trans); G_NWORDS = (num_trans + 63) / 64;
    if (verbose) fprintf(stderr, "[C++] Transactions: %d, Items: %d\n", num_trans, num_items);
    BitsetPool mainPool; std::vector<uint64_t*> tidsets(num_items);
    for (int i = 0; i < num_items; ++i) tidsets[i] = mainPool.alloc();
    for (int tid = 0; tid < num_trans; ++tid) {
        std::sort(transactions[tid].items.begin(), transactions[tid].items.end());
        transactions[tid].items.erase(std::unique(transactions[tid].items.begin(), transactions[tid].items.end()), transactions[tid].items.end());
        for (int iid : transactions[tid].items) tidsets[iid][tid >> 6] |= (1ULL << (tid & 63));
    }
    std::vector<EclatItem> f1;
    for (int i = 0; i < num_items; ++i) {
        int cnt = 0; for (int w = 0; w < G_NWORDS; ++w) cnt += (int)__popcnt64(tidsets[i][w]);
        if (cnt >= msup) f1.push_back({i, tidsets[i], cnt});
    }
    std::sort(f1.begin(), f1.end(), [](auto& a, auto& b) { return a.sup > b.sup; });
    std::vector<FIS> all_freq; for (auto& it : f1) all_freq.push_back({{it.id}, it.sup});
    int nf = (int)f1.size(); std::vector<std::vector<FIS>> thr_res(nf);
    std::vector<BitsetPool> pools(omp_get_max_threads());
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < nf; ++i) {
        BitsetPool& pool = pools[omp_get_thread_num()];
        std::vector<EclatItem> cands;
        for (int j = i + 1; j < nf; ++j) {
            uint64_t* x = pool.alloc(); int c = intersect_count(f1[i].ts, f1[j].ts, x);
            if (c >= msup) cands.push_back({f1[j].id, x, c});
        }
        for (int c = 0; c < (int)cands.size(); ++c) {
            std::vector<int> pfx = {f1[i].id, cands[c].id}; thr_res[i].push_back({pfx, cands[c].sup});
            std::vector<EclatItem> deeper;
            for (int d = c + 1; d < (int)cands.size(); ++d) {
                uint64_t* x2 = pool.alloc(); int c2 = intersect_count(cands[c].ts, cands[d].ts, x2);
                if (c2 >= msup) deeper.push_back({cands[d].id, x2, c2});
            }
            if (!deeper.empty()) eclat_rec(pfx, deeper, msup, thr_res[i], pool);
        }
    }
    for (auto& tr : thr_res) for (auto& fi : tr) all_freq.push_back(std::move(fi));
    std::vector<Rule> rules; gen_rules(all_freq, num_trans, min_confidence, rules);
    if (verbose) fprintf(stderr, "[C++] Itemsets: %d, Rules: %d\n", (int)all_freq.size(), (int)rules.size());
    std::string json = "[";
    for (int r = 0; r < (int)rules.size(); ++r) {
        if (r > 0) json += ","; json += "{\"A\":[";
        for (int a = 0; a < (int)rules[r].A.size(); ++a) { json += (a > 0 ? ",\"" : "\""); json += item_names[rules[r].A[a]]; json += "\""; }
        json += "],\"B\":[";
        for (int b = 0; b < (int)rules[r].B.size(); ++b) { json += (b > 0 ? ",\"" : "\""); json += item_names[rules[r].B[b]]; json += "\""; }
        char nb[64]; snprintf(nb, sizeof(nb), "],\"supp\":%.6f,\"conf\":%.6f}", rules[r].supp, rules[r].conf); json += nb;
    }
    json += "]";
    g_res.t = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - T0).count();
    free(g_res.json); g_res.json = (char*)malloc(json.size() + 1); memcpy(g_res.json, json.c_str(), json.size() + 1);
    mf.close(); return (int)rules.size();
}
__declspec(dllexport) const char* get_json_result() { return g_res.json; }
__declspec(dllexport) double get_elapsed_seconds() { return g_res.t; }
__declspec(dllexport) void free_result() { free(g_res.json); g_res.json = 0; }
}
