#ifndef THREAD_INFO_H
#define THREAD_INFO_H

#include <string>
#include <map>
#include <utility>

struct ThreadInfo {
    int tid;
    std::string name;
    double cpu_usage;
    std::string state;
    int priority;
};

ThreadInfo get_thread_info(int pid, int tid, const std::map<int, std::pair<long, long>>& prev_cpu_times);

#endif 
