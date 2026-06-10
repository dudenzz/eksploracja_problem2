#include "fp_growth.hpp"
#include "read_data.hpp"
#include <iostream>
#include <utility>

int main(int argc, char *argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    double min_support = std::stod(argv[1]);
    double min_confidence = std::stod(argv[2]);
    // C++ wykonuje ciężką część algorytmu, a Pythonowy wrapper parsuje JSON Lines ze stdout
    Dataset data = read_data(argv[3]);
    bool verbose = std::stoi(argv[4]);

    FPGrowth fp(min_support, min_confidence, std::move(data.transactions), std::move(data.item_names), verbose);

    fp.solve();
}
