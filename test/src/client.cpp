#include "json_parser.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>
#include <cstring>

class DatabaseClient {
private:
    std::string host_;
    int port_;
    std::string database_;
    int socket_;
    
    bool connect() {
        struct addrinfo hints, *result, *rp;
        std::memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        std::string portStr = std::to_string(port_);
        int status = getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &result);
        if (status != 0) {
            std::cerr << "Error resolving address " << host_ << ": " << gai_strerror(status) << "\n";
            return false;
        }
        
        // Пробуем подключиться к первому доступному адресу
        for (rp = result; rp != nullptr; rp = rp->ai_next) {
            socket_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_ < 0) {
                continue;
            }
            
            if (::connect(socket_, rp->ai_addr, rp->ai_addrlen) == 0) {
                break; // Успешное подключение
            }
            
            close(socket_);
            socket_ = -1;
        }
        
        freeaddrinfo(result);
        
        if (socket_ < 0) {
            std::cerr << "Connection failed to " << host_ << ":" << port_ << "\n";
            return false;
        }
        
        return true;
    }
    
    std::string readMessage() {
        uint32_t length;
        ssize_t bytesRead = recv(socket_, &length, sizeof(length), MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Server disconnected");
        }
        if (bytesRead != sizeof(length)) {
            throw std::runtime_error("Failed to read message length");
        }
        
        length = ntohl(length);
        
        if (length == 0 || length > 10 * 1024 * 1024) { // Максимум 10MB
            throw std::runtime_error("Invalid message length");
        }
        
        std::vector<char> buffer(length);
        bytesRead = recv(socket_, buffer.data(), length, MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Server disconnected");
        }
        if (bytesRead != static_cast<ssize_t>(length)) {
            throw std::runtime_error("Failed to read message");
        }
        
        return std::string(buffer.data(), length);
    }
    
    void sendMessage(const std::string& message) {
        uint32_t length = htonl(static_cast<uint32_t>(message.length()));
        ssize_t bytesSent = send(socket_, &length, sizeof(length), 0);
        if (bytesSent != sizeof(length)) {
            throw std::runtime_error("Failed to send message length");
        }
        
        bytesSent = send(socket_, message.c_str(), message.length(), 0);
        if (bytesSent != static_cast<ssize_t>(message.length())) {
            throw std::runtime_error("Failed to send message");
        }
    }
    
    JsonValue executeRequest(const JsonValue& request) {
        std::string requestStr = request.toString();
        sendMessage(requestStr);
        
        std::string responseStr = readMessage();
        JsonParser parser;
        return parser.parse(responseStr);
    }
    
    void printResponse(const JsonValue& response) {
        if (!response.isObject()) {
            std::cout << "Invalid response format\n";
            return;
        }
        
        std::string status = response["status"].asString();
        std::string message = response["message"].asString();
        
        std::cout << "[" << status << "] " << message << "\n";
        
        if (response.hasKey("data") && response["data"].isArray()) {
            auto data = response["data"].asArray();
            if (!data.empty()) {
                std::cout << "\nDocuments:\n";
                for (size_t i = 0; i < data.size(); ++i) {
                    std::cout << "\nDocument " << (i + 1) << ":\n";
                    printJson(data[i]);
                    std::cout << "\n";
                }
            }
        }
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
    
    std::vector<std::string> parseCommand(const std::string& line) {
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        
        // Парсим первые два токена (команда и коллекция)
        if (iss >> token) {
            tokens.push_back(token);
        }
        if (iss >> token) {
            tokens.push_back(token);
        }
        
        // Остальное - это JSON, читаем до конца строки
        std::string jsonPart;
        std::getline(iss, jsonPart);
        // Убираем ведущие пробелы
        jsonPart.erase(0, jsonPart.find_first_not_of(" \t"));
        if (!jsonPart.empty()) {
            tokens.push_back(jsonPart);
        }
        
        return tokens;
    }
    
    JsonValue parseJsonFromString(const std::string& str) {
        JsonParser parser;
        return parser.parse(str);
    }
    
    JsonValue buildRequest(const std::string& operation, const std::string& collection,
                          const JsonValue& data = JsonValue(), const JsonValue& query = JsonValue()) {
        JsonValue request;
        request["database"] = JsonValue(database_);
        request["operation"] = JsonValue(operation);
        request["collection"] = JsonValue(collection);
        
        if (!data.isNull()) {
            request["data"] = data;
        }
        if (!query.isNull()) {
            request["query"] = query;
        }
        
        return request;
    }
    
public:
    DatabaseClient(const std::string& host, int port, const std::string& database)
        : host_(host), port_(port), database_(database), socket_(-1) {}
    
    ~DatabaseClient() {
        if (socket_ >= 0) {
            close(socket_);
        }
    }
    
    bool connectToServer() {
        return connect();
    }
    
    void runInteractive() {
        std::cout << "Connected to database server at " << host_ << ":" << port_ << "\n";
        std::cout << "Database: " << database_ << "\n";
        std::cout << "Enter commands (INSERT, FIND, DELETE, CREATE_INDEX) or 'exit' to quit\n";
        std::cout << "Example: INSERT users {\"name\": \"Alice\", \"age\": 25}\n";
        std::cout << "> ";
        
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                std::cout << "> ";
                continue;
            }
            
            if (line == "exit" || line == "quit") {
                break;
            }
            
            try {
                std::vector<std::string> tokens = parseCommand(line);
                if (tokens.empty()) {
                    std::cout << "> ";
                    continue;
                }
                
                std::string command = tokens[0];
                std::transform(command.begin(), command.end(), command.begin(), ::toupper);
                
                if (command == "INSERT") {
                    if (tokens.size() < 3) {
                        std::cout << "Usage: INSERT <collection> <json>\n";
                        std::cout << "> ";
                        continue;
                    }
                    
                    std::string collection = tokens[1];
                    std::string jsonStr = tokens[2];
                    
                    JsonValue doc = parseJsonFromString(jsonStr);
                    JsonValue request = buildRequest("insert", collection, doc);
                    JsonValue response = executeRequest(request);
                    printResponse(response);
                    
                } else if (command == "FIND") {
                    if (tokens.size() < 3) {
                        std::cout << "Usage: FIND <collection> <json_query>\n";
                        std::cout << "> ";
                        continue;
                    }
                    
                    std::string collection = tokens[1];
                    std::string jsonStr = tokens[2];
                    
                    JsonValue query = parseJsonFromString(jsonStr);
                    JsonValue request = buildRequest("find", collection, JsonValue(), query);
                    JsonValue response = executeRequest(request);
                    printResponse(response);
                    
                } else if (command == "DELETE") {
                    if (tokens.size() < 3) {
                        std::cout << "Usage: DELETE <collection> <json_query>\n";
                        std::cout << "> ";
                        continue;
                    }
                    
                    std::string collection = tokens[1];
                    std::string jsonStr = tokens[2];
                    
                    JsonValue query = parseJsonFromString(jsonStr);
                    JsonValue request = buildRequest("delete", collection, JsonValue(), query);
                    JsonValue response = executeRequest(request);
                    printResponse(response);
                    
                } else if (command == "CREATE_INDEX") {
                    if (tokens.size() < 3) {
                        std::cout << "Usage: CREATE_INDEX <collection> <field>\n";
                        std::cout << "> ";
                        continue;
                    }
                    
                    std::string collection = tokens[1];
                    std::string field = tokens[2];
                    
                    JsonValue request;
                    request["database"] = JsonValue(database_);
                    request["operation"] = JsonValue("create_index");
                    request["collection"] = JsonValue(collection);
                    request["field"] = JsonValue(field);
                    
                    JsonValue response = executeRequest(request);
                    printResponse(response);
                    
                } else {
                    std::cout << "Unknown command: " << command << "\n";
                    std::cout << "Available commands: INSERT, FIND, DELETE, CREATE_INDEX\n";
                }
                
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
            
            std::cout << "> ";
        }
    }
    
    void executeSingleCommand(const std::string& command, const std::string& collection,
                              const std::string& jsonData) {
        try {
            JsonValue request;
            std::string op = command;
            std::transform(op.begin(), op.end(), op.begin(), ::toupper);
            
            if (op == "INSERT") {
                JsonValue doc = parseJsonFromString(jsonData);
                request = buildRequest("insert", collection, doc);
            } else if (op == "FIND") {
                JsonValue query = parseJsonFromString(jsonData);
                request = buildRequest("find", collection, JsonValue(), query);
            } else if (op == "DELETE") {
                JsonValue query = parseJsonFromString(jsonData);
                request = buildRequest("delete", collection, JsonValue(), query);
            } else {
                std::cerr << "Unknown command: " << command << "\n";
                return;
            }
            
            JsonValue response = executeRequest(request);
            printResponse(response);
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --host <host> --port <port> --database <database> [--command <cmd>]\n";
    std::cout << "Options:\n";
    std::cout << "  --host <host>       Server hostname or IP address\n";
    std::cout << "  --port <port>       Server port (default: 8080)\n";
    std::cout << "  --database <db>     Database name\n";
    std::cout << "  --command <cmd>     Single command to execute (optional)\n";
    std::cout << "\n";
    std::cout << "If --command is not specified, runs in interactive mode.\n";
    std::cout << "\n";
    std::cout << "Commands:\n";
    std::cout << "  INSERT <collection> <json>    Insert document\n";
    std::cout << "  FIND <collection> <json>      Find documents\n";
    std::cout << "  DELETE <collection> <json>    Delete documents\n";
}

int main(int argc, char* argv[]) {
    std::string host = "localhost";
    int port = 8080;
    std::string database;
    std::string command;
    
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"database", required_argument, 0, 'd'},
        {"command", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "h:p:d:c:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'd':
                database = optarg;
                break;
            case 'c':
                command = optarg;
                break;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    if (database.empty()) {
        std::cerr << "Error: --database is required\n";
        printUsage(argv[0]);
        return 1;
    }
    
    DatabaseClient client(host, port, database);
    
    if (!client.connectToServer()) {
        return 1;
    }
    
    if (!command.empty()) {
        // Режим одного запроса (упрощенный)
        std::cout << "Single command mode not fully implemented. Use interactive mode.\n";
        client.runInteractive();
    } else {
        client.runInteractive();
    }
    
    return 0;
}

