#include <bits/stdc++.h>
using namespace std;

struct Candidate {
    int itemId;
    vector<int> transactionIds;
};

vector<string> itemNames;
map<vector<int>, int> supportByItemset;
int minSupportCount;
int transactionCount;

bool isNumber(const string &text) {
    return !text.empty() && all_of(text.begin(), text.end(), [](unsigned char c) {
        return isdigit(c);
    });
}

vector<int> intersectionOf(const vector<int> &first, const vector<int> &second) {
    vector<int> result;
    result.reserve(min(first.size(), second.size()));
    set_intersection(first.begin(), first.end(), second.begin(), second.end(), back_inserter(result));
    return result;
}

void eclat(const vector<int> &prefix, const vector<Candidate> &candidates) {
    for (int i = 0; i < (int)candidates.size(); i++) {
        const Candidate &current = candidates[i];
        vector<int> itemset = prefix;
        itemset.push_back(current.itemId);
        supportByItemset[itemset] = current.transactionIds.size();

        vector<Candidate> nextCandidates;
        for (int j = i + 1; j < (int)candidates.size(); j++) {
            vector<int> commonTransactions = intersectionOf(current.transactionIds, candidates[j].transactionIds);
            if ((int)commonTransactions.size() >= minSupportCount) {
                nextCandidates.push_back({candidates[j].itemId, commonTransactions});
            }
        }
        eclat(itemset, nextCandidates);
    }
}

string joinItems(const vector<int> &itemset) {
    vector<string> names;
    names.reserve(itemset.size());
    for (int itemId : itemset) {
        names.push_back(itemNames[itemId]);
    }

    sort(names.begin(), names.end());
    string result;
    for (const string &name : names) {
        if (!result.empty()) {
            result += "|";
        }
        result += name;
    }
    return result;
}

void printRules(double minConfidence) {
    cout << setprecision(17);
    for (const auto &itemsetInfo : supportByItemset) {
        const vector<int> &itemset = itemsetInfo.first;
        int itemsetSupport = itemsetInfo.second;
        int itemsetSize = itemset.size();

        if (itemsetSize < 2 || itemsetSize > 30) {
            continue;
        }

        for (int mask = 1; mask < (1 << itemsetSize) - 1; mask++) {
            vector<int> leftSide;
            vector<int> rightSide;
            for (int i = 0; i < itemsetSize; i++) {
                if (mask & (1 << i)) {
                    leftSide.push_back(itemset[i]);
                } else {
                    rightSide.push_back(itemset[i]);
                }
            }

            auto leftSideSupport = supportByItemset.find(leftSide);
            if (leftSideSupport == supportByItemset.end()) {
                continue;
            }

            double support = (double)itemsetSupport / transactionCount;
            double confidence = (double)itemsetSupport / leftSideSupport->second;
            if (confidence >= minConfidence) {
                cout << joinItems(leftSide) << "\t" << joinItems(rightSide) << "\t";
                cout << support << "\t" << confidence << "\n";
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        return 1;
    }

    ifstream file(argv[1]);
    if (!file.is_open()) {
        return 2;
    }

    double minSupport = atof(argv[2]);
    double minConfidence = atof(argv[3]);
    unordered_map<string, int> itemToId;
    unordered_map<string, vector<int>> invoiceItems;

    string line;
    getline(file, line);
    while (getline(file, line)) {
        size_t firstComma = line.find(',');
        size_t secondComma = firstComma == string::npos ? string::npos : line.find(',', firstComma + 1);
        if (secondComma == string::npos) {
            continue;
        }

        string invoice = line.substr(0, firstComma);
        string stockCode = line.substr(firstComma + 1, secondComma - firstComma - 1);
        if (!isNumber(invoice) || stockCode.empty()) {
            continue;
        }

        if (itemToId.find(stockCode) == itemToId.end()) {
            itemToId[stockCode] = itemNames.size();
            itemNames.push_back(stockCode);
        }
        invoiceItems[invoice].push_back(itemToId[stockCode]);
    }

    vector<vector<int>> itemTransactions(itemNames.size());
    for (auto invoiceInfo : invoiceItems) {
        vector<int> items = invoiceInfo.second;
        sort(items.begin(), items.end());
        items.erase(unique(items.begin(), items.end()), items.end());
        if (items.empty()) {
            continue;
        }

        int transactionId = transactionCount++;
        for (int itemId : items) {
            itemTransactions[itemId].push_back(transactionId);
        }
    }

    if (transactionCount == 0) {
        return 0;
    }

    if (minSupport < 1) {
        minSupportCount = ceil(minSupport * transactionCount);
    } else {
        minSupportCount = minSupport;
    }
    minSupportCount = max(1, minSupportCount);

    vector<Candidate> candidates;
    for (int itemId = 0; itemId < (int)itemTransactions.size(); itemId++) {
        if ((int)itemTransactions[itemId].size() >= minSupportCount) {
            candidates.push_back({itemId, itemTransactions[itemId]});
        }
    }

    sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.transactionIds.size() == b.transactionIds.size()) {
            return a.itemId < b.itemId;
        }
        return a.transactionIds.size() < b.transactionIds.size();
    });

    eclat({}, candidates);
    printRules(minConfidence);
    return 0;
}
