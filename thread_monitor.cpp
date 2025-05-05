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
#include <utility>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

struct ThreadInfo {
    int tid;
    string name;
    double cpu_usage;
    string state;
    int priority;
};

struct ProcessInfo {
    int pid;
    string name;
    int threads;
    double cpu_usage;
    long memory_kb;
    string user;
    string state;  // Состояние процесса (R, S, D, Z и т.д.)
    int priority;  // Приоритет процесса
    int fd_count;  // Количество открытых файловых дескрипторов
    vector<ThreadInfo> thread_list;
};

volatile bool running = true;
bool show_threads = false;

void signal_handler(int sig) {
    running = false;
}

vector<int> get_pids() {
    vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            pids.push_back(stoi(entry->d_name));
        }
    }
    closedir(dir);
    return pids;
}

vector<int> get_tids(int pid) {
    vector<int> tids;
    string task_path = "/proc/" + to_string(pid) + "/task";
    DIR* dir = opendir(task_path.c_str());
    if (!dir) return tids;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            tids.push_back(stoi(entry->d_name));
        }
    }
    closedir(dir);
    return tids;
}

string get_username(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return string(pw->pw_name);
    }
    return to_string(uid);
}

ThreadInfo get_thread_info(int pid, int tid, const map<int, pair<long, long>>& prev_cpu_times) {
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


ProcessInfo get_process_info(int pid, const map<int, pair<long, long>>& prev_cpu_times) {
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
            info.state = line.substr(7, 1);  // Первый символ состояния
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

// Обновлённая функция print_process_table()
void print_process_table(const vector<ProcessInfo>& processes, WINDOW* win) {
    wclear(win);
    
    // Заголовки столбцов
    wattron(win, A_BOLD);
    mvwprintw(win, 0, 0, "%-17s %-17s %-17s %-17s %-17s %-17s %-17s %-17s", 
              "PID/TID", "User", "St", "Prio", "CPU%", "FDs", "Memory (MB)", "Name");
    wattroff(win, A_BOLD);

    int row = 1;
    for (const auto& proc : processes) {
        if (proc.threads < 2) continue;
        
	short color_pair = 0;
	if (proc.state == "R") {
	    color_pair = 1;
	} else if (proc.state == "S") {
	    color_pair = 2;
	} else if (proc.state == "D") {
	    color_pair = 3;
	} else if (proc.state == "I") {
	    color_pair = 4;
	}
	if (color_pair != 0) {
	    wattron(win, COLOR_PAIR(color_pair));
	}
        
        // Вывод информации о процессе
        mvwprintw(win, row, 0, "%-17d %-17s %-17s %-17d %-17.2f %-17d %-17.2f %-17s", 
                 proc.pid, proc.user.c_str(), proc.state.c_str(), 
                 proc.priority, proc.cpu_usage, proc.fd_count,
                 proc.memory_kb / 1024.0, proc.name.c_str());
        
	if (color_pair != 0) {
	    wattroff(win, COLOR_PAIR(color_pair));
	}
        
        row++;
        
        // Вывод информации о потоках, если включено
        if (show_threads) {
            for (const auto& thread : proc.thread_list) {
                mvwprintw(win, row, 0, "%-17d %-17s %-17s %-17d %-17.2f %-17s %-17.2s %-17s", 
                         thread.tid, proc.user.c_str(), thread.state.c_str(), thread.priority, 
                         thread.cpu_usage, "-", "-", thread.name.c_str());
                row++;
                
                if (row >= LINES-2) break;
            }
        }
        
        if (row >= LINES-2) break;
    }

    // Подсказка внизу экрана (переместим ниже)
    int bottom_row = LINES - 3;  // Делаем вывод на две строки ниже

    wattron(win, A_BOLD);
    mvwprintw(win, bottom_row + 1, 0, "Press 't' to toggle threads view, 'q' to quit | Threads: %s", 
              show_threads ? "ON" : "OFF");

    mvwprintw(win, bottom_row + 2, 0, "States: R-running (green), S-sleeping (yellow), D-uninterruptible (red), I-idle (blue)");

    wattroff(win, A_BOLD);
    
    wrefresh(win);
}

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
    scrollok(win, TRUE);
    
    signal(SIGINT, signal_handler);
    
    map<int, pair<long, long>> prev_cpu_times;
    
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
    
    cout << "Monitoring stopped." << endl;
    return 0;
}

