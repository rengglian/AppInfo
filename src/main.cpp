#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <bitset>
#include <iomanip>

void printSchedulerDetails(pid_t id, int indent) {
    // Get and print scheduler policy
    int policy = sched_getscheduler(id);
    switch (policy) {
    case SCHED_OTHER:
        std::cout << std::setw(indent) << "Scheduler: SCHED_OTHER" << std::endl;
        break;
    case SCHED_FIFO:
        std::cout << std::setw(indent)<< "Scheduler: SCHED_FIFO" << std::endl;
        break;
    case SCHED_RR:
        std::cout << std::setw(indent) << "Scheduler: SCHED_RR" << std::endl;
        break;
#ifdef SCHED_BATCH   // Since Linux 2.6.16
    case SCHED_BATCH:
        std::cout << std::setw(indent) << "Scheduler: SCHED_BATCH" << std::endl;
        break;
#endif
#ifdef SCHED_IDLE    // Since Linux 2.6.23
    case SCHED_IDLE:
        std::cout << std::setw(indent) << "Scheduler: SCHED_IDLE" << std::endl;
        break;
#endif
#ifdef SCHED_DEADLINE    // Since Linux 3.14
    case SCHED_DEADLINE:
        std::cout << std::setw(indent) << "Scheduler: SCHED_DEADLINE" << std::endl;
        break;
#endif
    default:
        std::cout << std::setw(indent) << "Scheduler: Unknown" << std::endl;
        break;
    }

    // Get and print scheduler priority
    struct sched_param param;
    if (sched_getparam(id, &param) == -1) {
        std::cerr << "Error getting scheduler priority for Thread ID " << id << std::endl;
    } else {
        std::cout << std::setw(indent-2) << "Scheduler Priority: " << param.sched_priority << std::endl;
    }
}

void printThreadDetails(pid_t pid) {
    std::string task_dir = "/proc/" + std::to_string(pid) + "/task/";
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(task_dir.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (isdigit(ent->d_name[0])) {
                std::cout << "\t"<< "Thread ID: " << ent->d_name << std::endl;
                
                // Convert thread ID from string to int
                pid_t tid = std::stoi(ent->d_name);
                printSchedulerDetails(tid, 34);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening " << task_dir << std::endl;
    }
}

void printProcessDetails(pid_t pid) {
    std::string status_file = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status(status_file);
    if (!status.is_open()) {
        std::cerr << "Error opening status file for PID " << pid << std::endl;
        return;
    }

    std::cout << "PID: " << pid << std::endl;

    std::string line;
    while (std::getline(status, line)) {
        if (line.find("Name:") == 0 || line.find("State:") == 0) {
            std::cout << "\t" << line << std::endl;
        }
        if (line.find("Cpus_allowed:") == 0) {
            std::string cpus_hex = line.substr(line.find_last_of("\t")+1);
            std::bitset<8> cpus_bin(std::stol(cpus_hex, nullptr, 16));
            std::cout << "\t" << "Cpus_allowed: " << cpus_bin.count() << " (" << cpus_bin << ")" << std::endl;
        }        
    }
    status.close();
    printSchedulerDetails(pid, 30);
    printThreadDetails(pid);
    
}

void killProcess(pid_t pid) {
    if (kill(pid, SIGTERM) == -1) {
        std::cerr << "Error killing process " << pid << std::endl;
    } else {
        std::cout << "Successfully killed process " << pid << std::endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide an application name." << std::endl;
        return 1;
    }

    std::string app_name(argv[1]);
    bool should_kill = (argc > 2 && std::string(argv[2]) == "kill");

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("/proc")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (isdigit(ent->d_name[0])) {
                pid_t pid = std::stoi(ent->d_name);
                std::string cmdline_file = "/proc/" + std::to_string(pid) + "/cmdline";
                std::ifstream cmdline(cmdline_file);
                if (!cmdline.is_open()) {
                    continue;
                }

                std::string line;
                if (std::getline(cmdline, line)) {
                    std::istringstream iss(line);
                    std::string arg;
                    if (std::getline(iss, arg, '\0') && arg.find(app_name) != std::string::npos) {
                        printProcessDetails(pid);
                        if (should_kill) {
                            killProcess(pid);
                        }
                    }
                }
                cmdline.close();
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening /proc directory." << std::endl;
        return 1;
    }

    return 0;
}
