#include "json.hpp"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cctype>

using namespace std;
using json = nlohmann::json;

class DatabaseClient {
private:
    string host_;
    int port_;
    string database_;
    int socket_;
    
    bool connect() {
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            cerr << "Error creating socket\n";
            return false;
        }
        
        sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        
        string hostToConnect = host_;
        if (host_ == "localhost") {
            hostToConnect = "127.0.0.1";
        }
        
        if (inet_pton(AF_INET, hostToConnect.c_str(), &serverAddr.sin_addr) <= 0) {
            struct hostent* he = gethostbyname(host_.c_str());
            if (he == nullptr) {
                cerr << "Invalid address or hostname: " << host_ << "\n";
                cerr << "Cannot resolve hostname. Try using IP address (e.g., 127.0.0.1)\n";
                close(socket_);
                return false;
            }
            memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        if (::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            cerr << "Connection failed to " << host_ << ":" << port_ << "\n";
            cerr << "Make sure the server is running: ./db_server --db-dir <dir> --port " << port_ << "\n";
            close(socket_);
            return false;
        }
        
        return true;
    }
    
    void disconnect() {
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
    }
    
    string readMessage() {
        uint32_t length;
        ssize_t bytesRead = recv(socket_, &length, sizeof(length), MSG_WAITALL);
        
        if (bytesRead != sizeof(length) || bytesRead <= 0) {
            return "";
        }
        
        length = ntohl(length);
        
        if (length == 0 || length > 1024 * 1024) {
            return "";
        }
        
        string message(length, '\0');
        bytesRead = recv(socket_, &message[0], length, MSG_WAITALL);
        
        if (bytesRead != static_cast<ssize_t>(length)) {
            return "";
        }
        
        return message;
    }
    
    void sendMessage(const string& message) {
        uint32_t length = htonl(message.length());
        send(socket_, &length, sizeof(length), 0);
        send(socket_, message.c_str(), message.length(), 0);
    }
    
    json sendRequest(const json& request) {
        if (socket_ < 0) {
            if (!connect()) {
                return json();
            }
        }
        
        sendMessage(request.dump());
        string responseStr = readMessage();
        
        if (responseStr.empty()) {
            disconnect();
            return json();
        }
        
        try {
            return json::parse(responseStr);
        } catch (...) {
            return json();
        }
    }
    
    string convertSingleQuotesToDouble(const string& str) {
        string result = str;
        bool escaped = false;
        
        for (size_t i = 0; i < result.length(); ++i) {
            if (escaped) {
                escaped = false;
                continue;
            }
            
            if (result[i] == '\\') {
                escaped = true;
                continue;
            }
            
            if (result[i] == '\'') {
                result[i] = '"';
            }
        }
        
        return result;
    }
    
    bool parseCommand(const string& line, string& operation, string& collection, json& data) {
        string trimmed = line;
        while (!trimmed.empty() && trimmed[0] == ' ') trimmed.erase(0, 1);
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        
        if (trimmed.empty()) return false;
        
        size_t spacePos = trimmed.find(' ');
        if (spacePos == string::npos) return false;
        
        operation = trimmed.substr(0, spacePos);
        string rest = trimmed.substr(spacePos + 1);
        
        size_t bracePos = rest.find('{');
        if (bracePos == string::npos) return false;
        
        collection = rest.substr(0, bracePos);
        while (!collection.empty() && collection.back() == ' ') collection.pop_back();
        
        string jsonStr = rest.substr(bracePos);
        
        jsonStr = convertSingleQuotesToDouble(jsonStr);
        
        try {
            data = json::parse(jsonStr);
        } catch (const exception& e) {
            cerr << "JSON parse error: " << e.what() << "\n";
            cerr << "Trying to parse: " << jsonStr << "\n";
            return false;
        }
        
        return true;
    }
    
public:
    DatabaseClient(const string& host, int port, const string& database)
        : host_(host), port_(port), database_(database), socket_(-1)
    {
    }
    
    ~DatabaseClient() {
        disconnect();
    }
    
    bool executeCommand(const string& line) {
        string operation, collection;
        json data;
        
        if (!parseCommand(line, operation, collection, data)) {
            cerr << "Error: Invalid command format\n";
            cerr << "Expected: OPERATION collection_name{...}\n";
            cerr << "Note: Database name is set via --database flag, not in command\n";
            cerr << "Examples:\n";
            cerr << "  INSERT users{'name': 'Alice', 'age': 25}\n";
            cerr << "  FIND users{'age': {'$gt': 20}}\n";
            cerr << "  DELETE users{'name': 'Alice'}\n";
            return false;
        }
        
        for (char& c : operation) {
            c = toupper(c);
        }
        
        json request;
        request["database"] = database_;
        request["collection"] = collection;
        request["operation"] = operation;
        
        if (operation == "INSERT") {
            if (data.is_array()) {
                request["data"] = data;
            } else {
                request["data"] = json::array({data});
            }
        } else if (operation == "FIND" || operation == "DELETE") {
            request["query"] = data;
        } else {
            cerr << "Error: Unknown operation '" << operation << "'\n";
            cerr << "Supported operations: INSERT, FIND, DELETE\n";
            return false;
        }
        
        json response = sendRequest(request);
        
        if (response.is_null()) {
            cerr << "Error: Failed to communicate with server\n";
            return false;
        }
        
        if (response.contains("status")) {
            string status = response["status"].get<string>();
            if (status == "error") {
                cerr << "Error: ";
            }
        }
        
        if (response.contains("message")) {
            cout << response["message"].get<string>() << "\n";
        }
        
        if (response.contains("data") && response["data"].is_array()) {
            json dataArray = response["data"];
            if (!dataArray.empty()) {
                cout << dataArray.dump(4) << "\n";
            }
        }
        
        return true;
    }
    
    void runInteractive() {
        cout << "Connected to database: " << database_ << "\n";
        cout << "Enter commands (type 'exit' or 'quit' to exit):\n";
        cout << "Format: OPERATION collection{...}\n";
        cout << "Example: INSERT users{'name': 'Alice', 'age': 25}\n";
        cout << "> ";
        
        string line;
        while (getline(cin, line)) {
            while (!line.empty() && line[0] == ' ') line.erase(0, 1);
            while (!line.empty() && line.back() == ' ') line.pop_back();
            
            if (line.empty()) {
                cout << "> ";
                continue;
            }
            
            if (line == "exit" || line == "quit") {
                break;
            }
            
            executeCommand(line);
            cout << "> ";
        }
    }
};

int main(int argc, char** argv) {
    string host = "localhost";
    int port = 8080;
    string database;
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        } else if (arg == "--database" && i + 1 < argc) {
            database = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: " << argv[0] << " --host <host> --port <port> --database <database>\n";
            cout << "Example: " << argv[0] << " --host localhost --port 8080 --database my_database\n";
            return 0;
        }
    }
    
    if (database.empty()) {
        cerr << "Error: Database name is required\n";
        cerr << "Use --database <name>\n";
        return 1;
    }
    
    DatabaseClient client(host, port, database);
    client.runInteractive();
    
    return 0;
}

