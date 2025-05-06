#include "ui.h"
#include <ncurses.h>
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

bool show_threads = false;

void print_process_table(const std::vector<ProcessInfo>& processes, WINDOW* win) {
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
