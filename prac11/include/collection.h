#pragma once
#include "hashmap.h"
#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

class Collection {
public:
    Collection(const std::string& path);
    ~Collection();

    // load/save
    void load();
    void save();

    // CRUD
    std::string insert(json document);
    std::vector<json> find(const json& query);
    int remove(const json& query);

    // optional index creation stub
    void create_index(const std::string& field);

private:
    std::string file_path;
    HashMap<std::string, json> map; // key: _id, value: document

    std::string gen_id();
    bool matches_query(const json& doc, const json& query);
    bool matches_condition(const json& doc, const std::string& field, const json& cond);
};