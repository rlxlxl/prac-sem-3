#pragma once
#include "db.h"
#include "json.hpp"
#include <string>
#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;
using json = nlohmann::json;

class DatabaseServer {
public:
    DatabaseServer(const string& dbDir, int port);
    ~DatabaseServer();
    
    void start();
    void stop();
    
private:
    string dbDir_;
    int port_;
    int serverSocket_;
    atomic<bool> running_;
    
    // Мьютексы для каждой базы данных
    map<string, shared_ptr<mutex>> dbMutexes_;
    mutex mutexMapMutex_; // Защищает map мьютексов
    
    // Получить или создать мьютекс для базы данных
    shared_ptr<mutex> getDatabaseMutex(const string& dbName);
    
    // Обработка клиентского подключения
    void handleClient(int clientSocket);
    
    // Обработка запроса
    json processRequest(const json& request);
    
    // Выполнение операций
    json executeInsert(const string& dbName, const string& collectionName, const json& data);
    json executeFind(const string& dbName, const string& collectionName, const json& query);
    json executeDelete(const string& dbName, const string& collectionName, const json& query);
    
    // Вспомогательные функции для работы с сетью
    string readMessage(int socket);
    void sendMessage(int socket, const string& message);
    json parseRequest(const string& message);
};

