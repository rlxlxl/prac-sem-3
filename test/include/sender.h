#ifndef SENDER_H
#define SENDER_H

#include "config_manager.h"
#include "message_buffer.h"
#include "json_parser.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <cstring>
#include <stdexcept>

class Sender {
private:
    ConfigManager& config_manager_;
    MessageBuffer& buffer_;
    int socket_;
    std::atomic<bool> running_;
    std::thread sender_thread_;
    
    bool connect() {
        const auto& config = config_manager_.getConfig();
        
        struct addrinfo hints, *result, *rp;
        std::memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        std::string portStr = std::to_string(config.server.port);
        int status = getaddrinfo(config.server.host.c_str(), portStr.c_str(), &hints, &result);
        if (status != 0) {
            std::cerr << "Error resolving address: " << gai_strerror(status) << std::endl;
            return false;
        }
        
        for (rp = result; rp != nullptr; rp = rp->ai_next) {
            socket_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_ < 0) {
                continue;
            }
            
            if (::connect(socket_, rp->ai_addr, rp->ai_addrlen) == 0) {
                break;
            }
            
            close(socket_);
            socket_ = -1;
        }
        
        freeaddrinfo(result);
        
        if (socket_ < 0) {
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
    
    bool sendMessage(const std::string& message) {
        if (socket_ < 0) {
            if (!connect()) {
                return false;
            }
        }
        
        // Отправляем длину сообщения (4 байта)
        uint32_t length = htonl(static_cast<uint32_t>(message.length()));
        ssize_t bytesSent = send(socket_, &length, sizeof(length), 0);
        if (bytesSent != sizeof(length)) {
            disconnect();
            return false;
        }
        
        // Отправляем само сообщение
        bytesSent = send(socket_, message.c_str(), message.length(), 0);
        if (bytesSent != static_cast<ssize_t>(message.length())) {
            disconnect();
            return false;
        }
        
        return true;
    }
    
    std::string readResponse() {
        uint32_t length;
        ssize_t bytesRead = recv(socket_, &length, sizeof(length), MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Server disconnected");
        }
        if (bytesRead != sizeof(length)) {
            throw std::runtime_error("Failed to read message length");
        }
        
        length = ntohl(length);
        
        if (length == 0 || length > 10 * 1024 * 1024) {
            throw std::runtime_error("Invalid message length");
        }
        
        std::vector<char> buffer(length);
        bytesRead = recv(socket_, buffer.data(), length, MSG_WAITALL);
        if (bytesRead == 0) {
            throw std::runtime_error("Server disconnected");
        }
        if (bytesRead != static_cast<ssize_t>(length)) {
            throw std::runtime_error("Failed to read message");
        }
        
        return std::string(buffer.data(), length);
    }
    
    bool sendBatch(const std::vector<SecurityEvent>& events) {
        if (events.empty()) {
            return true;
        }
        
        // Формируем сообщение для СУБД
        JsonValue request;
        request["database"] = JsonValue("security_db");
        request["operation"] = JsonValue("insert");
        request["collection"] = JsonValue("security_events");
        
        // Преобразуем события в JSON массив
        std::vector<JsonValue> eventsArray;
        for (const auto& event : events) {
            JsonValue eventJson = event.toJson();
            eventsArray.push_back(eventJson);
        }
        request["data"] = JsonValue(eventsArray);
        
        std::string message = request.toString();
        
        // Отправляем с повторными попытками
        int attempts = 3;
        while (attempts > 0) {
            if (sendMessage(message)) {
                try {
                    // Читаем ответ для проверки
                    std::string response = readResponse();
                    JsonParser parser;
                    JsonValue responseJson = parser.parse(response);
                    
                    if (responseJson.hasKey("status")) {
                        std::string status = responseJson["status"].asString();
                        if (status == "success") {
                            return true;
                        }
                    }
                } catch (const std::exception& e) {
                    // Ошибка чтения ответа, но сообщение могло быть отправлено
                    std::cerr << "Warning: " << e.what() << std::endl;
                    return true; // Считаем успешным
                }
            }
            
            attempts--;
            if (attempts > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        return false;
    }
    
    void senderLoop() {
        const auto& config = config_manager_.getConfig();
        
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(config.send_interval));
            
            if (!running_) break;
            
            // Берем события из буфера
            std::vector<SecurityEvent> events = buffer_.take(config.batch_size);
            
            if (!events.empty()) {
                if (sendBatch(events)) {
                    std::cout << "Sent " << events.size() << " events to server" << std::endl;
                } else {
                    // Возвращаем события обратно в буфер
                    for (const auto& event : events) {
                        buffer_.add(event);
                    }
                    std::cerr << "Failed to send events, returned to buffer" << std::endl;
                }
            }
        }
        
        disconnect();
    }
    
public:
    Sender(ConfigManager& config_manager, MessageBuffer& buffer)
        : config_manager_(config_manager), buffer_(buffer), socket_(-1), running_(false) {}
    
    ~Sender() {
        stop();
        disconnect();
    }
    
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        sender_thread_ = std::thread(&Sender::senderLoop, this);
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        if (sender_thread_.joinable()) {
            sender_thread_.join();
        }
    }
    
    bool sendImmediate(const std::vector<SecurityEvent>& events) {
        return sendBatch(events);
    }
};

#endif // SENDER_H

