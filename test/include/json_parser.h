#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <sstream>
#include <stdexcept>

class JsonValue {
public:
    using Value = std::variant<std::nullptr_t, bool, int, double, std::string, 
                               std::vector<JsonValue>, std::map<std::string, JsonValue>>;
    
private:
    Value value_;
    
public:
    JsonValue() : value_(nullptr) {}
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool b) : value_(b) {}
    JsonValue(int i) : value_(i) {}
    JsonValue(double d) : value_(d) {}
    JsonValue(const std::string& s) : value_(s) {}
    JsonValue(const std::vector<JsonValue>& arr) : value_(arr) {}
    JsonValue(const std::map<std::string, JsonValue>& obj) : value_(obj) {}
    
    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isInt() const { return std::holds_alternative<int>(value_) || std::holds_alternative<double>(value_); }
    bool isDouble() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<std::vector<JsonValue>>(value_); }
    bool isObject() const { return std::holds_alternative<std::map<std::string, JsonValue>>(value_); }
    
    bool asBool() const { 
        if (!isBool()) {
            throw std::runtime_error("Value is not a boolean");
        }
        return std::get<bool>(value_); 
    }
    int asInt() const { 
        if (std::holds_alternative<int>(value_)) return std::get<int>(value_);
        if (std::holds_alternative<double>(value_)) return static_cast<int>(std::get<double>(value_));
        throw std::runtime_error("Value is not a number");
    }
    double asDouble() const {
        if (std::holds_alternative<double>(value_)) return std::get<double>(value_);
        if (std::holds_alternative<int>(value_)) return static_cast<double>(std::get<int>(value_));
        throw std::runtime_error("Value is not a number");
    }
    std::string asString() const { 
        if (!isString()) {
            throw std::runtime_error("Value is not a string");
        }
        return std::get<std::string>(value_); 
    }
    std::vector<JsonValue> asArray() const { 
        if (!isArray()) {
            throw std::runtime_error("Value is not an array");
        }
        return std::get<std::vector<JsonValue>>(value_); 
    }
    std::map<std::string, JsonValue> asObject() const { 
        if (!isObject()) {
            throw std::runtime_error("Value is not an object");
        }
        return std::get<std::map<std::string, JsonValue>>(value_); 
    }
    
    const Value& getValue() const { return value_; }
    Value& getValue() { return value_; }
    
    JsonValue& operator[](const std::string& key) {
        if (!isObject()) {
            value_ = std::map<std::string, JsonValue>();
        }
        return std::get<std::map<std::string, JsonValue>>(value_)[key];
    }
    
    const JsonValue& operator[](const std::string& key) const {
        return std::get<std::map<std::string, JsonValue>>(value_).at(key);
    }
    
    bool hasKey(const std::string& key) const {
        if (!isObject()) return false;
        return std::get<std::map<std::string, JsonValue>>(value_).count(key) > 0;
    }
    
    std::string toString() const;
};

class JsonParser {
private:
    std::string input_;
    size_t pos_;
    
    void skipWhitespace() {
        while (pos_ < input_.length() && std::isspace(input_[pos_])) {
            pos_++;
        }
    }
    
    std::string parseString() {
        if (input_[pos_] != '"') {
            throw std::runtime_error("Expected string");
        }
        pos_++;
        std::string result;
        while (pos_ < input_.length() && input_[pos_] != '"') {
            if (input_[pos_] == '\\') {
                pos_++;
                if (pos_ >= input_.length()) break;
                switch (input_[pos_]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += input_[pos_]; break;
                }
            } else {
                result += input_[pos_];
            }
            pos_++;
        }
        if (pos_ >= input_.length() || input_[pos_] != '"') {
            throw std::runtime_error("Unterminated string");
        }
        pos_++;
        return result;
    }
    
    JsonValue parseNumber() {
        size_t start = pos_;
        bool isFloat = false;
        
        if (input_[pos_] == '-') pos_++;
        while (pos_ < input_.length() && std::isdigit(input_[pos_])) pos_++;
        if (pos_ < input_.length() && input_[pos_] == '.') {
            isFloat = true;
            pos_++;
            while (pos_ < input_.length() && std::isdigit(input_[pos_])) pos_++;
        }
        
        std::string numStr = input_.substr(start, pos_ - start);
        if (isFloat) {
            return JsonValue(std::stod(numStr));
        } else {
            return JsonValue(std::stoi(numStr));
        }
    }
    
    JsonValue parseValue() {
        skipWhitespace();
        if (pos_ >= input_.length()) {
            throw std::runtime_error("Unexpected end of input");
        }
        
        char c = input_[pos_];
        if (c == '{') {
            return parseObject();
        } else if (c == '[') {
            return parseArray();
        } else if (c == '"') {
            return JsonValue(parseString());
        } else if (c == '-' || std::isdigit(c)) {
            return parseNumber();
        } else if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return JsonValue(true);
        } else if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return JsonValue(false);
        } else if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue(nullptr);
        } else {
            throw std::runtime_error("Unexpected character: " + std::string(1, c));
        }
    }
    
    JsonValue parseObject() {
        if (input_[pos_] != '{') {
            throw std::runtime_error("Expected object");
        }
        pos_++;
        skipWhitespace();
        
        std::map<std::string, JsonValue> obj;
        
        if (input_[pos_] == '}') {
            pos_++;
            return JsonValue(obj);
        }
        
        while (true) {
            skipWhitespace();
            std::string key = parseString();
            skipWhitespace();
            if (input_[pos_] != ':') {
                throw std::runtime_error("Expected colon");
            }
            pos_++;
            JsonValue value = parseValue();
            obj[key] = value;
            
            skipWhitespace();
            if (input_[pos_] == '}') {
                pos_++;
                break;
            } else if (input_[pos_] == ',') {
                pos_++;
            } else {
                throw std::runtime_error("Expected comma or closing brace");
            }
        }
        
        return JsonValue(obj);
    }
    
    JsonValue parseArray() {
        if (input_[pos_] != '[') {
            throw std::runtime_error("Expected array");
        }
        pos_++;
        skipWhitespace();
        
        std::vector<JsonValue> arr;
        
        if (input_[pos_] == ']') {
            pos_++;
            return JsonValue(arr);
        }
        
        while (true) {
            arr.push_back(parseValue());
            skipWhitespace();
            if (input_[pos_] == ']') {
                pos_++;
                break;
            } else if (input_[pos_] == ',') {
                pos_++;
            } else {
                throw std::runtime_error("Expected comma or closing bracket");
            }
        }
        
        return JsonValue(arr);
    }
    
public:
    JsonValue parse(const std::string& json) {
        input_ = json;
        pos_ = 0;
        skipWhitespace();
        JsonValue result = parseValue();
        skipWhitespace();
        if (pos_ < input_.length()) {
            throw std::runtime_error("Unexpected characters after JSON");
        }
        return result;
    }
};

std::string JsonValue::toString() const {
    if (isNull()) return "null";
    if (isBool()) return asBool() ? "true" : "false";
    if (isInt()) return std::to_string(asInt());
    if (isDouble()) {
        std::ostringstream oss;
        oss << asDouble();
        return oss.str();
    }
    if (isString()) {
        std::string s = asString();
        std::string result = "\"";
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\t') result += "\\t";
            else if (c == '\r') result += "\\r";
            else result += c;
        }
        result += "\"";
        return result;
    }
    if (isArray()) {
        std::string result = "[";
        auto arr = asArray();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) result += ", ";
            result += arr[i].toString();
        }
        result += "]";
        return result;
    }
    if (isObject()) {
        std::string result = "{";
        auto obj = asObject();
        bool first = true;
        for (const auto& [key, value] : obj) {
            if (!first) result += ", ";
            first = false;
            result += "\"" + key + "\": " + value.toString();
        }
        result += "}";
        return result;
    }
    return "null";
}

#endif // JSON_PARSER_H

