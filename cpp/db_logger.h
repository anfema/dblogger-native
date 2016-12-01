#ifndef DB_LOGGER_H
#define DB_LOGGER_H

#include <string>
#include <set>
#include <vector>

#include "db.h"

using std::set;
using std::string;
using std::vector;

void log_db(DBConnection *connection, int level, time_t date, string hostname, int pid, string filename, string function, int line, int column, vector<string>parts, set<string>tags);

#endif // DB_LOGGER_H