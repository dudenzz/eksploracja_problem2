#include "read_data.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

Dataset read_data(const std::string &datapath) {
    std::ifstream file(datapath);
    Dataset dataset;

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << datapath << "\n";
        return dataset;
    }

    std::string line;
    std::getline(file, line); // skip header

    // Invoice zostaje stringiem, żeby nie sklejać np. "00123" i "123"
    std::unordered_map<std::string, std::unordered_set<Item>> trans_dict;
    std::unordered_map<std::string, Item> item_ids;

    while (std::getline(file, line)) {
        size_t first_comma = line.find(',');
        if (first_comma == std::string::npos) {
            continue;
        }

        std::string_view id_view(line.data(), first_comma);
        if (id_view.empty() || !std::all_of(id_view.begin(), id_view.end(), [](unsigned char ch) { return std::isdigit(ch); })) {
            continue;
        }

        size_t second_comma = line.find(',', first_comma + 1);
        std::string item = line.substr(
            first_comma + 1,
            second_comma == std::string::npos ? std::string::npos : second_comma - first_comma - 1
        );

        if (!item.empty()) {
            // Każdy StockCode dostaje mały int używany dalej w FP-Growth
            auto item_it = item_ids.find(item);
            if (item_it == item_ids.end()) {
                Item id = static_cast<Item>(dataset.item_names.size());
                dataset.item_names.push_back(item);
                item_it = item_ids.emplace(std::move(item), id).first;
            }

            trans_dict[std::string(id_view)].insert(item_it->second);
        }
    }

    dataset.transactions.reserve(trans_dict.size());
    for (auto &[_, items] : trans_dict) {
        // Transakcja jest zbiorem produktów
        dataset.transactions.emplace_back(items.begin(), items.end());
    }

    return dataset;
}
