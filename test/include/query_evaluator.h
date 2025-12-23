#ifndef QUERY_EVALUATOR_H
#define QUERY_EVALUATOR_H

#include "json_parser.h"
#include <regex>
#include <algorithm>
#include <cmath>

class QueryEvaluator {
private:
    static bool matchesPattern(const std::string& text, const std::string& pattern) {
        // Преобразуем SQL-like pattern в regex
        std::string regexPattern;
        for (char c : pattern) {
            if (c == '%') {
                regexPattern += ".*";
            } else if (c == '_') {
                regexPattern += ".";
            } else if (c == '.' || c == '^' || c == '$' || c == '*' || c == '+' || 
                       c == '?' || c == '(' || c == ')' || c == '[' || c == ']' || 
                       c == '{' || c == '}' || c == '|' || c == '\\') {
                regexPattern += "\\";
                regexPattern += c;
            } else {
                regexPattern += c;
            }
        }
        
        std::regex regex(regexPattern, std::regex_constants::icase);
        return std::regex_match(text, regex);
    }
    
    static bool evaluateOperator(const JsonValue& docValue, const std::string& op, const JsonValue& queryValue) {
        if (op == "$eq" || op == "") {
            if (docValue.isString() && queryValue.isString()) {
                return docValue.asString() == queryValue.asString();
            } else if (docValue.isInt() && queryValue.isInt()) {
                return docValue.asInt() == queryValue.asInt();
            } else if (docValue.isDouble() && queryValue.isDouble()) {
                return std::abs(docValue.asDouble() - queryValue.asDouble()) < 1e-9;
            }
            return false;
        } else if (op == "$gt") {
            if (docValue.isInt() && queryValue.isInt()) {
                return docValue.asInt() > queryValue.asInt();
            } else if (docValue.isDouble() || queryValue.isDouble()) {
                return docValue.asDouble() > queryValue.asDouble();
            }
            return false;
        } else if (op == "$lt") {
            if (docValue.isInt() && queryValue.isInt()) {
                return docValue.asInt() < queryValue.asInt();
            } else if (docValue.isDouble() || queryValue.isDouble()) {
                return docValue.asDouble() < queryValue.asDouble();
            }
            return false;
        } else if (op == "$like") {
            if (docValue.isString() && queryValue.isString()) {
                return matchesPattern(docValue.asString(), queryValue.asString());
            }
            return false;
        } else if (op == "$in") {
            if (!queryValue.isArray()) return false;
            auto arr = queryValue.asArray();
            for (const auto& item : arr) {
                if (evaluateOperator(docValue, "$eq", item)) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }
    
    static bool evaluateCondition(const JsonValue& doc, const std::string& field, const JsonValue& condition) {
        if (!doc.hasKey(field)) {
            return false;
        }
        
        JsonValue docValue = doc[field];
        
        // Если condition - это объект с операторами
        if (condition.isObject()) {
            auto condObj = condition.asObject();
            
            // Проверяем операторы сравнения
            for (const auto& [op, value] : condObj) {
                if (op == "$eq" || op == "$gt" || op == "$lt" || op == "$like" || op == "$in") {
                    return evaluateOperator(docValue, op, value);
                }
            }
            
            // Если нет операторов, проверяем как обычное равенство
            return false;
        } else {
            // Простое равенство
            return evaluateOperator(docValue, "$eq", condition);
        }
    }
    
    static bool evaluateQuery(const JsonValue& doc, const JsonValue& query) {
        if (!query.isObject()) {
            return false;
        }
        
        auto queryObj = query.asObject();
        
        // Проверяем логические операторы
        if (queryObj.count("$or") > 0) {
            JsonValue orCondition = queryObj.at("$or");
            if (orCondition.isArray()) {
                auto orArr = orCondition.asArray();
                for (const auto& condition : orArr) {
                    if (evaluateQuery(doc, condition)) {
                        return true;
                    }
                }
                return false;
            }
        }
        
        if (queryObj.count("$and") > 0) {
            JsonValue andCondition = queryObj.at("$and");
            if (andCondition.isArray()) {
                auto andArr = andCondition.asArray();
                for (const auto& condition : andArr) {
                    if (!evaluateQuery(doc, condition)) {
                        return false;
                    }
                }
                return true;
            }
        }
        
        // Обрабатываем обычные поля (неявный AND)
        bool allMatch = true;
        for (const auto& [field, condition] : queryObj) {
            // Пропускаем специальные операторы
            if (field == "$or" || field == "$and") {
                continue;
            }
            
            if (!evaluateCondition(doc, field, condition)) {
                allMatch = false;
                break;
            }
        }
        
        return allMatch;
    }
    
public:
    static bool matches(const JsonValue& doc, const JsonValue& query) {
        return evaluateQuery(doc, query);
    }
};

#endif // QUERY_EVALUATOR_H

