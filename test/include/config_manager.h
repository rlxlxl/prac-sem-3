#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "json_parser.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

struct ServerConfig {
    std::string host;
    int port;
};

struct LoggingConfig {
    std::string agent_id;
};

struct AgentConfig {
    ServerConfig server;
    LoggingConfig logging;
    std::vector<std::string> sources;
    int send_interval;  // секунды
    int batch_size;
    std::string state_file;  // файл для сохранения позиций чтения
    std::string output_json_file;  // файл для сохранения последних команд в JSON
    int max_json_events;  // максимальное количество событий в JSON файле
};

class ConfigManager {
private:
    AgentConfig config_;
    std::string config_path_;
    
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    void parseConfig(const std::string& configContent) {
        JsonParser parser;
        JsonValue config = parser.parse(configContent);
        
        // Server config
        if (config.hasKey("server")) {
            JsonValue server = config["server"];
            if (server.hasKey("host")) {
                config_.server.host = server["host"].asString();
            }
            if (server.hasKey("port")) {
                config_.server.port = server["port"].asInt();
            }
        }
        
        // Logging config
        if (config.hasKey("logging")) {
            JsonValue logging = config["logging"];
            if (logging.hasKey("agent_id")) {
                config_.logging.agent_id = logging["agent_id"].asString();
            }
        }
        
        // Sources
        if (config.hasKey("sources") && config["sources"].isArray()) {
            auto sources = config["sources"].asArray();
            config_.sources.clear();
            for (const auto& source : sources) {
                if (source.isString()) {
                    config_.sources.push_back(source.asString());
                }
            }
        }
        
        // Send interval
        if (config.hasKey("send_interval")) {
            config_.send_interval = config["send_interval"].asInt();
        }
        
        // Batch size
        if (config.hasKey("batch_size")) {
            config_.batch_size = config["batch_size"].asInt();
        }
        
        // State file
        if (config.hasKey("state_file")) {
            config_.state_file = config["state_file"].asString();
        }
        
        // Output JSON file
        if (config.hasKey("output_json_file")) {
            config_.output_json_file = config["output_json_file"].asString();
        }
        
        // Max JSON events
        if (config.hasKey("max_json_events")) {
            config_.max_json_events = config["max_json_events"].asInt();
        }
    }
    
public:
    ConfigManager() {
        // Значения по умолчанию
        config_.server.host = "localhost";
        config_.server.port = 8080;
        config_.logging.agent_id = "agent-macos-01";
        config_.sources = {"system_log", "bash_history", "unified_log"};
        config_.send_interval = 30;
        config_.batch_size = 100;
        config_.state_file = "/tmp/security_agent_state.json";
        config_.output_json_file = "/tmp/security_events.json";
        config_.max_json_events = 1000;
    }
    
    bool loadFromFile(const std::string& path) {
        try {
            config_path_ = path;
            std::string content = readFile(path);
            parseConfig(content);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool loadFromString(const std::string& configContent) {
        try {
            parseConfig(configContent);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << std::endl;
            return false;
        }
    }
    
    const AgentConfig& getConfig() const {
        return config_;
    }
    
    AgentConfig& getConfig() {
        return config_;
    }
};

#endif // CONFIG_MANAGER_H

