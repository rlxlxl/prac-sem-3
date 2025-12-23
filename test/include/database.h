#ifndef DATABASE_H
#define DATABASE_H

#include "hashmap.h"
#include "json_parser.h"
#include "query_evaluator.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <ctime>

class Database {
private:
    std::string dbPath_;
    std::string collectionName_;
    HashMap<std::string, JsonValue> documents_;
    
    std::string generateId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::ostringstream oss;
        oss << std::hex;
        for (int i = 0; i < 24; ++i) {
            oss << dis(gen);
        }
        return oss.str();
    }
    
    std::string getCollectionPath() const {
        std::filesystem::path path(dbPath_);
        path /= collectionName_ + ".json";
        return path.string();
    }
    
    void saveCollection() {
        std::string filePath = getCollectionPath();
        std::ofstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filePath);
        }
        
        file << "{\n";
        auto items = documents_.items();
        for (size_t i = 0; i < items.size(); ++i) {
            file << "  \"" << items[i].first << "\": " << items[i].second.toString();
            if (i < items.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        file << "}\n";
        file.close();
    }
    
    void loadCollection() {
        std::string filePath = getCollectionPath();
        
        if (!std::filesystem::exists(filePath)) {
            documents_.clear();
            return;
        }
        
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for reading: " + filePath);
        }
        
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();
        
        if (content.empty() || content.find_first_not_of(" \t\n\r") == std::string::npos) {
            documents_.clear();
            return;
        }
        
        try {
            JsonParser parser;
            JsonValue root = parser.parse(content);
            
            if (!root.isObject()) {
                documents_.clear();
                return;
            }
            
            documents_.clear();
            auto obj = root.asObject();
            for (const auto& [id, doc] : obj) {
                documents_.put(id, doc);
            }
        } catch (...) {
            documents_.clear();
        }
    }
    
public:
    Database(const std::string& dbPath, const std::string& collectionName) 
        : dbPath_(dbPath), collectionName_(collectionName) {
        // Создаем директорию базы данных, если её нет
        if (!std::filesystem::exists(dbPath_)) {
            std::filesystem::create_directories(dbPath_);
        }
        
        loadCollection();
    }
    
    void insert(const JsonValue& document) {
        JsonValue doc = document;
        std::string id = generateId();
        doc["_id"] = JsonValue(id);
        documents_.put(id, doc);
        saveCollection();
    }
    
    std::vector<JsonValue> find(const JsonValue& query) {
        std::vector<JsonValue> results;
        auto items = documents_.items();
        
        for (const auto& [id, doc] : items) {
            if (QueryEvaluator::matches(doc, query)) {
                results.push_back(doc);
            }
        }
        
        return results;
    }
    
    int remove(const JsonValue& query) {
        std::vector<std::string> idsToRemove;
        auto items = documents_.items();
        
        for (const auto& [id, doc] : items) {
            if (QueryEvaluator::matches(doc, query)) {
                idsToRemove.push_back(id);
            }
        }
        
        for (const std::string& id : idsToRemove) {
            documents_.remove(id);
        }
        
        if (!idsToRemove.empty()) {
            saveCollection();
        }
        
        return idsToRemove.size();
    }
    
    void createIndex(const std::string& field) {
        // Базовая реализация индекса (можно расширить)
        // Для задания со звездочкой - просто сохраняем информацию об индексе
        std::string indexPath = std::filesystem::path(dbPath_) / (collectionName_ + "_" + field + "_index.json");
        std::ofstream file(indexPath);
        if (file.is_open()) {
            file << "{\"field\": \"" << field << "\", \"collection\": \"" << collectionName_ << "\"}\n";
            file.close();
        }
    }
};

#endif // DATABASE_H

