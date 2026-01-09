#include "security_agent.h"
#include <iostream>
#include <getopt.h>
#include <string>
#include <signal.h>
#include <thread>
#include <chrono>

using namespace std;

void printUsage(const char* programName) {
    cout << "Usage: " << programName << " [OPTIONS]\n";
    cout << "Options:\n";
    cout << "  -c, --config <file>    Configuration file path (default: config/agent_config.json)\n";
    cout << "  -d, --daemon           Run as daemon\n";
    cout << "  -h, --help             Show this help message\n";
    cout << "  -s, --stop             Stop running daemon\n";
    cout << "  -r, --restart          Restart daemon\n";
    cout << "\n";
    cout << "Examples:\n";
    cout << "  " << programName << " --config config/agent_config.json\n";
    cout << "  " << programName << " --daemon\n";
    cout << "  " << programName << " --stop\n";
}

pid_t readPidFromFile(const string& pid_file) {
    ifstream file(pid_file);
    if (!file.is_open()) {
        return -1;
    }
    
    pid_t pid;
    file >> pid;
    file.close();
    
    return pid;
}

bool stopDaemon(const string& pid_file) {
    pid_t pid = readPidFromFile(pid_file);
    if (pid < 0) {
        cerr << "Cannot find PID file or daemon is not running" << endl;
        return false;
    }
    
    if (kill(pid, SIGTERM) == 0) {
        cout << "Sent SIGTERM to process " << pid << endl;
        
        // Ждем завершения процесса
        for (int i = 0; i < 10; ++i) {
            if (kill(pid, 0) != 0) {
                cout << "Daemon stopped" << endl;
                return true;
            }
            this_thread::sleep_for(chrono::seconds(1));
        }
        
        // Если процесс не завершился, отправляем SIGKILL
        kill(pid, SIGKILL);
        cout << "Daemon force stopped" << endl;
        return true;
    } else {
        cerr << "Failed to send signal to process " << pid << endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    string config_path = "config/agent_config.json";
    bool daemon_mode = false;
    bool stop_mode = false;
    bool restart_mode = false;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"daemon", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"stop", no_argument, 0, 's'},
        {"restart", no_argument, 0, 'r'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:dhsr", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config_path = optarg;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 's':
                stop_mode = true;
                break;
            case 'r':
                restart_mode = true;
                break;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    if (stop_mode) {
        return stopDaemon("/tmp/security_agent.pid") ? 0 : 1;
    }
    
    if (restart_mode) {
        if (stopDaemon("/tmp/security_agent.pid")) {
            this_thread::sleep_for(chrono::seconds(2));
        }
        daemon_mode = true;
    }
    
    try {
        SecurityAgent agent(config_path);
        
        if (!agent.start(daemon_mode)) {
            cerr << "Failed to start security agent" << endl;
            return 1;
        }
        
        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}

