#include "utils.h"
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

std::string get_username(uid_t uid) {
    struct passwd* pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
    return std::to_string(uid);
}
