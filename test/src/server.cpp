#include "server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

DatabaseServer* g_server = nullptr;

void signalHandler(int /*signal*/) {
    if (g_server) {
        std::cout << "\nShutting down server...\n";
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    
    DatabaseServer server(port);
    g_server = &server;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }
    
    return 0;
}

