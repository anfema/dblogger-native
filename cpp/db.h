#ifndef DB_H
#define DB_H

#include <string>
#include <map>
#include <vector>

#include <sqlite3.h>
#include <libpq-fe.h>

using std::string;
using std::map;
using std::vector;
using std::exception;

class DBConnection {
	public:
		DBConnection(string db_type, string db_host, int db_port, string db_user, string db_password, string db_name, string prefix, string logger_name);
		~DBConnection();
		bool execute(string sql);
		bool execute(string sql, vector<string> parameters); // not available for all DB implementations
		vector< map<string, string> >* query(string sql);
		vector< map<string, string> >* query(string sql, vector<string> parameters);
		int insert(string sql);
		int insert(string sql, vector<string> parameters);
		int insert(string sql, vector<string> parameters, bool ignore_conflicts);

		bool valid;
		string logger_name;
		int global_log_level;
		bool log_to_stdout;

		const string db_type;
		const string db_host;
		const int db_port;
		const string db_user;
		const string db_password;
		const string db_name;
		const string prefix;

	private:
		void setup();

		sqlite3 *sqlite;
		PGconn *pg;
};

#endif
