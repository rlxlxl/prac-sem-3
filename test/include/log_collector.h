#ifndef LOG_COLLECTOR_H
#define LOG_COLLECTOR_H

#include "log_parser.h"
#include "message_buffer.h"
#include "config_manager.h"
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <sys/event.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <ctime>

struct FileState {
    std::string path;
    size_t position;
    int kq_fd;
    int file_fd;
    
    FileState() : position(0), kq_fd(-1), file_fd(-1) {}
};

class LogCollector {
private:
    ConfigManager& config_manager_;
    MessageBuffer& buffer_;
    LogParser parser_;
    std::map<std::string, FileState> file_states_;
    std::mutex state_mutex_;
    std::atomic<bool> running_;
    std::vector<std::thread> collector_threads_;
    int kq_;
    
    std::string getHomeDirectory() {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home);
        }
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            return std::string(pw->pw_dir);
        }
        return "/tmp";
    }
    
    std::string getUsername() {
        const char* user = getenv("USER");
        if (user) {
            return std::string(user);
        }
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            return std::string(pw->pw_name);
        }
        return "unknown";
    }
    
    void saveState() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        const auto& config = config_manager_.getConfig();
        
        try {
            JsonValue state;
            for (const auto& [source, fileState] : file_states_) {
                JsonValue fileStateJson;
                fileStateJson["path"] = JsonValue(fileState.path);
                fileStateJson["position"] = JsonValue(static_cast<int>(fileState.position));
                state[source] = fileStateJson;
            }
            
            std::ofstream file(config.state_file);
            if (file.is_open()) {
                file << state.toString();
                file.close();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving state: " << e.what() << std::endl;
        }
    }
    
    void loadState() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        const auto& config = config_manager_.getConfig();
        
        try {
            std::ifstream file(config.state_file);
            if (!file.is_open()) {
                return; // Файл не существует, это нормально
            }
            
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            
            if (content.empty()) {
                return;
            }
            
            JsonParser jsonParser;
            JsonValue state = jsonParser.parse(content);
            
            for (const auto& [source, fileStateJson] : state.asObject()) {
                if (fileStateJson.isObject() && fileStateJson.hasKey("path") && 
                    fileStateJson.hasKey("position")) {
                    std::string path = fileStateJson["path"].asString();
                    int positionInt = fileStateJson["position"].asInt();
                    // Если позиция отрицательная, сбрасываем на 0
                    size_t position = (positionInt < 0) ? 0 : static_cast<size_t>(positionInt);
                    
                    if (file_states_.count(source)) {
                        file_states_[source].position = position;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading state: " << e.what() << std::endl;
        }
    }
    
    void processSystemLog() {
        std::string logPath = "/var/log/system.log";
        
        FileState* state_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!file_states_.count("system_log")) {
                FileState state;
                state.path = logPath;
                state.position = 0;
                file_states_.emplace("system_log", state);
            }
            state_ptr = &file_states_["system_log"];
        }
        
        FileState& state = *state_ptr;
        
        std::ifstream file(state.path);
        if (!file.is_open()) {
            std::cerr << "Cannot open system log: " << state.path << std::endl;
            return;
        }
        
        // Переходим на сохраненную позицию
        file.seekg(state.position);
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            SecurityEvent event = parser_.parseSystemLog(line);
            if (!parser_.shouldFilterEvent(event)) {
                buffer_.add(event);
            }
        }
        
        // Сохраняем новую позицию
        state.position = file.tellg();
        file.close();
        
        saveState();
    }
    
    void saveEventsToJson(const std::vector<SecurityEvent>& events) {
        const auto& config = config_manager_.getConfig();
        if (config.output_json_file.empty()) {
            std::cerr << "Warning: output_json_file is empty, skipping save" << std::endl;
            return;
        }
        
        std::cout << "Saving " << events.size() << " events to " << config.output_json_file << std::endl;
        
        try {
            // Читаем существующие события
            std::vector<SecurityEvent> existingEvents;
            std::ifstream inFile(config.output_json_file);
            if (inFile.is_open()) {
                std::string line;
                JsonParser parser;
                while (std::getline(inFile, line)) {
                    if (line.empty()) continue;
                    try {
                        JsonValue json = parser.parse(line);
                        SecurityEvent event;
                        event.timestamp = json["timestamp"].asString();
                        event.hostname = json["hostname"].asString();
                        event.source = json["source"].asString();
                        event.event_type = json["event_type"].asString();
                        event.severity = json["severity"].asString();
                        event.user = json["user"].asString();
                        event.process = json["process"].asString();
                        event.command = json["command"].asString();
                        event.raw_log = json["raw_log"].asString();
                        existingEvents.push_back(event);
                    } catch (...) {
                        // Пропускаем некорректные строки
                        continue;
                    }
                }
                inFile.close();
            }
            
            // Добавляем новые события, проверяя на дубликаты
            // Используем простую проверку: команда + timestamp должны быть уникальными
            std::set<std::string> existingKeys;
            for (const auto& existing : existingEvents) {
                std::string key = existing.command + "|" + existing.timestamp;
                existingKeys.insert(key);
            }
            
            std::vector<SecurityEvent> eventsToAdd;
            for (const auto& event : events) {
                std::string key = event.command + "|" + event.timestamp;
                if (existingKeys.find(key) == existingKeys.end()) {
                    eventsToAdd.push_back(event);
                    existingKeys.insert(key);
                }
            }
            
            // Объединяем существующие и новые события
            std::vector<SecurityEvent> allEvents = existingEvents;
            allEvents.insert(allEvents.end(), eventsToAdd.begin(), eventsToAdd.end());
            
            // Ограничиваем количество событий (оставляем последние N)
            if (static_cast<int>(allEvents.size()) > config.max_json_events) {
                allEvents.erase(allEvents.begin(), 
                              allEvents.begin() + (allEvents.size() - config.max_json_events));
            }
            
            // Сохраняем в JSON файл (каждое событие на отдельной строке)
            std::ofstream outFile(config.output_json_file, std::ios::out | std::ios::trunc);
            if (!outFile.is_open()) {
                std::cerr << "Error: Cannot open JSON output file for writing: " 
                         << config.output_json_file << " (errno: " << errno << ")" << std::endl;
                return;
            }
            
            int savedCount = 0;
            for (const auto& event : allEvents) {
                JsonValue json = event.toJson();
                outFile << json.toString() << "\n";
                savedCount++;
            }
            
            outFile.close();
            std::cout << "Successfully saved " << savedCount << " events to " << config.output_json_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error saving events to JSON: " << e.what() << std::endl;
        }
    }
    
    // Убрана функция forceWriteHistory - она создавала бесконечный цикл,
    // так как команды osascript попадали в историю и читались снова
    
    std::string findHistoryFile() {
        std::string homeDir = getHomeDirectory();
        
        // Проверяем zsh_history (macOS по умолчанию)
        std::string zshHistory = homeDir + "/.zsh_history";
        std::ifstream zshFile(zshHistory);
        if (zshFile.is_open()) {
            zshFile.close();
            return zshHistory;
        }
        
        // Проверяем bash_history
        std::string bashHistory = homeDir + "/.bash_history";
        std::ifstream bashFile(bashHistory);
        if (bashFile.is_open()) {
            bashFile.close();
            return bashHistory;
        }
        
        // По умолчанию возвращаем zsh_history
        return zshHistory;
    }
    
    void processBashHistory() {
        std::string username = getUsername();
        std::string historyPath = findHistoryFile();
        
        FileState* state_ptr = nullptr;
        bool pathChanged = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!file_states_.count("bash_history")) {
                FileState state;
                state.path = historyPath;
                state.position = 0;
                file_states_.emplace("bash_history", state);
            } else {
                // Если путь изменился (например, с bash на zsh), сбрасываем позицию
                if (file_states_["bash_history"].path != historyPath) {
                    file_states_["bash_history"].path = historyPath;
                    file_states_["bash_history"].position = 0;
                    pathChanged = true;
                }
            }
            state_ptr = &file_states_["bash_history"];
        }
        
        FileState& state = *state_ptr;
        
        std::ifstream file(state.path);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open history file: " << state.path << std::endl;
            return;
        }
        
        std::cout << "Processing history file: " << state.path << std::endl;
        
        // Получаем текущий размер файла
        file.seekg(0, std::ios::end);
        size_t currentSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Если позиция некорректная (больше размера) или путь изменился, 
        // начинаем читать с последних ~100 строк (только новые команды)
        if (state.position > currentSize || pathChanged || state.position == 0) {
            if (currentSize > 0) {
                // Находим позицию примерно 100 строк назад от конца
                size_t targetPos = currentSize;
                int linesToSkip = 100;
                file.seekg(0, std::ios::end);
                for (int i = 0; i < linesToSkip && targetPos > 0; ++i) {
                    while (targetPos > 0) {
                        targetPos--;
                        file.seekg(targetPos);
                        if (file.peek() == '\n') {
                            break;
                        }
                    }
                }
                state.position = (targetPos > 0) ? targetPos + 1 : 0;
            } else {
                state.position = 0;
            }
            std::cout << "Starting to read new commands from position " << state.position 
                     << " (file size: " << currentSize << " bytes)" << std::endl;
        } else {
            std::cout << "Reading new commands from position " << state.position 
                     << " (file size: " << currentSize << " bytes)" << std::endl;
        }
        
        // Переходим на сохраненную позицию
        file.seekg(state.position);
        
        std::vector<SecurityEvent> newEvents;
        std::string line;
        size_t lastPosition = state.position;
        int linesRead = 0;
        int emptyLines = 0;
        int filteredEvents = 0;
        int parsedEvents = 0;
        
        std::cout << "Starting to read from position " << state.position << std::endl;
        
        while (std::getline(file, line)) {
            linesRead++;
            
            // Показываем прогресс каждые 100 строк (для новых команд)
            if (linesRead > 0 && linesRead % 100 == 0) {
                std::cout << "Progress: read " << linesRead << " new lines, parsed " << parsedEvents 
                         << ", saved " << newEvents.size() << " events..." << std::endl;
            }
            
            if (line.empty()) {
                emptyLines++;
                lastPosition = file.tellg();
                continue;
            }
            
            // Для zsh_history формат: ": timestamp:0;command" или ": timestamp:duration;command"
            // Извлекаем timestamp и команду
            std::string command = line;
            std::string historyTimestamp = "";
            
            if (!line.empty() && line[0] == ':') {
                // Ищем второе двоеточие
                size_t secondColon = line.find(':', 1);
                if (secondColon != std::string::npos) {
                    // Извлекаем timestamp (число между первым и вторым двоеточием)
                    std::string timestampStr = line.substr(1, secondColon - 1);
                    try {
                        // Конвертируем Unix timestamp в ISO формат
                        long long timestamp = std::stoll(timestampStr);
                        std::time_t time_t = static_cast<std::time_t>(timestamp);
                        std::tm* tm = std::gmtime(&time_t);
                        if (tm) {
                            std::ostringstream oss;
                            oss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
                            historyTimestamp = oss.str();
                        }
                    } catch (...) {
                        // Если не удалось распарсить timestamp, используем текущее время
                    }
                    
                    // Ищем точку с запятой после timestamp
                    size_t semicolon = line.find(';', secondColon);
                    if (semicolon != std::string::npos && semicolon + 1 < line.length()) {
                        command = line.substr(semicolon + 1);
                    } else {
                        // Если нет точки с запятой, возможно это не zsh формат
                        command = line;
                    }
                }
            }
            
            // Убираем ведущие пробелы и trailing backslashes (zsh формат)
            if (!command.empty()) {
                size_t firstNonSpace = command.find_first_not_of(" \t");
                if (firstNonSpace != std::string::npos) {
                    command = command.substr(firstNonSpace);
                } else {
                    command.clear();
                }
                
                // Убираем trailing backslash (zsh формат для многострочных команд)
                while (!command.empty() && command.back() == '\\') {
                    command.pop_back();
                }
            }
            
            if (command.empty()) {
                emptyLines++;
                lastPosition = file.tellg();
                continue;
            }
            
            SecurityEvent event = parser_.parseBashHistory(command, username, historyTimestamp);
            parsedEvents++;
            
            if (!parser_.shouldFilterEvent(event)) {
                buffer_.add(event);
                newEvents.push_back(event);
            } else {
                filteredEvents++;
            }
            
            lastPosition = file.tellg();
        }
        
        std::cout << "Finished reading: " << linesRead << " total lines, " << emptyLines << " empty, " 
                 << parsedEvents << " parsed, " << filteredEvents << " filtered, " 
                 << newEvents.size() << " events to save" << std::endl;
        
        // Сохраняем новые события в JSON файл
        if (!newEvents.empty()) {
            saveEventsToJson(newEvents);
            std::cout << "Processed " << newEvents.size() << " commands from " << historyPath 
                     << " (read " << linesRead << " lines, position: " << state.position 
                     << " -> " << lastPosition << ")" << std::endl;
        } else if (linesRead > 0) {
            std::cout << "Read " << linesRead << " lines but all were filtered" << std::endl;
        }
        
        // Сохраняем новую позицию
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            file_states_["bash_history"].position = lastPosition;
        }
        saveState();
        
        file.close();
    }
    
    void processUnifiedLog() {
        // Для unified logging используем команду log stream
        // Это упрощенная версия - в реальности нужно парсить вывод log stream
        // Здесь мы будем читать из /var/log/system.log как альтернативу
        
        // Можно также использовать log show --last 1m --predicate 'eventMessage contains "auth"'
        // Но для простоты используем system.log
        
        processSystemLog();
    }
    
    void collectorLoop(const std::string& source) {
        while (running_) {
            try {
                if (source == "system_log") {
                    processSystemLog();
                } else if (source == "bash_history") {
                    processBashHistory();
                } else if (source == "unified_log") {
                    processUnifiedLog();
                }
                
                // Пауза перед следующей проверкой
                // Для bash_history используем более частую проверку (2 секунды),
                // чтобы быстрее замечать новые команды
                if (source == "bash_history") {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                } else {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in collector for " << source << ": " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }
    }
    
public:
    LogCollector(ConfigManager& config_manager, MessageBuffer& buffer)
        : config_manager_(config_manager), buffer_(buffer), running_(false), kq_(-1) {
        loadState();
    }
    
    ~LogCollector() {
        stop();
        saveState();
        if (kq_ >= 0) {
            close(kq_);
        }
    }
    
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        const auto& config = config_manager_.getConfig();
        
        // Создаем поток для каждого источника
        for (const auto& source : config.sources) {
            collector_threads_.emplace_back(&LogCollector::collectorLoop, this, source);
        }
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        for (auto& thread : collector_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        collector_threads_.clear();
        saveState();
    }
};

#endif // LOG_COLLECTOR_H

