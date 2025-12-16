#include "server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <signal.h>
#include <errno.h>
#include <algorithm>
#include <cctype>

DatabaseServer::DatabaseServer(const string& dbDir, int port)
    : dbDir_(dbDir), port_(port), serverSocket_(-1), running_(false)
{
}

DatabaseServer::~DatabaseServer() {
    stop();
}

shared_ptr<mutex> DatabaseServer::getDatabaseMutex(const string& dbName) {
    lock_guard<mutex> lock(mutexMapMutex_);
    
    if (dbMutexes_.find(dbName) == dbMutexes_.end()) {
        dbMutexes_[dbName] = make_shared<mutex>();
    }
    
    return dbMutexes_[dbName];
}

string DatabaseServer::readMessage(int socket) {
    // Читаем длину сообщения (4 байта)
    uint32_t length;
    ssize_t bytesRead = recv(socket, &length, sizeof(length), MSG_WAITALL);
    
    if (bytesRead != sizeof(length) || bytesRead <= 0) {
        return "";
    }
    
    // Конвертируем из network byte order
    length = ntohl(length);
    
    if (length == 0 || length > 1024 * 1024) { // Максимум 1MB
        return "";
    }
    
    // Читаем само сообщение
    string message(length, '\0');
    bytesRead = recv(socket, &message[0], length, MSG_WAITALL);
    
    if (bytesRead != static_cast<ssize_t>(length)) {
        return "";
    }
    
    return message;
}

void DatabaseServer::sendMessage(int socket, const string& message) {
    // Отправляем длину сообщения (4 байта)
    uint32_t length = htonl(message.length());
    send(socket, &length, sizeof(length), 0);
    
    // Отправляем само сообщение
    send(socket, message.c_str(), message.length(), 0);
}

json DatabaseServer::parseRequest(const string& message) {
    try {
        return json::parse(message);
    } catch (const exception& e) {
        return json();
    }
}

json DatabaseServer::executeInsert(const string& dbName, const string& collectionName, const json& data) {
    json response;
    
    try {
        Database db(dbDir_ + "/" + dbName);
        auto collection = db.openCollection(collectionName);
        
        int insertedCount = 0;
        if (data.is_array()) {
            for (const auto& doc : data) {
                string id = collection->insert(doc);
                if (!id.empty()) {
                    insertedCount++;
                }
            }
        } else {
            string id = collection->insert(data);
            if (!id.empty()) {
                insertedCount = 1;
            }
        }
        
        response["status"] = "success";
        response["message"] = "Inserted " + to_string(insertedCount) + " document(s)";
        response["count"] = insertedCount;
        response["data"] = json::array();
        
    } catch (const exception& e) {
        response["status"] = "error";
        response["message"] = string("Insert failed: ") + e.what();
        response["count"] = 0;
        response["data"] = json::array();
    }
    
    return response;
}

json DatabaseServer::executeFind(const string& dbName, const string& collectionName, const json& query) {
    json response;
    
    try {
        Database db(dbDir_ + "/" + dbName);
        auto collection = db.openCollection(collectionName);
        
        auto results = collection->find(query);
        
        response["status"] = "success";
        response["message"] = "Fetched " + to_string(results.size()) + " doc(s) from " + dbName;
        response["data"] = results;
        response["count"] = results.size();
        
    } catch (const exception& e) {
        response["status"] = "error";
        response["message"] = string("Find failed: ") + e.what();
        response["count"] = 0;
        response["data"] = json::array();
    }
    
    return response;
}

json DatabaseServer::executeDelete(const string& dbName, const string& collectionName, const json& query) {
    json response;
    
    try {
        Database db(dbDir_ + "/" + dbName);
        auto collection = db.openCollection(collectionName);
        
        int removedCount = collection->remove(query);
        
        response["status"] = "success";
        response["message"] = "Removed " + to_string(removedCount) + " document(s)";
        response["count"] = removedCount;
        response["data"] = json::array();
        
    } catch (const exception& e) {
        response["status"] = "error";
        response["message"] = string("Delete failed: ") + e.what();
        response["count"] = 0;
        response["data"] = json::array();
    }
    
    return response;
}

json DatabaseServer::processRequest(const json& request) {
    json response;
    
    // Проверка корректности запроса
    if (!request.contains("database") || !request.contains("operation")) {
        response["status"] = "error";
        response["message"] = "Invalid request: missing 'database' or 'operation' field";
        response["count"] = 0;
        response["data"] = json::array();
        return response;
    }
    
    string dbName = request["database"].get<string>();
    string operation = request["operation"].get<string>();
    
    // Преобразуем операцию в нижний регистр для сравнения
    string operationLower = operation;
    transform(operationLower.begin(), operationLower.end(), operationLower.begin(), ::tolower);
    
    // Извлекаем имя коллекции из запроса (если есть)
    string collectionName = "collection"; // По умолчанию
    if (request.contains("collection")) {
        collectionName = request["collection"].get<string>();
    }
    
    // Получаем мьютекс для базы данных
    auto dbMutex = getDatabaseMutex(dbName);
    
    // Для операций записи нужна блокировка
    bool needsLock = (operationLower == "insert" || operationLower == "delete");
    
    if (needsLock) {
        dbMutex->lock();
    }
    
    try {
        if (operationLower == "insert") {
            if (!request.contains("data")) {
                response["status"] = "error";
                response["message"] = "Insert operation requires 'data' field";
                response["count"] = 0;
                response["data"] = json::array();
            } else {
                response = executeInsert(dbName, collectionName, request["data"]);
            }
            
        } else if (operationLower == "find") {
            json query = request.contains("query") ? request["query"] : json::object();
            response = executeFind(dbName, collectionName, query);
            
        } else if (operationLower == "delete") {
            if (!request.contains("query")) {
                response["status"] = "error";
                response["message"] = "Delete operation requires 'query' field";
                response["count"] = 0;
                response["data"] = json::array();
            } else {
                response = executeDelete(dbName, collectionName, request["query"]);
            }
            
        } else {
            response["status"] = "error";
            response["message"] = "Unknown operation: " + operation + " (supported: insert, find, delete)";
            response["count"] = 0;
            response["data"] = json::array();
        }
    } catch (const exception& e) {
        response["status"] = "error";
        response["message"] = string("Operation failed: ") + e.what();
        response["count"] = 0;
        response["data"] = json::array();
    }
    
    if (needsLock) {
        dbMutex->unlock();
    }
    
    return response;
}

void DatabaseServer::handleClient(int clientSocket) {
    while (running_) {
        string message = readMessage(clientSocket);
        
        if (message.empty()) {
            break; // Соединение закрыто или ошибка
        }
        
        json request = parseRequest(message);
        if (request.is_null()) {
            json errorResponse;
            errorResponse["status"] = "error";
            errorResponse["message"] = "Invalid JSON in request";
            errorResponse["count"] = 0;
            errorResponse["data"] = json::array();
            sendMessage(clientSocket, errorResponse.dump());
            continue;
        }
        
        json response = processRequest(request);
        sendMessage(clientSocket, response.dump());
    }
    
    close(clientSocket);
}

void DatabaseServer::start() {
    // Создаем сокет
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        cerr << "Error creating socket\n";
        return;
    }
    
    // Устанавливаем опцию SO_REUSEADDR
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Настраиваем адрес
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    // Привязываем сокет
    if (::bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding socket on port " << port_ << ": " << strerror(errno) << "\n";
        cerr << "Port may be already in use. Try a different port or stop the process using this port.\n";
        close(serverSocket_);
        return;
    }
    
    // Слушаем подключения
    if (listen(serverSocket_, 10) < 0) {
        cerr << "Error listening on socket\n";
        close(serverSocket_);
        return;
    }
    
    running_ = true;
    cout << "Server started on port " << port_ << "\n";
    cout << "Database directory: " << dbDir_ << "\n";
    
    // Основной цикл принятия подключений
    while (running_) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (running_) {
                cerr << "Error accepting connection\n";
            }
            continue;
        }
        
        cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr) 
             << ":" << ntohs(clientAddr.sin_port) << "\n";
        
        // Запускаем обработку клиента в отдельном потоке
        thread clientThread(&DatabaseServer::handleClient, this, clientSocket);
        clientThread.detach();
    }
    
    close(serverSocket_);
}

void DatabaseServer::stop() {
    running_ = false;
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
}

