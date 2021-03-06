#ifndef STDOUT_LOGGER_H
#define STDOUT_LOGGER_H

#include <string>
#include <set>
#include <vector>

using std::set;
using std::string;
using std::vector;

void log_stdout(int level, time_t date, string hostname, int pid, string filename, string function, int line, int column, vector<string>parts, set<string>tags);

#endif // STDOUT_LOGGER_H