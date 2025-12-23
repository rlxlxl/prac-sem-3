#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include "log_parser.h"
#include "json_parser.h"
#include <vector>
#include <mutex>
#include <fstream>
#include <deque>
#include <string>

class MessageBuffer {
private:
    std::deque<SecurityEvent> buffer_;
    std::mutex mutex_;
    size_t max_size_;
    std::string state_file_;
    std::string buffer_file_;
    
    void saveToDisk() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.empty()) return;
        
        try {
            std::ofstream file(buffer_file_, std::ios::app);
            if (!file.is_open()) {
                std::cerr << "Warning: Cannot save buffer to disk" << std::endl;
                return;
            }
            
            for (const auto& event : buffer_) {
                JsonValue json = event.toJson();
                file << json.toString() << "\n";
            }
            
            file.close();
        } catch (const std::exception& e) {
            std::cerr << "Error saving buffer to disk: " << e.what() << std::endl;
        }
    }
    
    void loadFromDisk() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ifstream file(buffer_file_);
            if (!file.is_open()) {
                return; // Файл не существует, это нормально
            }
            
            std::string line;
            JsonParser parser;
            while (std::getline(file, line)) {
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
                    
                    buffer_.push_back(event);
                } catch (const std::exception& e) {
                    // Пропускаем некорректные строки
                    continue;
                }
            }
            
            file.close();
            
            // Удаляем файл после загрузки
            std::remove(buffer_file_.c_str());
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading buffer from disk: " << e.what() << std::endl;
        }
    }
    
public:
    MessageBuffer(size_t max_size = 1000, const std::string& buffer_file = "/tmp/security_agent_buffer.jsonl")
        : max_size_(max_size), buffer_file_(buffer_file) {
        loadFromDisk();
    }
    
    ~MessageBuffer() {
        saveToDisk();
    }
    
    void add(const SecurityEvent& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Кольцевой буфер: если переполнен, удаляем старые события
        if (buffer_.size() >= max_size_) {
            buffer_.pop_front();
        }
        
        buffer_.push_back(event);
        
        // Если буфер переполнен, сохраняем на диск
        if (buffer_.size() >= max_size_) {
            saveToDisk();
        }
    }
    
    std::vector<SecurityEvent> take(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SecurityEvent> result;
        size_t to_take = std::min(count, buffer_.size());
        
        for (size_t i = 0; i < to_take; ++i) {
            result.push_back(buffer_.front());
            buffer_.pop_front();
        }
        
        return result;
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.empty();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
    }
};

#endif // MESSAGE_BUFFER_H

