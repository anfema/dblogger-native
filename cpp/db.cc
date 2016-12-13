#include <iostream>
#include "db.h"

using std::cerr;
using std::exception;

DBConnection::DBConnection(
	string db_type, string db_host, int db_port,
	string db_user, string db_password, string db_name,
	string prefix) :
		db_type(db_type), db_host(db_host), db_port(db_port),
		db_user(db_user), db_password(db_password), db_name(db_name),
		prefix(prefix) {

	if (db_type == "sqlite") {
		int result = sqlite3_open_v2(db_name.c_str(), &sqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		if ((result != SQLITE_OK) && (sqlite != NULL)) {
			cerr << "Could not initialize DB: " << sqlite3_errmsg(sqlite) << "\n";
			sqlite3_close_v2(sqlite);
			return;
		}
		if (sqlite != NULL) {
			valid = true;
		}
	} else if (db_type == "postgres") {
		// TODO: Initialize Postgres
	} else {
		// not valid
		return;
	}

	if (valid) {
		setup();
	}
}

DBConnection::~DBConnection() {
	if ((db_type == "sqlite") && (sqlite != NULL)) {
		sqlite3_close_v2(sqlite);
		sqlite = NULL;
	}
}

/*
 * Private API
 */

void
DBConnection::setup() {
	if (db_type == "sqlite") {
		execute("PRAGMA synchronous = 0;");
		execute("PRAGMA auto_vacuum = 0;");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_hosts` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `name` VARCHAR(255) NOT NULL, UNIQUE (id), CONSTRAINT 'host_unique' UNIQUE (name COLLATE NOCASE));");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_logger` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `name` VARCHAR(255) NOT NULL, UNIQUE (id), CONSTRAINT 'name_unique' UNIQUE (name COLLATE NOCASE));");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_tag` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `name` VARCHAR(255) NOT NULL, UNIQUE (id), CONSTRAINT 'tag_unique' UNIQUE (name COLLATE NOCASE));");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_source` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `path` VARCHAR(1024) NOT NULL, UNIQUE (id), CONSTRAINT 'path_unique' UNIQUE (path));");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_function` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `name` VARCHAR(1024) NOT NULL, `lineNumber` INTEGER DEFAULT NULL, `sourceID` INTEGER REFERENCES `logger_source` (`id`) ON DELETE SET NULL ON UPDATE CASCADE, UNIQUE (id), CONSTRAINT 'func_unique' UNIQUE (name, lineNumber, sourceID));");
		execute("CREATE TABLE IF NOT EXISTS `" + prefix + "_log` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, `level` INTEGER NOT NULL, `message` TEXT, `pid` INTEGER NOT NULL, `time` INTEGER NOT NULL, `functionID` INTEGER REFERENCES `logger_function` (`id`) ON DELETE SET NULL ON UPDATE CASCADE, `loggerID` INTEGER REFERENCES `logger_logger` (`id`) ON DELETE SET NULL ON UPDATE CASCADE, `hostnameID` INTEGER REFERENCES `logger_hosts` (`id`) ON DELETE SET NULL ON UPDATE CASCADE, UNIQUE (id));");
		execute("CREATE TABLE IF NOT EXISTS `logger_log_tag` (`tagID` INTEGER NOT NULL REFERENCES `logger_tag` (`id`) ON DELETE CASCADE ON UPDATE CASCADE, `logID` INTEGER NOT NULL REFERENCES `logger_log` (`id`) ON DELETE CASCADE ON UPDATE CASCADE, PRIMARY KEY (`tagID`, `logID`));");
	} else if (db_type == "postgres") {
		// TODO: Setup postgres tables
	}
}


sqlite3_stmt *prepare_sqlite_statement(string sql, map<string, string> parameters, sqlite3 *sqlite) {
	sqlite3_stmt *stmt = NULL;

	int result = sqlite3_prepare_v2(sqlite, sql.c_str(), sql.size(), &stmt, NULL);
	if (result != SQLITE_OK) {
		cerr << "Could not prepare SQL statement " << sql << ": " << sqlite3_errmsg(sqlite) << "\n";
		return NULL;
	}
	for (auto replacement : parameters) {
		int index = sqlite3_bind_parameter_index(stmt, replacement.first.c_str());
		if (index == 0) {
			cerr << "Could not bind parameter '" << replacement.first << "': Parameter not found in SQL " << sql << "\n";
			return NULL;
		}
		result = sqlite3_bind_text(stmt, index, replacement.second.c_str(), -1, SQLITE_TRANSIENT);
		if (result != SQLITE_OK) {
			cerr << "Could not bind parameter '" << replacement.first << "': " << sqlite3_errmsg(sqlite) << "\n";
	 		return NULL;
		}
	}

	return stmt;
}

/*
 * Public API
 */

int
DBConnection::insert(string sql) {
	return insert(sql, map<string, string>());
}

int
DBConnection::insert(string sql, map<string, string> parameters) {
	if (db_type == "sqlite") {
		sqlite3_mutex* mtx = sqlite3_db_mutex(sqlite);
		sqlite3_mutex_enter(mtx);

		sqlite3_stmt *stmt = prepare_sqlite_statement(sql, parameters, sqlite);
		if (stmt != NULL) {
			int result = sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			if ((result != SQLITE_DONE) && (result != SQLITE_ROW)) {
				sqlite3_mutex_leave(mtx);
				return -1;
			}
			int id = sqlite3_last_insert_rowid(sqlite);
			sqlite3_mutex_leave(mtx);
			return id;
		}
	}

	// should not happen
	return -1;
}

bool
DBConnection::execute(string sql) {
	return execute(sql, map<string, string>());
}

bool
DBConnection::execute(string sql, map<string, string> parameters) {
	if (!valid) return false;

	if (db_type == "sqlite") {
		sqlite3_mutex* mtx = sqlite3_db_mutex(sqlite);
		sqlite3_mutex_enter(mtx);

		sqlite3_stmt *stmt = prepare_sqlite_statement(sql, parameters, sqlite);
		if (stmt != NULL) {
			int result = sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			if ((result != SQLITE_DONE) && (result != SQLITE_ROW)) {
				sqlite3_mutex_leave(mtx);
				return false;
			}
			sqlite3_mutex_leave(mtx);
			return true;
		}
		sqlite3_mutex_leave(mtx);
	} else {
		// TODO: Postgres implementation of execute
	}

	// Unknown DB type?
	return false;
}

vector< map<string, string> >*
DBConnection::query(string sql) {
	return query(sql, map<string, string>());
}

vector< map<string, string> >*
DBConnection::query(string sql, map<string, string> parameters) {
	auto result = new vector< map<string, string> >();
	if (!valid) return result;

	if (db_type == "sqlite") {
		sqlite3_mutex* mtx = sqlite3_db_mutex(sqlite);
		sqlite3_mutex_enter(mtx);

		sqlite3_stmt *stmt = prepare_sqlite_statement(sql, parameters, sqlite);
		if (stmt != NULL) {
			int status = sqlite3_step(stmt);
			while (status == SQLITE_ROW) {
				auto row = map<string, string>();
				for(int i = 0; i < sqlite3_column_count(stmt); i++) {
					auto name = string((char *)sqlite3_column_name(stmt, i));
					auto value = string((char *)sqlite3_column_text(stmt, i));
					row[name] = value;
				}
				result->push_back(row);
				status = sqlite3_step(stmt);
			}
			if (status != SQLITE_DONE) {
				cerr << "SQL query failed " << sql << ": " << sqlite3_errmsg(sqlite);
			}
			sqlite3_finalize(stmt);
			sqlite3_mutex_leave(mtx);
			return result;
		}
		sqlite3_mutex_leave(mtx);
	} else {
		// TODO: Postgres implementation of execute
	}

	return result;
}
