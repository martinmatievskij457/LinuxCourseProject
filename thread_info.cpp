#include "thread_info.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>
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

ThreadInfo get_thread_info(int pid, int tid, const std::map<int, std::pair<long, long>>& prev_cpu_times) {
    ThreadInfo info;
    info.tid = tid;
    // Открываем stat файл
    ifstream stat_file("/proc/" + to_string(pid) + "/task/" + to_string(tid) + "/stat");
    string stat_line;
    getline(stat_file, stat_line);

    // Извлекаем имя потока (в скобках)
    size_t lparen = stat_line.find('(');
    size_t rparen = stat_line.find(')');
    if (lparen == string::npos || rparen == string::npos || rparen <= lparen) {
        info.name = "Unknown";
        info.state = "?";
        info.priority = 0;
        info.cpu_usage = 0.0;
        return info;
    }

    info.name = stat_line.substr(lparen + 1, rparen - lparen - 1);

    // Остальная часть после имени
    istringstream iss(stat_line.substr(rparen + 2));
    vector<string> stat_fields;
    string field;
    while (iss >> field) {
        stat_fields.push_back(field);
    }

    if (stat_fields.size() >= 16) {
        info.state = stat_fields[0];          // Это поле №3 в оригинале
        info.priority = stoi(stat_fields[15]); // Это поле №18 в оригинале

        long utime = stol(stat_fields[11]);   // поле №14
        long stime = stol(stat_fields[12]);   // поле №15

        // Получаем общее время CPU системы
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

        if (prev_cpu_times.count(tid)) {
            auto prev = prev_cpu_times.at(tid);
            long process_time_diff = (utime + stime) - (prev.first + prev.second);
            long total_time_diff = total_cpu_time - prev_cpu_times.at(-1).first;

            if (total_time_diff > 0 && process_time_diff >= 0) {
                info.cpu_usage = min(100.0, 100.0 * process_time_diff / total_time_diff);
            } else {
                info.cpu_usage = 0.0;
            }
        } else {
            info.cpu_usage = 0.0;
        }
    }
    
    return info;
}
