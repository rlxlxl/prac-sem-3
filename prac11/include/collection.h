#pragma once
#include "hashmap.h"
#include "json.hpp"
#include <string>
#include <vector>
using namespace std;
using json = nlohmann::json;

class Collection {
public:
    Collection(const string& filePath);
    ~Collection();

    void load();
    void save();

    string insert(const json& document);
    vector<json> find(const json& query);
    int remove(const json& query);

    void createIndex(const string& field);

private:
    string filePath_;                 
    HashMap<string, json> map_;     

    string generateId();
    bool matchesQuery(const json& document, const json& query);
    bool matchesCondition(const json& document, const string& field, const json& condition);
};
