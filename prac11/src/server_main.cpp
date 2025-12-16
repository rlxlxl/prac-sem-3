#include "server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

DatabaseServer* g_server = nullptr;

void signalHandler(int signal) {
    if (g_server) {
        cout << "\nShutting down server...\n";
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char** argv) {
    string dbDir = "build/my_database";
    int port = 8080;
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--db-dir" && i + 1 < argc) {
            dbDir = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: " << argv[0] << " [--db-dir <directory>] [--port <port>]\n";
            cout << "Example: " << argv[0] << " --db-dir build/my_database --port 8080\n";
            return 0;
        }
    }
    
    DatabaseServer server(dbDir, port);
    g_server = &server;
    
    // Устанавливаем обработчик сигналов для корректного завершения
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Запускаем сервер
    server.start();
    
    return 0;
}

