#include "collection.h"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iostream>

using namespace std;

Collection::Collection(const string& filePath)
    : filePath_(filePath), map_(32)
{
    load();
}

Collection::~Collection() {
    save();
}

void Collection::load() {
    ifstream in(filePath_);
    if (!in.good()) return;

    try {
        json jsonArray;
        in >> jsonArray;
        if (!jsonArray.is_array()) return;

        for (const auto& document : jsonArray) {
            if (document.contains("_id")) {
                map_.put(document["_id"].get<string>(), document);
            }
        }
    } catch (...) {}
}

void Collection::save() {
    auto items = map_.items();
    json jsonArray = json::array();
    for (auto& item : items) {
        jsonArray.push_back(item.second);
    }

    ofstream out(filePath_);
    out << jsonArray.dump(4);
}

static string randomHex(size_t length = 16) {
    static mt19937_64 rng((random_device())());
    static const char* hexChars = "0123456789abcdef";

    string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(hexChars[rng() % 16]);
    }

    return result;
}

string Collection::generateId() {
    return randomHex(16);
}

string Collection::insert(const json& document) {
    json copy = document;
    string id = generateId();
    copy["_id"] = id;
    map_.put(id, copy);
    save();
    return id;
}

bool Collection::matchesCondition(const json& document, const string& field, const json& condition) {
    if (!document.contains(field)) return false;

    const json& value = document[field];

    if (condition.is_object()) {
        for (auto it = condition.begin(); it != condition.end(); ++it) {
            string op = it.key();
            const json& rhs = it.value();

            if (op == "$gt") {
                if (!value.is_number() || !rhs.is_number()) return false;
                if (!(value.get<double>() > rhs.get<double>())) return false;

            } else if (op == "$lt") {
                if (!value.is_number() || !rhs.is_number()) return false;
                if (!(value.get<double>() < rhs.get<double>())) return false;

            } else if (op == "$eq") {
                if (!(value == rhs)) return false;

            } else if (op == "$in") {
                if (!rhs.is_array()) return false;
                bool found = false;
                for (const auto& item : rhs) {
                    if (item == value) { found = true; break; }
                }
                if (!found) return false;

            } else if (op == "$like") {
                if (!value.is_string() || !rhs.is_string()) return false;
                if (value.get<string>() != rhs.get<string>()) return false;

            } else {
                return false; 
            }
        }
        return true;
    }

    return (value == condition);
}

bool Collection::matchesQuery(const json& document, const json& query) {
    if (!query.is_object()) return false;

    if (query.contains("$or") && query["$or"].is_array()) {
        for (const auto& clause : query["$or"]) {
            if (matchesQuery(document, clause)) return true;
        }
        return false;
    }

    if (query.contains("$and") && query["$and"].is_array()) {
        for (const auto& clause : query["$and"]) {
            if (!matchesQuery(document, clause)) return false;
        }
    }

    for (auto it = query.begin(); it != query.end(); ++it) {
        string key = it.key();
        const json& condition = it.value();
        if (key == "$and" || key == "$or") continue;

        if (!matchesCondition(document, key, condition)) return false;
    }

    return true;
}

vector<json> Collection::find(const json& query) {
    vector<json> result;
     auto allItems = map_.items();

    for (auto& item : allItems) {
        if (matchesQuery(item.second, query)) {
            result.push_back(item.second);
        }
    }

    return result;
}

int Collection::remove(const json& query) {
    int removedCount = 0;
    auto allItems = map_.items();

    for (auto& item : allItems) {
        if (matchesQuery(item.second, query)) {
            map_.remove(item.first);
            ++removedCount;
        }
    }

    if (removedCount > 0) save();
    return removedCount;
}

void Collection::createIndex(const string& field) {
    cout << "Index created on field '" << field << "' (stub implementation)\n";
}
