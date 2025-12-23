#ifndef SERVER_H
#define SERVER_H

#include "db_manager.h"
#include "json_parser.h"
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

class DatabaseServer {
private:
    int port_;
    int serverSocket_;
    std::atomic<bool> running_;
    DatabaseManager dbManager_;
    
    void setNonBlocking(int sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    
    std::string readMessage(int clientSocket) {
        // Читаем длину сообщения (4 байта)
        uint32_t length;
        ssize_t bytesRead = recv(clientSocket, &length, sizeof(length), MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Client disconnected");
        }
        if (bytesRead != sizeof(length)) {
            throw std::runtime_error("Failed to read message length");
        }
        
        length = ntohl(length); // Преобразуем из network byte order
        
        if (length == 0 || length > 10 * 1024 * 1024) { // Максимум 10MB
            throw std::runtime_error("Invalid message length");
        }
        
        // Читаем само сообщение
        std::vector<char> buffer(length);
        bytesRead = recv(clientSocket, buffer.data(), length, MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Client disconnected");
        }
        if (bytesRead != static_cast<ssize_t>(length)) {
            throw std::runtime_error("Failed to read message");
        }
        
        return std::string(buffer.data(), length);
    }
    
    void sendMessage(int clientSocket, const std::string& message) {
        // Отправляем длину сообщения
        uint32_t length = htonl(static_cast<uint32_t>(message.length()));
        ssize_t bytesSent = send(clientSocket, &length, sizeof(length), 0);
        if (bytesSent != sizeof(length)) {
            throw std::runtime_error("Failed to send message length");
        }
        
        // Отправляем само сообщение
        bytesSent = send(clientSocket, message.c_str(), message.length(), 0);
        if (bytesSent != static_cast<ssize_t>(message.length())) {
            throw std::runtime_error("Failed to send message");
        }
    }
    
    JsonValue createResponse(const std::string& status, const std::string& message, 
                            const std::vector<JsonValue>& data = {}) {
        JsonValue response;
        response["status"] = JsonValue(status);
        response["message"] = JsonValue(message);
        response["data"] = JsonValue(data);
        response["count"] = JsonValue(static_cast<int>(data.size()));
        
        return response;
    }
    
    JsonValue createErrorResponse(const std::string& message) {
        return createResponse("error", message);
    }
    
    JsonValue createSuccessResponse(const std::string& message, 
                                   const std::vector<JsonValue>& data = {}) {
        return createResponse("success", message, data);
    }
    
    JsonValue handleRequest(const JsonValue& request) {
        try {
            if (!request.isObject()) {
                return createErrorResponse("Invalid request: must be an object");
            }
            
            if (!request.hasKey("database") || !request.hasKey("operation")) {
                return createErrorResponse("Missing required fields: database, operation");
            }
            
            std::string dbName;
            if (request["database"].isString()) {
                dbName = request["database"].asString();
            } else {
                return createErrorResponse("Invalid 'database' field: must be a string");
            }
            
            std::string operation;
            if (request["operation"].isString()) {
                operation = request["operation"].asString();
            } else {
                return createErrorResponse("Invalid 'operation' field: must be a string");
            }
            
            // Получаем коллекцию (по умолчанию "default")
            std::string collectionName = "default";
            if (request.hasKey("collection") && request["collection"].isString()) {
                collectionName = request["collection"].asString();
            }
            
            if (operation == "insert") {
                if (!request.hasKey("data")) {
                    return createErrorResponse("Missing 'data' field for insert operation");
                }
                
                JsonValue data = request["data"];
                if (data.isArray()) {
                    // Вставка нескольких документов
                    auto arr = data.asArray();
                    int count = 0;
                    dbManager_.executeWrite(dbName, collectionName, [&](Database& db) {
                        for (const auto& doc : arr) {
                            if (doc.isObject()) {
                                db.insert(doc);
                                count++;
                            }
                        }
                    });
                    return createSuccessResponse("Inserted " + std::to_string(count) + " document(s)");
                } else if (data.isObject()) {
                    // Вставка одного документа
                    dbManager_.executeWrite(dbName, collectionName, [&](Database& db) {
                        db.insert(data);
                    });
                    return createSuccessResponse("Document inserted successfully");
                } else {
                    return createErrorResponse("Invalid 'data' field: must be object or array");
                }
                
            } else if (operation == "find") {
                if (!request.hasKey("query")) {
                    return createErrorResponse("Missing 'query' field for find operation");
                }
                
                JsonValue query = request["query"];
                std::vector<JsonValue> results;
                
                dbManager_.executeRead(dbName, collectionName, [&](Database& db) {
                    results = db.find(query);
                });
                
                return createSuccessResponse("Found " + std::to_string(results.size()) + " document(s)", results);
                
            } else if (operation == "delete") {
                if (!request.hasKey("query")) {
                    return createErrorResponse("Missing 'query' field for delete operation");
                }
                
                JsonValue query = request["query"];
                int deleted = 0;
                
                dbManager_.executeWrite(dbName, collectionName, [&](Database& db) {
                    deleted = db.remove(query);
                });
                
                return createSuccessResponse("Deleted " + std::to_string(deleted) + " document(s)");
                
            } else if (operation == "create_index") {
                if (!request.hasKey("field")) {
                    return createErrorResponse("Missing 'field' field for create_index operation");
                }
                
                std::string field;
                if (request["field"].isString()) {
                    field = request["field"].asString();
                } else {
                    return createErrorResponse("Invalid 'field' field: must be a string");
                }
                dbManager_.executeWrite(dbName, collectionName, [&](Database& db) {
                    db.createIndex(field);
                });
                
                return createSuccessResponse("Index created on field: " + field);
                
            } else {
                return createErrorResponse("Unknown operation: " + operation);
            }
            
        } catch (const std::exception& e) {
            return createErrorResponse("Error: " + std::string(e.what()));
        }
    }
    
    void handleClient(int clientSocket) {
        try {
            while (running_) {
                std::string requestStr = readMessage(clientSocket);
                
                JsonParser parser;
                JsonValue request = parser.parse(requestStr);
                JsonValue response = handleRequest(request);
                
                std::string responseStr = response.toString();
                sendMessage(clientSocket, responseStr);
            }
        } catch (const std::exception& e) {
            // Клиент отключился или произошла ошибка
            // Можно логировать ошибку: std::cerr << "Client error: " << e.what() << "\n";
        }
        
        close(clientSocket);
    }
    
public:
    DatabaseServer(int port) : port_(port), serverSocket_(-1), running_(false) {}
    
    ~DatabaseServer() {
        stop();
    }
    
    bool start() {
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ < 0) {
            std::cerr << "Error creating socket\n";
            return false;
        }
        
        int opt = 1;
        if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting socket options\n";
            close(serverSocket_);
            return false;
        }
        
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Error binding socket to port " << port_ << "\n";
            close(serverSocket_);
            return false;
        }
        
        if (listen(serverSocket_, 10) < 0) {
            std::cerr << "Error listening on socket\n";
            close(serverSocket_);
            return false;
        }
        
        running_ = true;
        std::cout << "Database server started on port " << port_ << "\n";
        
        while (running_) {
            sockaddr_in clientAddress;
            socklen_t clientAddrLen = sizeof(clientAddress);
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddress, &clientAddrLen);
            
            if (clientSocket < 0) {
                if (running_) {
                    continue;
                }
                break;
            }
            
            std::cout << "Client connected from " << inet_ntoa(clientAddress.sin_addr) 
                      << ":" << ntohs(clientAddress.sin_port) << "\n";
            
            // Создаем поток для обработки клиента
            std::thread clientThread(&DatabaseServer::handleClient, this, clientSocket);
            clientThread.detach();
        }
        
        return true;
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            if (serverSocket_ >= 0) {
                close(serverSocket_);
            }
        }
    }
};

#endif // SERVER_H

