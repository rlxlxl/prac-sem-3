#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "json_parser.h"
#include <string>
#include <regex>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <unistd.h>
#include <iostream>

struct SecurityEvent {
    std::string timestamp;
    std::string hostname;
    std::string source;
    std::string event_type;
    std::string severity;
    std::string user;
    std::string process;
    std::string command;
    std::string raw_log;
    
    JsonValue toJson() const {
        JsonValue event;
        event["timestamp"] = JsonValue(timestamp);
        event["hostname"] = JsonValue(hostname);
        event["source"] = JsonValue(source);
        event["event_type"] = JsonValue(event_type);
        event["severity"] = JsonValue(severity);
        event["user"] = JsonValue(user);
        event["process"] = JsonValue(process);
        event["command"] = JsonValue(command);
        event["raw_log"] = JsonValue(raw_log);
        return event;
    }
};

class LogParser {
private:
    std::string hostname_;
    
    std::string getHostname() {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            return std::string(hostname);
        }
        return "unknown";
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::gmtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
    
    std::string parseTimestamp(const std::string& logLine) {
        // Пытаемся извлечь timestamp из различных форматов
        // macOS system.log: Jan 15 10:30:00
        std::regex dateRegex(R"((\w{3})\s+(\d{1,2})\s+(\d{2}):(\d{2}):(\d{2}))");
        std::smatch match;
        
        if (std::regex_search(logLine, match, dateRegex)) {
            // Упрощенная конвертация - используем текущий год
            auto now = std::time(nullptr);
            auto tm = *std::localtime(&now);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y") << "-";
            
            // Месяц
            std::string month = match[1].str();
            std::map<std::string, std::string> months = {
                {"Jan", "01"}, {"Feb", "02"}, {"Mar", "03"}, {"Apr", "04"},
                {"May", "05"}, {"Jun", "06"}, {"Jul", "07"}, {"Aug", "08"},
                {"Sep", "09"}, {"Oct", "10"}, {"Nov", "11"}, {"Dec", "12"}
            };
            if (months.count(month)) {
                oss << months[month] << "-";
            } else {
                oss << "01-";
            }
            
            // День и время
            std::string day = match[2].str();
            if (day.length() == 1) day = "0" + day;
            oss << day << "T" << match[3].str() << ":" << match[4].str() 
                << ":" << match[5].str() << "Z";
            return oss.str();
        }
        
        return getCurrentTimestamp();
    }
    
    std::string determineSeverity(const std::string& logLine, const std::string& /*source*/) {
        std::string lower = logLine;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower.find("error") != std::string::npos || 
            lower.find("failed") != std::string::npos ||
            lower.find("denied") != std::string::npos ||
            lower.find("unauthorized") != std::string::npos) {
            return "high";
        }
        if (lower.find("warning") != std::string::npos ||
            lower.find("invalid") != std::string::npos) {
            return "medium";
        }
        if (lower.find("sudo") != std::string::npos ||
            lower.find("su ") != std::string::npos ||
            lower.find("login") != std::string::npos) {
            return "medium";
        }
        return "low";
    }
    
public:
    LogParser() {
        hostname_ = getHostname();
    }
    
    SecurityEvent parseSystemLog(const std::string& logLine) {
        SecurityEvent event;
        event.hostname = hostname_;
        event.source = "system_log";
        event.raw_log = logLine;
        event.timestamp = parseTimestamp(logLine);
        event.severity = determineSeverity(logLine, "system_log");
        
        // Парсинг системного лога macOS
        // Формат: Jan 15 10:30:00 hostname process[pid]: message
        std::regex syslogRegex(R"(\w{3}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\s+\S+\s+(\S+)\[(\d+)\]:\s*(.*))");
        std::smatch match;
        
        if (std::regex_search(logLine, match, syslogRegex)) {
            event.process = match[1].str();
            std::string message = match[3].str();
            
            // Определяем тип события
            std::string lower = message;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            
            if (lower.find("sudo") != std::string::npos) {
                event.event_type = "sudo_command";
                // Извлекаем пользователя
                std::regex userRegex(R"((\w+)\s+:\s+TTY=)");
                std::smatch userMatch;
                if (std::regex_search(message, userMatch, userRegex)) {
                    event.user = userMatch[1].str();
                }
                // Извлекаем команду
                std::regex cmdRegex(R"(COMMAND=(.+))");
                std::smatch cmdMatch;
                if (std::regex_search(message, cmdMatch, cmdRegex)) {
                    event.command = cmdMatch[1].str();
                }
            } else if (lower.find("login") != std::string::npos || 
                      lower.find("sshd") != std::string::npos) {
                event.event_type = "user_login";
                std::regex userRegex(R"(user\s+(\w+))");
                std::smatch userMatch;
                if (std::regex_search(message, userMatch, userRegex)) {
                    event.user = userMatch[1].str();
                }
            } else if (lower.find("authentication failure") != std::string::npos) {
                event.event_type = "auth_failure";
                event.severity = "high";
            } else {
                event.event_type = "system_event";
            }
        } else {
            event.event_type = "system_event";
            event.process = "unknown";
        }
        
        return event;
    }
    
    SecurityEvent parseBashHistory(const std::string& logLine, const std::string& username) {
        SecurityEvent event;
        event.hostname = hostname_;
        event.source = "bash_history";
        event.raw_log = logLine;
        event.timestamp = getCurrentTimestamp();
        event.user = username;
        event.command = logLine;
        event.process = "bash";
        
        // Определяем тип события по команде
        std::string lower = logLine;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower.find("sudo") != std::string::npos) {
            event.event_type = "sudo_command";
            event.severity = "medium";
        } else if (lower.find("su ") != std::string::npos) {
            event.event_type = "su_command";
            event.severity = "medium";
        } else if (lower.find("ssh") != std::string::npos) {
            event.event_type = "ssh_command";
            event.severity = "low";
        } else if (lower.find("rm ") != std::string::npos || 
                   lower.find("del ") != std::string::npos) {
            event.event_type = "delete_command";
            event.severity = "medium";
        } else {
            event.event_type = "command_execution";
            event.severity = "low";
        }
        
        return event;
    }
    
    SecurityEvent parseUnifiedLog(const std::string& logLine) {
        SecurityEvent event;
        event.hostname = hostname_;
        event.source = "unified_log";
        event.raw_log = logLine;
        event.timestamp = getCurrentTimestamp();
        event.severity = determineSeverity(logLine, "unified_log");
        
        // Парсинг unified logging macOS (log stream output)
        // Формат может быть разным, упрощенный парсинг
        if (logLine.find("auth") != std::string::npos ||
            logLine.find("authentication") != std::string::npos) {
            event.event_type = "authentication";
            event.severity = "medium";
        } else if (logLine.find("security") != std::string::npos) {
            event.event_type = "security_event";
            event.severity = "high";
        } else {
            event.event_type = "system_event";
        }
        
        // Извлекаем процесс если есть
        std::regex procRegex(R"((\w+)\[(\d+)\])");
        std::smatch match;
        if (std::regex_search(logLine, match, procRegex)) {
            event.process = match[1].str();
        } else {
            event.process = "unknown";
        }
        
        return event;
    }
    
    bool shouldFilterEvent(const SecurityEvent& event) {
        // Фильтрация незначимых событий
        std::string lower = event.raw_log;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        // Игнорируем некоторые системные сообщения
        if (lower.find("kernel") != std::string::npos && 
            event.severity == "low") {
            return true;
        }
        
        if (lower.find("com.apple") != std::string::npos && 
            event.severity == "low") {
            return true;
        }
        
        return false;
    }
};

#endif // LOG_PARSER_H

