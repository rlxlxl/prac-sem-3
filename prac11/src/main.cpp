#include "db.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace std;
using json = nlohmann::json;

static string readJsonArgFromArgv(int argc, char** argv, int index) {
    if (index >= argc) return "";
    return string(argv[index]);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Usage:\n"
                  << "  " << argv[0] << " <db_dir> insert '<json_doc>'\n"
                  << "  " << argv[0] << " <db_dir> find '<json_query>'\n"
                  << "  " << argv[0] << " <db_dir> delete '<json_query>'\n"
                  << "  " << argv[0] << " <db_dir> createIndex <field>\n";
        return 1;
    }

    string dbDir = argv[1];
    string command = argv[2];

    Database db(dbDir);
    auto collection = db.openCollection("collection");

    try {
        if (command == "insert") {
            if (argc < 4) { 
                cout << "Missing JSON document\n"; 
                return 1; 
            }

            string jsonString = argv[3];
            if (jsonString.empty() || jsonString.front() != '{') {
                stringstream ss;
                for (int i = 3; i < argc; ++i) {
                    if (i > 3) ss << " ";
                    ss << argv[i];
                }
                jsonString = ss.str();
            }

            json document = json::parse(jsonString);
            string id = collection->insert(document);
            if (!id.empty()) 
                cout << "Document inserted successfully. _id=" << id << "\n";
            else 
                cout << "Insert failed\n";

            return 0;

        } else if (command == "find") {
            if (argc < 4) { 
                cout << "Missing JSON query\n"; 
                return 1; 
            }

            stringstream ss;
            for (int i = 3; i < argc; ++i) {
                if (i > 3) ss << " ";
                ss << argv[i];
            }

            string queryString = ss.str();
            json query = json::parse(queryString);
            auto results = collection->find(query);

            json output = json::array();
            for (auto& doc : results) {
                output.push_back(doc);
            }

            cout << output.dump(4) << "\n";
            return 0;

        } else if (command == "delete") {
            if (argc < 4) { 
                cout << "Missing JSON query\n"; 
                return 1; 
            }

            stringstream ss;
            for (int i = 3; i < argc; ++i) {
                if (i > 3) ss << " ";
                ss << argv[i];
            }

            string queryString = ss.str();
            json query = json::parse(queryString);
            int removedCount = collection->remove(query);

            cout << "Removed " << removedCount << " document(s)\n";
            return 0;

        } else if (command == "createIndex") {
            if (argc < 4) { 
                cout << "Missing field name\n"; 
                return 1; 
            }

            string fieldName = argv[3];
            collection->createIndex(fieldName);
            return 0;

        } else {
            cout << "Unknown command: " << command << "\n";
            return 1;
        }

    } catch (const exception& ex) {
        cout << "Error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        cout << "Unknown error\n";
        return 1;
    }
}
