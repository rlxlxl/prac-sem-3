#include "db.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

static std::string read_json_arg_from_argv(int argc, char** argv, int idx) {
    // argv[idx] can be something like "base insert '{\"name\":\"Alice\"}'" in problem description,
    // but we expect CLI usage: ./no_sql_dbms dbname insert '{"name":"Alice"}'
    if (idx >= argc) return "";
    return std::string(argv[idx]);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage:\n"
                  << "  " << argv[0] << " <db_dir> insert '<json_doc>'\n"
                  << "  " << argv[0] << " <db_dir> find '<json_query>'\n"
                  << "  " << argv[0] << " <db_dir> delete '<json_query>'\n"
                  << "  " << argv[0] << " <db_dir> create_index <field>\n";
        return 1;
    }

    std::string dbdir = argv[1];
    std::string cmd = argv[2];

    Database db(dbdir);
    auto coll = db.open_collection("collection");

    try {
        if (cmd == "insert") {
            if (argc < 4) { std::cout << "Missing JSON document\n"; return 1; }
            std::string jstr = argv[3];
            // allow JSON passed across spaces by concatenating remaining args if first char is '{'
            if (jstr.size() && jstr.front()=='{') {
                // good
            } else {
                // try concat everything from 3
                std::stringstream ss;
                for (int i=3;i<argc;++i) {
                    if (i>3) ss << " ";
                    ss << argv[i];
                }
                jstr = ss.str();
            }
            json doc = json::parse(jstr);
            std::string id = coll->insert(doc);
            if (!id.empty()) std::cout << "Document inserted successfully. _id=" << id << "\n";
            else std::cout << "Insert failed\n";
            return 0;
        } else if (cmd == "find") {
            if (argc < 4) { std::cout << "Missing JSON query\n"; return 1; }
            std::stringstream ss;
            for (int i=3;i<argc;++i) {
                if (i>3) ss << " ";
                ss << argv[i];
            }
            std::string q = ss.str();
            json query = json::parse(q);
            auto res = coll->find(query);
            json out = json::array();
            for (auto &d : res) out.push_back(d);
            std::cout << out.dump(4) << "\n";
            return 0;
        } else if (cmd == "delete") {
            if (argc < 4) { std::cout << "Missing JSON query\n"; return 1; }
            std::stringstream ss;
            for (int i=3;i<argc;++i) {
                if (i>3) ss << " ";
                ss << argv[i];
            }
            std::string q = ss.str();
            json query = json::parse(q);
            int removed = coll->remove(query);
            std::cout << "Removed " << removed << " documents\n";
            return 0;
        } else if (cmd == "create_index") {
            if (argc < 4) { std::cout << "Missing field name\n"; return 1; }
            std::string field = argv[3];
            coll->create_index(field);
            return 0;
        } else {
            std::cout << "Unknown command: " << cmd << "\n";
            return 1;
        }
    } catch (const std::exception &ex) {
        std::cout << "Error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "Unknown error\n";
        return 1;
    }
}