#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <string>
#include <vector>
#include <map>
#include <utility>
#include "thread_info.h"

struct ProcessInfo {
    int pid;
    std::string name;
    int threads;
    double cpu_usage;
    long memory_kb;
    std::string user;
    std::string state;
    int priority;
    int fd_count;
    std::vector<ThreadInfo> thread_list;
};

std::vector<int> get_pids();
std::vector<int> get_tids(int pid);
ProcessInfo get_process_info(int pid, const std::map<int, std::pair<long, long>>& prev_cpu_times);

#endif 
