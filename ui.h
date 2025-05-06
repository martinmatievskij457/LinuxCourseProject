#ifndef UI_H
#define UI_H

#include <vector>
#include "process_info.h"
#include <ncurses.h>

extern bool show_threads;

void print_process_table(const std::vector<ProcessInfo>& processes, WINDOW* win);

#endif 
