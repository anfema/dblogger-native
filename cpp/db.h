#ifndef DB_H
#define DB_H

#include <string>
#include <map>
#include <vector>

#include "sqlite3.h"

using std::string;
using std::map;
using std::vector;
using std::exception;

class DBConnection {
	public:
		DBConnection(string db_type, string db_host, int db_port, string db_user, string db_password, string db_name, string prefix);
		~DBConnection();
		bool execute(string sql);
		bool execute(string sql, map<string, string> parameters);
		vector< map<string, string> >* query(string sql);
		vector< map<string, string> >* query(string sql, map<string, string> parameters);
		int insert(string sql);
		int insert(string sql, map<string, string> parameters);

		bool valid;
		string prefix;
		string logger_name;

	private:
		void setup();

		string db_type_;
		string db_host_;
		int db_port_;
		string db_user_;
		string db_password_;
		string db_name_;

		sqlite3 *sqlite;
};

#endif
