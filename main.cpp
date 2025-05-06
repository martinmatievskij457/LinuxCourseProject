#include <csignal>
#include <map>
#include <utility>
#include <fstream>
#include <numeric>
#include <ncurses.h>
#include "process_info.h"
#include "ui.h"
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
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

volatile bool running = true;
void signal_handler(int sig) { running = false; }

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    curs_set(0);
    
    init_pair(1, COLOR_GREEN, COLOR_BLACK);   // R - выполняющийся
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // S - ожидающий
    init_pair(3, COLOR_RED, COLOR_BLACK);     // D - непрерываемый сон
    init_pair(4, COLOR_BLUE, COLOR_BLACK);   // I - idle (бездействующий)
    
    WINDOW* win = newwin(LINES, COLS, 0, 0);
    signal(SIGINT, signal_handler);

    std::map<int, std::pair<long, long>> prev_cpu_times;
    
    while (running) {
        // Обработка нажатий клавиш
        nodelay(stdscr, TRUE);
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = false;
            break;
        } else if (ch == 't' || ch == 'T') {
            show_threads = !show_threads;
        }
        
        vector<int> pids = get_pids();
        
        // Получение общего времени CPU системы
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
        
        prev_cpu_times[-1] = {total_cpu_time, 0};
        
        vector<ProcessInfo> multi_threaded;
        
        for (int pid : pids) {
            try {
                ProcessInfo info = get_process_info(pid, prev_cpu_times);
                
                if (info.threads > 1) {
                    multi_threaded.push_back(info);
                    
                    // Сохранение времени CPU для потоков
                    for (const auto& thread : info.thread_list) {
                        ifstream stat_file("/proc/" + to_string(pid) + "/task/" + to_string(thread.tid) + "/stat");
                        string stat_line;
                        getline(stat_file, stat_line);
                        
                        istringstream iss(stat_line);
                        vector<string> stat_fields;
                        string field;
                        
                        while (iss >> field) {
                            stat_fields.push_back(field);
                        }
                        
                        if (stat_fields.size() >= 22) {
                            long utime = stol(stat_fields[13]);
                            long stime = stol(stat_fields[14]);
                            prev_cpu_times[thread.tid] = {utime, stime};
                        }
                    }
                }
                
                // Сохранение времени CPU для процесса
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
                    long utime = stol(stat_fields[13]);
                    long stime = stol(stat_fields[14]);
                    prev_cpu_times[pid] = {utime, stime};
                }
            } catch (...) {
                continue;
            }
        }
        
        // Сортировка по использованию CPU
        sort(multi_threaded.begin(), multi_threaded.end(), 
            [](const ProcessInfo& a, const ProcessInfo& b) {
                return a.cpu_usage > b.cpu_usage;
            });
        
        print_process_table(multi_threaded, win);
        
        // Ожидание обновления
        napms(2000);
    }
    delwin(win);
    endwin();
    return 0;
}
