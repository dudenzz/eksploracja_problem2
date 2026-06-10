#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using Item = int;

// Itemy są wewnętrznie liczbami całkowitymi; oryginalne StockCode trzymamy osobno
// w Dataset::item_names. Dzięki temu gorące pętle nie porównują stringów
using Itemset = std::vector<Item>;

// Itemset jest zawsze posortowany. Pozwala to używać vectora jako klucza mapy
// bez kosztu wielu małych alokacji, które generowałby std::set
class ItemsetHash {
  public:
    size_t operator()(const Itemset &items) const {
        size_t seed = items.size();
        for (Item item : items) {
            seed ^= static_cast<size_t>(item) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

using FrequentMap = std::unordered_map<Itemset, int, ItemsetHash>;
using Transaction = std::vector<Item>;
using Transactions = std::vector<Transaction>;

class Dataset {
  public:
    Transactions transactions;
    // item_names[id] zwraca oryginalny kod produktu potrzebny przy wypisywaniu wyniku
    std::vector<std::string> item_names;
};

// Węzeł FP-tree. next_link łączy wszystkie węzły tego samego itemu przez header table
class Node {
  public:
    Item item;
    int count;

    std::weak_ptr<Node> parent;
    std::unordered_map<Item, std::shared_ptr<Node>> children;
    std::weak_ptr<Node> next_link;

    Node(Item item, int count,
         std::shared_ptr<Node> parent = nullptr)
        : item(item), count(count), parent(parent) {}
};

using NodePointer = std::shared_ptr<Node>;

class HeaderEntry {
  public:
    int count = 0;
    NodePointer head = nullptr;
    // tail przyspiesza dopinanie next_link z O(k) do O(1)
    NodePointer tail = nullptr;
};

using HeaderTable = std::unordered_map<Item, HeaderEntry>;

class Rule {
  public:
    Transaction A;
    Transaction B;
    double supp;
    double conf;
};
