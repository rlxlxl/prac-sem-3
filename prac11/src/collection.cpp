#include "collection.h"
#include <fstream>
#include <sstream>
#include <random>
#include <regex>
#include <algorithm>
#include <iostream>

Collection::Collection(const std::string& path)
    : file_path(path), map(32)
{
    load();
}

Collection::~Collection() {
    save();
}

void Collection::load() {
    std::ifstream in(file_path);
    if (!in.good()) return;
    try {
        json arr;
        in >> arr;
        if (!arr.is_array()) return;
        for (const auto &doc : arr) {
            if (doc.contains("_id")) {
                map.put(doc["_id"].get<std::string>(), doc);
            }
        }
    } catch (...) {
        // ignore parse errors
    }
}

void Collection::save() {
    auto items = map.items();
    json arr = json::array();
    for (auto &p : items) {
        arr.push_back(p.second);
    }
    std::ofstream out(file_path);
    out << arr.dump(4);
}

// random _id generator
static std::string random_hex(size_t len = 16) {
    static std::mt19937_64 rng((std::random_device())());
    static const char *chars = "0123456789abcdef";
    std::string s;
    s.reserve(len);
    for (size_t i=0;i<len;++i) s.push_back(chars[rng() % 16]);
    return s;
}

std::string Collection::gen_id() {
    return random_hex(16);
}

std::string Collection::insert(json document) {
    if (!document.is_object()) return "";
    std::string id = gen_id();
    document["_id"] = id;
    map.put(id, document);
    save();
    return id;
}

// convert SQL-like %/_ to regex
static std::regex like_to_regex(const std::string &pattern) {
    std::string re;
    re.reserve(pattern.size()*2);
    re.push_back('^');

    for (char c : pattern) {
        if (c == '%') re += ".*";
        else if (c == '_') re += ".";
        else if (std::string(".^$+()[]{}|\\").find(c) != std::string::npos) {
            re.push_back('\\'); re.push_back(c);
        } else {
            re.push_back(c);
        }
    }

    re.push_back('$');
    return std::regex(re, std::regex::ECMAScript | std::regex::icase);
}

bool Collection::matches_condition(const json& doc, const std::string& field, const json& cond) {
    if (!doc.contains(field)) return false;
    const json &value = doc[field];

    if (cond.is_object()) {
        for (auto it = cond.begin(); it != cond.end(); ++it) {
            std::string op = it.key();
            const json &rhs = it.value();

            if (op == "$gt") {
                if (!value.is_number() || !rhs.is_number()) return false;
                if (!((value.get<double>()) > (rhs.get<double>()))) return false;

            } else if (op == "$lt") {
                if (!value.is_number() || !rhs.is_number()) return false;
                if (!((value.get<double>()) < (rhs.get<double>()))) return false;

            } else if (op == "$like") {
                if (!value.is_string() || !rhs.is_string()) return false;
                auto rx = like_to_regex(rhs.get<std::string>());
                if (!std::regex_match(value.get<std::string>(), rx)) return false;

            } else if (op == "$in") {
                if (!rhs.is_array()) return false;
                bool found = false;
                for (const auto &item : rhs) {
                    if (item == value) { found = true; break; }
                }
                if (!found) return false;

            } else if (op == "$eq") {
                if (!(value == rhs)) return false;

            } else {
                return false; // unknown operator
            }
        }
        return true;
    }

    // default EQ
    return (value == cond);
}

// recursive AND / OR logic
bool Collection::matches_query(const json& doc, const json& query) {
    if (!query.is_object()) return false;

    // explicit OR
    if (query.contains("$or") && query["$or"].is_array()) {
        for (const auto &cl : query["$or"]) {
            if (matches_query(doc, cl)) return true;
        }
        return false;
    }

    // explicit AND
    if (query.contains("$and") && query["$and"].is_array()) {
        for (const auto &cl : query["$and"]) {
            if (!matches_query(doc, cl)) return false;
        }
    }

    // implicit AND
    for (auto it = query.begin(); it != query.end(); ++it) {
        std::string key = it.key();
        const json &cond = it.value();
        if (key == "$and" || key == "$or") continue;

        if (!matches_condition(doc, key, cond)) return false;
    }

    return true;
}

std::vector<json> Collection::find(const json& query) {
    std::vector<json> result;
    auto all = map.items();

    for (auto &p : all)
        if (matches_query(p.second, query))
            result.push_back(p.second);

    return result;
}

int Collection::remove(const json& query) {
    int removed = 0;
    auto all = map.items();

    for (auto &p : all) {
        if (matches_query(p.second, query)) {
            map.remove(p.first);
            ++removed;
        }
    }

    if (removed > 0) save();
    return removed;
}

void Collection::create_index(const std::string& field) {
    std::cout << "Index created on field '" << field << "' (stub implementation)\n";
}