#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <algorithm>

using namespace std;
namespace py = pybind11;

// A -> B
struct AssociationRule {
    vector<string> A;
    vector<string> B;
    double supp;
    double conf;
};

// Zbiór częsty
struct FrequentItemset {
    vector<string> items;
    double supp;
};

struct MiningResult {
    vector<FrequentItemset> frequent_itemsets;
    vector<AssociationRule> rules;
};

map<vector<int>, double> supp_cache;

// Przechowuje id transakcji, w których dany zbiór nie wystepuje
typedef vector<int> DiffSet;

// Generuje r. asocjacyjne dla danego zbioru częstego
void generate_rules(const vector<int>& itemset, double supp, double min_conf, const map<int, string>& id_to_item, 
    vector<AssociationRule>& rules) {

    int n = itemset.size();
    if (n<2) 
        return;

    for(int i =1; i < (1 << n)-1; i++){
        vector<int> left_set, right_set;
        for(int j=0; j<n; j++){
            if ((i >> j) & 1){
                left_set.push_back(itemset[j]);
            } else {
                right_set.push_back(itemset[j]);
            }
        }

        sort(left_set.begin(), left_set.end());
        double supp_A = supp_cache[left_set];
        double confidence = supp/supp_A;

        if (confidence >= min_conf){
            AssociationRule r;
            for (int id : left_set) r.A.push_back(id_to_item.at(id));
            for (int id : right_set) r.B.push_back(id_to_item.at(id));
            r.supp = supp;
            r.conf = confidence;
            rules.push_back(r);
        }
    }
}

// Rekurencyjne dEclat - eksploracja drzewa
void declat_recursive(const vector<int>& prefix, int prefix_supp_cnt, const DiffSet& prefix_diff, vector<pair<int, DiffSet>>& items, 
    int min_supp_cnt, int total_n, double min_conf, const map<int, string>& id_to_item, MiningResult& result) {

    for (size_t i=0; i<items.size(); i++) {
        int new_supp_cnt = prefix_supp_cnt - items[i].second.size();

        if(new_supp_cnt >= min_supp_cnt){
            vector<int> new_pref = prefix; 
            new_pref.push_back(items[i].first);
            sort(new_pref.begin(), new_pref.end());
            double current_supp = (double)new_supp_cnt/total_n;
            supp_cache[new_pref] = current_supp;

            FrequentItemset f;
            for(int id: new_pref) f.items.push_back(id_to_item.at(id));
            f.supp = current_supp;
            result.frequent_itemsets.push_back(f);

            generate_rules(new_pref, current_supp, min_conf, id_to_item, result.rules);

            vector<pair<int, DiffSet>> next_items;
            for (size_t j=i+1; j<items.size(); j++){
                DiffSet next_diff;
                set_difference(items[j].second.begin(), items[j].second.end(), items[i].second.begin(), items[i].second.end(),
                    back_inserter(next_diff));
                next_items.push_back({items[j].first, next_diff});
            }
            
            if (!next_items.empty()) {
                declat_recursive(new_pref, new_supp_cnt, items[i].second, next_items, min_supp_cnt, total_n, min_conf, 
                    id_to_item, result);
            }
        }
    }

}

// Parsowanie csv, wydobycie danych pierwszze
MiningResult solve_eclat(string path, double min_supp, double min_conf){
    supp_cache.clear();
    MiningResult result;
    unordered_map<string, int> item_to_id;
    map<int, string> id_to_item;
    unordered_map<string, vector<int>> trans_map;
    int current_id = 0;
    ifstream file(path);
    string line;
    getline(file, line);

    // Wstepna obróbka csv
    while(getline(file, line)){
        if(line.empty())
            continue;
        
        size_t c1 = line.find(',');
        if (c1 == string::npos)
            continue;

        string inv = line.substr(0, c1);
        size_t c2 = line.find(',', c1+1);
        string stock = line.substr(c1+1, c2-c1-1);

        if(!inv.empty() && isdigit(inv[0]) && !stock.empty()){
            if(item_to_id.find(stock) == item_to_id.end()){
                item_to_id[stock] = current_id;
                id_to_item[current_id] = stock;
                current_id++;
            }
            trans_map[inv].push_back(item_to_id[stock]);
        }
    }

    int n_trans = trans_map.size();
    int min_supp_cnt = (int)(min_supp*n_trans);
    map<int, vector<int>> tidsets;
    int t_idx = 0;

    // Budowanie zbiorów transakcji dla produktu
    for(auto& t : trans_map){
        sort(t.second.begin(), t.second.end());
        t.second.erase(unique(t.second.begin(), t.second.end()), t.second.end());
        for (int id : t.second) tidsets[id].push_back(t_idx);
        t_idx++;
    }

    // Jednoelementowe zbiory i diffsety
    vector<pair<int, DiffSet>> initial_items;
    for (auto const& [id, tids] : tidsets){
        if(tids.size()>=min_supp_cnt){
            double s = (double)tids.size()/n_trans;
            supp_cache[{id}]=s;

            FrequentItemset f;
            f.items = {id_to_item[id]};
            f.supp = s;
            result.frequent_itemsets.push_back(f);

            DiffSet d;
            int cur = 0;
            for(int i =0; i<n_trans; i++){
                if (cur < tids.size() && tids[cur] == i) 
                    cur++;
                else 
                    d.push_back(i);
            }
            initial_items.push_back({id, d});
        }
    }

    // Początek algorytmu
    for (size_t i =0; i<initial_items.size(); i++){
        vector<pair<int, DiffSet>> next_items;
        int supp_i = n_trans-initial_items[i].second.size();
        for (size_t j = i+1; j<initial_items.size(); j++) {
            DiffSet d;
            set_difference(initial_items[j].second.begin(), initial_items[j].second.end(), initial_items[i].second.begin(), initial_items[i].second.end(),
                           back_inserter(d));
            next_items.push_back({initial_items[j].first, d});
        }
        declat_recursive({initial_items[i].first}, supp_i, initial_items[i].second, next_items, min_supp_cnt, n_trans, min_conf, id_to_item, result);
    }

    return result;
}

PYBIND11_MODULE(eclat_155921_execute, m) {
    py::class_<AssociationRule>(m, "AssociationRule")
        .def_readonly("A", &AssociationRule::A)
        .def_readonly("B", &AssociationRule::B)
        .def_readonly("supp", &AssociationRule::supp)
        .def_readonly("conf", &AssociationRule::conf);

    py::class_<FrequentItemset>(m, "FrequentItemset")
        .def_readonly("items", &FrequentItemset::items)
        .def_readonly("supp", &FrequentItemset::supp);

    py::class_<MiningResult>(m, "MiningResult")
        .def_readonly("frequent_itemsets", &MiningResult::frequent_itemsets)
        .def_readonly("rules", &MiningResult::rules);

    m.def("solve", &solve_eclat);
}