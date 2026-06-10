#pragma once

#include "fp_types.hpp"
#include <cmath>
#include <utility>

class FPGrowth {
  private:
    bool verbose;

    double min_support;
    double min_confidence;

    Transactions transactions;
    std::vector<std::string> item_names;
    size_t n_trans;
    int min_support_count;

    NodePointer root;
    HeaderTable header_table;
    // Mapa wszystkich znalezionych zbiorów częstych: posortowany itemset -> licznik
    FrequentMap frequent_itemsets;

  public:
    FPGrowth(double min_support, double min_confidence, Transactions data,
             std::vector<std::string> item_names, bool verbose) {

        this->verbose = verbose;
        this->min_support = min_support;
        this->min_confidence = min_confidence;
        this->transactions = std::move(data);
        this->item_names = std::move(item_names);
        this->n_trans = transactions.size();
        this->min_support_count = static_cast<int>(std::ceil(min_support * n_trans));
        this->root = std::make_shared<Node>(-1, 0);
    }

    void solve();

  private:
    // Liczy globalne wystąpienia itemów; używa lokalnych liczników per wątek
    void count_items(std::vector<int> &item_counts);

    // Filtruje transakcje do itemów częstych i buduje właściwe FP-tree
    void build_tree(const Transaction &sorted_items,
                    const std::vector<int> &item_counts);

    // Rekurencyjnie wydobywa zbiory częste z drzewa i drzew warunkowych
    void mine_tree(HeaderTable &table, Itemset prefix);

    void insert_tree(const Transaction &items, NodePointer node,
                     HeaderTable &table, size_t idx = 0);

    // Generuje reguły asocjacyjne i wypisuje je jako JSON Lines do wrappera Pythona
    void generate_rules();
};
