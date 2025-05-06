#include "process_info.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <numeric>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <numeric>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <pwd.h>
#include <map>
#include <utility>
#include <ncurses.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

std::vector<int> get_pids() {
    std::vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            pids.push_back(std::stoi(entry->d_name));
        }
    }
    closedir(dir);
    return pids;
}

std::vector<int> get_tids(int pid) {
    std::vector<int> tids;
    std::string path = "/proc/" + std::to_string(pid) + "/task";
    DIR* dir = opendir(path.c_str());
    if (!dir) return tids;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            tids.push_back(std::stoi(entry->d_name));
        }
    }
    closedir(dir);
    return tids;
}

ProcessInfo get_process_info(int pid, const std::map<int, std::pair<long, long>>& prev_cpu_times) {
    ProcessInfo info;
    info.pid = pid;
    // Чтение информации о процессе
    ifstream status_file("/proc/" + to_string(pid) + "/status");
    string line;
    
    while (getline(status_file, line)) {
        if (line.substr(0, 5) == "Name:") {
            info.name = line.substr(6);
            info.name.erase(info.name.find_last_not_of(" \t") + 1);
        } else if (line.substr(0, 7) == "Threads:") {
            info.threads = stoi(line.substr(8));
        } else if (line.substr(0, 6) == "VmRSS:") {
            info.memory_kb = stol(line.substr(7));
        } else if (line.substr(0, 4) == "Uid:") {
            istringstream iss(line.substr(5));
            uid_t uid;
            iss >> uid;
            info.user = get_username(uid);
        } else if (line.substr(0, 6) == "State:") {
            info.state = line.substr(7, 1);
        }
    }
    
    // Чтение приоритета и времени CPU из stat
    ifstream stat_file("/proc/" + to_string(pid) + "/stat");
    string stat_line;
    getline(stat_file, stat_line);
    
    istringstream iss(stat_line);
    vector<string> stat_fields;
    string field;
    
    while (iss >> field) {
        stat_fields.push_back(field);
    }
           
    if (stat_fields.size() >= 22) {
        info.priority = stoi(stat_fields[17]);
        
        long utime = stol(stat_fields[13]);
        long stime = stol(stat_fields[14]);
        
        ifstream proc_stat("/proc/stat");
        string cpu_line;
        getline(proc_stat, cpu_line);
        istringstream cpu_iss(cpu_line.substr(5));
        vector<long> cpu_times;
        long time;
        while (cpu_iss >> time) {
            cpu_times.push_back(time);
        }
        long total_cpu_time = accumulate(cpu_times.begin(), cpu_times.end(), 0L);
        
        if (prev_cpu_times.count(pid)) {
            auto prev = prev_cpu_times.at(pid);
            long process_time_diff = (utime + stime) - (prev.first + prev.second);
            long total_time_diff = total_cpu_time - prev_cpu_times.at(-1).first;
            
            if (total_time_diff > 0) {
                info.cpu_usage = 100.0 * process_time_diff / total_time_diff;
            } else {
                info.cpu_usage = 0.0;
            }
        } else {
            info.cpu_usage = 0.0;
        }
    }
    
    // Подсчёт файловых дескрипторов
    string fd_path = "/proc/" + to_string(pid) + "/fd";
    DIR* fd_dir = opendir(fd_path.c_str());
    if (fd_dir) {
        info.fd_count = 0;
        struct dirent* fd_entry;
        while ((fd_entry = readdir(fd_dir)) != nullptr) {
            if (isdigit(fd_entry->d_name[0])) {
                info.fd_count++;
            }
        }
        closedir(fd_dir);
    } else {
        info.fd_count = -1;  // Нет доступа
    }
    
    // Добавляем потоки только если их больше одного
    if (info.threads > 1) {
        vector<int> tids = get_tids(pid);
        for (int tid : tids) {
            info.thread_list.push_back(get_thread_info(pid, tid, prev_cpu_times));
        }
    }
    
    return info;
}
