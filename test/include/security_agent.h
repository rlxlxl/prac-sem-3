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

using namespace std;
class SecurityAgent {
private:
    ConfigManager config_manager_;
    MessageBuffer message_buffer_;
    LogCollector log_collector_;
    Sender sender_;
    atomic<bool> running_;
    string pid_file_;
    
    void daemonize() {
        // Создаем дочерний процесс
        pid_t pid = fork();
        
        if (pid < 0) {
            throw runtime_error("Failed to fork");
        }
        
        if (pid > 0) {
            // Родительский процесс завершается
            exit(0);
        }
        
        // Создаем новую сессию
        if (setsid() < 0) {
            throw runtime_error("Failed to create session");
        }
        
        // Второй fork для гарантии, что процесс не является лидером сессии
        pid = fork();
        if (pid < 0) {
            throw runtime_error("Failed to fork second time");
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
        ofstream file(pid_file_);
        if (file.is_open()) {
            file << getpid() << endl;
            file.close();
        }
    }
    
    void removePid() {
        remove(pid_file_.c_str());
    }
    
    static SecurityAgent* instance_;
    static void signalHandler(int signal) {
        if (instance_) {
            cerr << "Received signal " << signal << ", shutting down..." << endl;
            instance_->stop();
        }
    }
    
public:
    SecurityAgent(const string& config_path = "config/agent_config.json",
                  const string& pid_file = "/tmp/security_agent.pid")
        : config_manager_(),
          message_buffer_(1000, "/tmp/security_agent_buffer.jsonl"),
          log_collector_(config_manager_, message_buffer_),
          sender_(config_manager_, message_buffer_),
          running_(false),
          pid_file_(pid_file) {
        instance_ = this;
        
        // Загружаем конфигурацию
        if (!config_manager_.loadFromFile(config_path)) {
            cerr << "Warning: Using default configuration" << endl;
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
            } catch (const exception& e) {
                cerr << "Failed to daemonize: " << e.what() << endl;
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
        
        cout << "Security agent started" << endl;
        cout << "Agent ID: " << config_manager_.getConfig().logging.agent_id << endl;
        cout << "Server: " << config_manager_.getConfig().server.host 
                  << ":" << config_manager_.getConfig().server.port << endl;
        
        // Основной цикл
        while (running_) {
            this_thread::sleep_for(chrono::seconds(1));
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
        
        cout << "Security agent stopped" << endl;
    }
    
    bool isRunning() const {
        return running_;
    }
};

SecurityAgent* SecurityAgent::instance_ = nullptr;

#endif // SECURITY_AGENT_H

