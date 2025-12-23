#include "database.h"
#include "json_parser.h"
#include <iostream>
#include <vector>
#include <string>

void printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  ./no_sql_dbms <database> insert '<json>'\n";
    std::cout << "  ./no_sql_dbms <database> find '<json>'\n";
    std::cout << "  ./no_sql_dbms <database> delete '<json>'\n";
    std::cout << "  ./no_sql_dbms <database> create_index <field>\n";
}

void printJson(const JsonValue& json, int indent = 0) {
    std::string indentStr(indent * 2, ' ');
    
    if (json.isObject()) {
        auto obj = json.asObject();
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << indentStr << "  \"" << key << "\": ";
            if (value.isObject() || value.isArray()) {
                std::cout << "\n";
                printJson(value, indent + 1);
            } else {
                std::cout << value.toString();
            }
        }
        std::cout << "\n" << indentStr << "}";
    } else if (json.isArray()) {
        auto arr = json.asArray();
        std::cout << "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) std::cout << ",\n";
            std::cout << indentStr << "  ";
            printJson(arr[i], indent + 1);
        }
        std::cout << "\n" << indentStr << "]";
    } else {
        std::cout << json.toString();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }
    
    std::string dbName = argv[1];
    std::string command = argv[2];
    
    // Используем "default" как имя коллекции
    std::string collectionName = "default";
    std::string dbPath = dbName;
    
    try {
        Database db(dbPath, collectionName);
        JsonParser parser;
        
        if (command == "insert") {
            if (argc < 4) {
                std::cerr << "Error: JSON document required for insert\n";
                return 1;
            }
            
            std::string jsonStr = argv[3];
            JsonValue doc = parser.parse(jsonStr);
            
            if (!doc.isObject()) {
                std::cerr << "Error: Document must be a JSON object\n";
                return 1;
            }
            
            db.insert(doc);
            std::cout << "Document inserted successfully.\n";
            
        } else if (command == "find") {
            if (argc < 4) {
                std::cerr << "Error: Query JSON required for find\n";
                return 1;
            }
            
            std::string queryStr = argv[3];
            JsonValue query = parser.parse(queryStr);
            
            if (!query.isObject()) {
                std::cerr << "Error: Query must be a JSON object\n";
                return 1;
            }
            
            std::vector<JsonValue> results = db.find(query);
            
            if (results.empty()) {
                std::cout << "No documents found.\n";
            } else {
                std::cout << "Found " << results.size() << " document(s):\n";
                for (size_t i = 0; i < results.size(); ++i) {
                    std::cout << "\nDocument " << (i + 1) << ":\n";
                    printJson(results[i]);
                    std::cout << "\n";
                }
            }
            
        } else if (command == "delete") {
            if (argc < 4) {
                std::cerr << "Error: Query JSON required for delete\n";
                return 1;
            }
            
            std::string queryStr = argv[3];
            JsonValue query = parser.parse(queryStr);
            
            if (!query.isObject()) {
                std::cerr << "Error: Query must be a JSON object\n";
                return 1;
            }
            
            int deleted = db.remove(query);
            std::cout << "Deleted " << deleted << " document(s).\n";
            
        } else if (command == "create_index") {
            if (argc < 4) {
                std::cerr << "Error: Field name required for create_index\n";
                return 1;
            }
            
            std::string field = argv[3];
            db.createIndex(field);
            std::cout << "Index created on field: " << field << "\n";
            
        } else {
            std::cerr << "Error: Unknown command: " << command << "\n";
            printUsage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

