#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <set>


// PROMPT:
// chciałbym abyś wygenerował kod w c++ algorytmu generowania reguł asocjacyjnych fp-growth. dane wejściowe zostały pobrane przez skrypt pythonowy korzystający z kagglehub i zapisane w folderze {lokacja datasetu}. chciałbym abyś dodał parser csv oraz napisał implementację własnej obsługi zbiorów częstych. chciałbym aby wygenerowane reguły mogły być przechwycone przez pythonowy wrapper, który dodam samodzielnie. 
// CONTEXT:
// readme.md, main.py

#include "config.h"

// Struktura przygotowana pod pybind11
struct Rule {
    std::vector<std::string> A;
    std::vector<std::string> B;
    double support;
    double confidence;
};

class AssociationRuleMiner {
private:
    std::unordered_map<std::string, int> item_to_id;
    std::vector<std::string> id_to_item;
    int total_transactions = 0;

    // Rekurencyjna funkcja budująca warunkowe bazy danych
    void pattern_growth(const std::vector<std::vector<int>>& transactions, 
                        int min_support, 
                        std::vector<int> current_pattern, 
                        std::vector<std::pair<std::vector<int>, int>>& frequent_itemsets) 
    {
        std::map<int, int> item_counts;
        for (const auto& txn : transactions) {
            for (int item : txn) item_counts[item]++;
        }

        for (const auto& pair : item_counts) {
            int item = pair.first;
            int count = pair.second;
            
            if (count >= min_support) {
                std::vector<int> new_pattern = current_pattern;
                new_pattern.push_back(item);
                frequent_itemsets.push_back({new_pattern, count});

                std::vector<std::vector<int>> conditional_db;
                for (const auto& txn : transactions) {
                    auto it = std::find(txn.begin(), txn.end(), item);
                    if (it != txn.end()) {
                        // Bierzemy tylko elementy występujące PO naszym elemencie
                        conditional_db.push_back(std::vector<int>(it + 1, txn.end()));
                    }
                }
                
                if (!conditional_db.empty()) {
                    pattern_growth(conditional_db, min_support, new_pattern, frequent_itemsets);
                }
            }
        }
    }

    // Funkcja pomocnicza do generowania podzbiorów dla reguł
    void generate_subsets(const std::vector<int>& itemset, std::vector<std::vector<int>>& subsets) {
        int n = itemset.size();
        int subset_count = (1 << n); 
        
        for (int i = 1; i < subset_count - 1; ++i) {
            std::vector<int> subset;
            for (int j = 0; j < n; ++j) {
                if (i & (1 << j)) subset.push_back(itemset[j]);
            }
            std::sort(subset.begin(), subset.end());
            subsets.push_back(subset);
        }
    }

public:
    std::vector<Rule> solve(double min_support, double min_confidence, bool verbose = false) {
        std::vector<Rule> generated_rules;
        std::vector<std::vector<int>> transactions;
        
        // Ścieżka do docelowego pliku (możesz zmienić na "data/online_retail_II.csv" przed oddaniem)
        std::string filepath = DATAPATH; 
        
        if (verbose) std::cout << "Wczytywanie danych z pliku CSV..." << std::endl;
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "BLAD: Nie mozna otworzyc pliku: " << filepath << std::endl;
            return generated_rules; 
        }

        std::string line;
        std::getline(file, line); // Pomijamy nagłówek

        std::unordered_map<std::string, std::vector<int>> invoice_map;

        while (std::getline(file, line)) {
            size_t pos1 = line.find(',');
            if (pos1 == std::string::npos) continue;
            std::string invoice = line.substr(0, pos1);
            
            size_t pos2 = line.find(',', pos1 + 1);
            if (pos2 == std::string::npos) continue;
            std::string stock_code = line.substr(pos1 + 1, pos2 - pos1 - 1);

            // Ignorujemy puste wartości i zwroty ('C')
            if (invoice.empty() || invoice[0] == 'C' || stock_code.empty()) continue;

            if (item_to_id.find(stock_code) == item_to_id.end()) {
                item_to_id[stock_code] = id_to_item.size();
                id_to_item.push_back(stock_code);
            }
            int item_id = item_to_id[stock_code];

            invoice_map[invoice].push_back(item_id);
        }
        file.close();

        transactions.reserve(invoice_map.size());
        for (auto& pair : invoice_map) {
            std::vector<int>& transaction = pair.second;
            std::sort(transaction.begin(), transaction.end());
            transaction.erase(std::unique(transaction.begin(), transaction.end()), transaction.end());
            if (!transaction.empty()) transactions.push_back(transaction);
        }

        total_transactions = transactions.size(); 
        if (total_transactions == 0) return generated_rules;

        int min_support_count = static_cast<int>(min_support * total_transactions);

        if (verbose) {
            std::cout << "Wczytano " << total_transactions << " unikalnych transakcji." << std::endl;
            std::cout << "Szukanie zbiorow czestych (min_support_count = " << min_support_count << ")..." << std::endl;
        }

        // --- Wywołanie wewnętrznej metody FIM ---
        std::vector<std::pair<std::vector<int>, int>> frequent_itemsets;
        pattern_growth(transactions, min_support_count, {}, frequent_itemsets);

        if (verbose) std::cout << "Budowa mapy wsparcia i generowanie regul..." << std::endl;
        std::map<std::vector<int>, int> support_map;
        for (auto& pair : frequent_itemsets) {
            std::sort(pair.first.begin(), pair.first.end()); 
            support_map[pair.first] = pair.second;
        }

        for (const auto& itemset_pair : frequent_itemsets) {
            const std::vector<int>& itemset = itemset_pair.first;
            int itemset_support_count = itemset_pair.second;
            
            if (itemset.size() < 2) continue;

            double itemset_support_rate = static_cast<double>(itemset_support_count) / total_transactions;
            std::vector<std::vector<int>> subsets;
            generate_subsets(itemset, subsets);

            for (const auto& A : subsets) {
                auto it = support_map.find(A);
                if (it == support_map.end()) continue; 
                
                int support_A_count = it->second;
                double confidence = static_cast<double>(itemset_support_count) / support_A_count;

                if (confidence >= min_confidence) {
                    std::vector<int> B;
                    std::set_difference(
                        itemset.begin(), itemset.end(),
                        A.begin(), A.end(),
                        std::inserter(B, B.begin())
                    );

                    Rule rule;
                    for (int id : A) rule.A.push_back(id_to_item[id]);
                    for (int id : B) rule.B.push_back(id_to_item[id]);
                    rule.support = itemset_support_rate;
                    rule.confidence = confidence;

                    generated_rules.push_back(rule);
                }
            }
        }

        if (verbose) std::cout << "Zakonczono! Znaleziono " << generated_rules.size() << " regul." << std::endl;
        return generated_rules;
    }
};