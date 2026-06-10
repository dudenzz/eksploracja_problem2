#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <iomanip>

using namespace std;

struct AssociationRule {
    vector<string> A, B;
    double supp, conf;
};

struct FPNode {
    string item;
    int count;
    FPNode* parent;
    unordered_map<string, FPNode*> children; // Zmiana na unordered_map
    FPNode* nodeLink;

    FPNode(string item, FPNode* parent) : item(item), parent(parent), count(0), nodeLink(nullptr) {}
    ~FPNode() { for (auto& pair : children) delete pair.second; }
};

void get_subsets(const vector<string>& items, vector<vector<string>>& subsets) {
    int n = items.size();
    subsets.reserve((1 << n)); 
    for (int i = 1; i < (1 << n) - 1; ++i) {
        vector<string> subset;
        for (int j = 0; j < n; ++j) if ((i >> j) & 1) subset.push_back(items[j]);
        subsets.push_back(subset);
    }
}

vector<AssociationRule> generateRules(const map<set<string>, int>& frequentItemsets, double minConf, int totalTransactions) {
    vector<AssociationRule> rules;
    for (auto const& [itemset, support] : frequentItemsets) {
        if (itemset.size() < 2) continue;
        vector<string> items(itemset.begin(), itemset.end());
        vector<vector<string>> subsets;
        get_subsets(items, subsets);

        for (const auto& ant_vec : subsets) {
            set<string> antecedent(ant_vec.begin(), ant_vec.end());
            if (frequentItemsets.count(antecedent)) {
                double conf = (double)support / frequentItemsets.at(antecedent);
                if (conf >= minConf) {
                    AssociationRule rule;
                    rule.A = ant_vec;
                    set_difference(itemset.begin(), itemset.end(), antecedent.begin(), antecedent.end(), back_inserter(rule.B));
                    rule.supp = (double)support / totalTransactions;
                    rule.conf = conf;
                    rules.push_back(rule);
                }
            }
        }
    }
    return rules;
}

void insertTree(const vector<string>& transaction, int increment, FPNode* root, unordered_map<string, FPNode*>& headerTable) {
    FPNode* curr = root;
    for (const string& item : transaction) {
        auto it = curr->children.find(item);
        if (it != curr->children.end()) {
            it->second->count += increment;
            curr = it->second;
        } else {
            FPNode* newNode = new FPNode(item, curr);
            newNode->count = increment;
            curr->children[item] = newNode;
            newNode->nodeLink = headerTable[item];
            headerTable[item] = newNode;
            curr = newNode;
        }
    }
}

void mineTree(FPNode* root, unordered_map<string, FPNode*>& headerTable, int minSup, set<string> prefix, map<set<string>, int>& frequentItemsets) {
    for (auto const& [itemName, _] : headerTable) {
        set<string> newPrefix = prefix;
        newPrefix.insert(itemName);
        int support = 0;
        FPNode* curr = headerTable.at(itemName);
        while (curr) { support += curr->count; curr = curr->nodeLink; }

        if (support >= minSup) {
            frequentItemsets[newPrefix] = support;
            map<vector<string>, int> condPatterns;
            curr = headerTable.at(itemName);
            while (curr) {
                vector<string> path;
                FPNode* p = curr->parent;
                while (p && p->item != "root") { path.push_back(p->item); p = p->parent; }
                if (!path.empty()) { reverse(path.begin(), path.end()); condPatterns[path] = curr->count; }
                curr = curr->nodeLink;
            }

            unordered_map<string, int> condFList;
            for (auto const& [path, count] : condPatterns) for (const string& it : path) condFList[it] += count;

            FPNode* condRoot = new FPNode("root", nullptr);
            unordered_map<string, FPNode*> condHeaderTable;
            for (auto const& [path, count] : condPatterns) {
                vector<string> filtered;
                for (const string& it : path) if (condFList[it] >= minSup) filtered.push_back(it);
                if (!filtered.empty()) insertTree(filtered, count, condRoot, condHeaderTable);
            }
            if (!condHeaderTable.empty()) mineTree(condRoot, condHeaderTable, minSup, newPrefix, frequentItemsets);
            delete condRoot;
        }
    }
}

// Szybszy parser CSV bez stringstream
vector<vector<string>> loadCSV(string path) {
    vector<vector<string>> transactions;
    ifstream file(path);
    if (!file.is_open()) return transactions;
    unordered_map<string, vector<string>> groups;
    string line; getline(file, line); 
    while (getline(file, line)) {
        size_t first_comma = line.find(',');
        size_t second_comma = line.find(',', first_comma + 1);
        if (first_comma == string::npos || second_comma == string::npos) continue;
        string inv = line.substr(0, first_comma);
        string itm = line.substr(first_comma + 1, second_comma - first_comma - 1);
        if (isdigit(inv[0])) groups[inv].push_back(itm);
    }
    for (auto& [id, items] : groups) {
        sort(items.begin(), items.end());
        items.erase(unique(items.begin(), items.end()), items.end());
        transactions.push_back(items);
    }
    return transactions;
}

int main(int argc, char* argv[]) {
    if (argc < 5) return 1;
    double minSupPct = stod(argv[1]), minConf = stod(argv[2]);
    bool silent = (string(argv[3]) == "1");
    vector<vector<string>> transactions = loadCSV(argv[4]);
    int nTrans = transactions.size(), minSupCount = (int)ceil(minSupPct * nTrans);

    unordered_map<string, int> f_list;
    for (auto const& trans : transactions) for (auto const& it : trans) f_list[it]++;

    for (auto& trans : transactions) {
        trans.erase(remove_if(trans.begin(), trans.end(), [&](const string& i) { return f_list[i] < minSupCount; }), trans.end());
        sort(trans.begin(), trans.end(), [&](const string& a, const string& b) {
            return f_list[a] != f_list[b] ? f_list[a] > f_list[b] : a < b;
        });
    }

    FPNode* root = new FPNode("root", nullptr);
    unordered_map<string, FPNode*> headerTable;
    for (auto const& trans : transactions) if (!trans.empty()) insertTree(trans, 1, root, headerTable);

    map<set<string>, int> frequentItemsets;
    mineTree(root, headerTable, minSupCount, {}, frequentItemsets);
    vector<AssociationRule> finalRules = generateRules(frequentItemsets, minConf, nTrans);

    if (!silent) {
        cout << "Frequent itemsets: " << frequentItemsets.size() << "\nRules count: " << finalRules.size() << "\n";
        for (const auto& r : finalRules) {
            cout << "{";
            for (size_t i = 0; i < r.A.size(); ++i) cout << r.A[i] << (i < r.A.size() - 1 ? "," : "");
            cout << "}=>{";
            for (size_t i = 0; i < r.B.size(); ++i) cout << r.B[i] << (i < r.B.size() - 1 ? "," : "");
            cout << "}," << r.supp << "," << r.conf << "\n";
        }
    }
    delete root;
    return 0;
}