#ifndef SECURITY_AGENT_H
#define SECURITY_AGENT_H

#include "config_manager.h"
#include "log_collector.h"
#include "message_buffer.h"
#include "sender.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <cstdlib>

class SecurityAgent {
private:
    ConfigManager config_manager_;
    MessageBuffer message_buffer_;
    LogCollector log_collector_;
    Sender sender_;
    std::atomic<bool> running_;
    std::string pid_file_;
    
    void daemonize() {
        // Создаем дочерний процесс
        pid_t pid = fork();
        
        if (pid < 0) {
            throw std::runtime_error("Failed to fork");
        }
        
        if (pid > 0) {
            // Родительский процесс завершается
            exit(0);
        }
        
        // Создаем новую сессию
        if (setsid() < 0) {
            throw std::runtime_error("Failed to create session");
        }
        
        // Второй fork для гарантии, что процесс не является лидером сессии
        pid = fork();
        if (pid < 0) {
            throw std::runtime_error("Failed to fork second time");
        }
        
        if (pid > 0) {
            exit(0);
        }
        
        // Меняем рабочую директорию
        chdir("/");
        
        // Закрываем стандартные файловые дескрипторы
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // Перенаправляем в /dev/null
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            if (null_fd > 2) {
                close(null_fd);
            }
        }
        
        // Устанавливаем umask
        umask(0);
        
        // Сохраняем PID
        savePid();
    }
    
    void savePid() {
        std::ofstream file(pid_file_);
        if (file.is_open()) {
            file << getpid() << std::endl;
            file.close();
        }
    }
    
    void removePid() {
        std::remove(pid_file_.c_str());
    }
    
    static SecurityAgent* instance_;
    static void signalHandler(int signal) {
        if (instance_) {
            std::cerr << "Received signal " << signal << ", shutting down..." << std::endl;
            instance_->stop();
        }
    }
    
public:
    SecurityAgent(const std::string& config_path = "config/agent_config.json",
                  const std::string& pid_file = "/tmp/security_agent.pid")
        : config_manager_(),
          message_buffer_(1000, "/tmp/security_agent_buffer.jsonl"),
          log_collector_(config_manager_, message_buffer_),
          sender_(config_manager_, message_buffer_),
          running_(false),
          pid_file_(pid_file) {
        instance_ = this;
        
        // Загружаем конфигурацию
        if (!config_manager_.loadFromFile(config_path)) {
            std::cerr << "Warning: Using default configuration" << std::endl;
        }
    }
    
    ~SecurityAgent() {
        stop();
        removePid();
        instance_ = nullptr;
    }
    
    bool start(bool daemon = false) {
        if (running_) {
            return false;
        }
        
        if (daemon) {
            try {
                daemonize();
            } catch (const std::exception& e) {
                std::cerr << "Failed to daemonize: " << e.what() << std::endl;
                return false;
            }
        } else {
            savePid();
        }
        
        // Устанавливаем обработчики сигналов
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        signal(SIGHUP, signalHandler);
        
        running_ = true;
        
        // Запускаем компоненты
        log_collector_.start();
        sender_.start();
        
        std::cout << "Security agent started" << std::endl;
        std::cout << "Agent ID: " << config_manager_.getConfig().logging.agent_id << std::endl;
        std::cout << "Server: " << config_manager_.getConfig().server.host 
                  << ":" << config_manager_.getConfig().server.port << std::endl;
        
        // Основной цикл
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        return true;
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        log_collector_.stop();
        sender_.stop();
        
        std::cout << "Security agent stopped" << std::endl;
    }
    
    bool isRunning() const {
        return running_;
    }
};

SecurityAgent* SecurityAgent::instance_ = nullptr;

#endif // SECURITY_AGENT_H

