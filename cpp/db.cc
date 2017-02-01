#include <iostream>
#include <cstring>
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
		string connString = "";

		if (db_host != "undefined") {
			connString = connString + " host='" + db_host + "'";
		}
		if (db_port > 0) {
			connString = connString + " port='" + std::to_string(db_port) + "'";
		}
		if (db_name != "undefined") {
			connString = connString + " dbname='" + db_name + "'";
		}
		if (db_user != "undefined") {
			connString = connString + " user='" + db_user + "'";
		}
		if (db_password != "undefined") {
			connString = connString + " password='" + db_password + "'"; // so password may not contain a ''
		}

		cerr << "Conn String: " << connString << "\n";
		pg = PQconnectdb(connString.c_str());
		if (pg == NULL) {
			cerr << "Could not initialize DB: PQConnectdb returned NULL\n";
		} else {
			if (PQstatus(pg) != CONNECTION_OK) {
				cerr << "Could not initialize DB: " << PQstatus(pg) << "\n";
				PQfinish(pg);
			} else {
				valid = true;
			}
		}
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
	if ((db_type == "postgres") && (pg != NULL)) {
		PQfinish(pg);
		pg = NULL;
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
		// Hosts + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_hosts_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_hosts\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_hosts_id_seq'),"
			"	\"name\" varchar(1024) NOT NULL COLLATE \"default\","
			"	CONSTRAINT \"" + prefix + "_logger_hosts_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_host_name_idx\" UNIQUE (\"name\") NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_host_name_idx\" ON \"" + prefix + "_hosts\" USING btree(\"name\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST);");
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_logger_hosts_id_key\" ON \"" + prefix + "_hosts\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");

		// Logger + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_logger_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_logger\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_logger_id_seq'),"
			"	\"name\" varchar(255) NOT NULL COLLATE \"default\","
			"	CONSTRAINT \"" + prefix + "_logger_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_logger_name_idx\" UNIQUE (\"name\") NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_logger_name_idx\" ON \"" + prefix + "_logger\" USING btree(\"name\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST);");
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_logger_logger_id_key\" ON \"" + prefix + "_logger\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");

		// Tag + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_tag_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_tag\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_tag_id_seq'),"
			"	\"name\" varchar(255) NOT NULL COLLATE \"default\","
			"	CONSTRAINT \"" + prefix + "_logger_tag_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_tag_name_idx\" UNIQUE (\"name\") NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_logger_tag_id_key\" ON \"" + prefix + "_tag\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_tag_name_idx\" ON \"" + prefix + "_tag\" USING btree(\"name\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST);");

		// Source + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_source_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS\"" + prefix + "_source\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_source_id_seq'),"
			"	\"path\" varchar(1024) NOT NULL COLLATE \"default\","
			"	CONSTRAINT \"" + prefix + "_logger_source_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_source_path_idx\" UNIQUE (\"path\") NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_logger_source_id_key\" ON \"" + prefix + "_source\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_source_path_idx\" ON \"" + prefix + "_source\" USING btree(\"path\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST);");

		// Function + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_function_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_function\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_function_id_seq'),"
			"	\"name\" varchar(1024) NOT NULL COLLATE \"default\","
			"	\"lineNumber\" int4, \"sourceID\" int4,"
			"	CONSTRAINT \"" + prefix + "_function_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_source_fk\" FOREIGN KEY (\"sourceID\") REFERENCES \"" + prefix + "_source\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_func_uniq\" UNIQUE (\"name\",\"lineNumber\",\"sourceID\") NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_function_id_key\" ON \"" + prefix + "_function\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_func_uniq\" ON \"" + prefix + "_function\" USING btree(\"name\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST, \"lineNumber\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST, \"sourceID\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
		execute("CREATE INDEX IF NOT EXISTS \"" + prefix + "_function_name_idx\" ON \"" + prefix + "_function\" USING btree(\"name\" COLLATE \"default\" \"pg_catalog\".\"text_ops\" ASC NULLS LAST);");

		// Log + Indexes
		execute("CREATE SEQUENCE IF NOT EXISTS \"" + prefix + "_log_id_seq\";");
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_log\" ("
			"	\"id\" int4 NOT NULL DEFAULT nextval('" + prefix + "_log_id_seq'),"
			"	\"level\" int4 NOT NULL DEFAULT 0, \"message\" text COLLATE \"default\","
			"	\"pid\" int4 NOT NULL, \"time\" int4 NOT NULL, \"functionID\" int4, \"loggerID\" int4, \"hostnameID\" int4,"
			"	CONSTRAINT \"" + prefix + "_log_id_key\" PRIMARY KEY (\"id\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_log_function_fk\" FOREIGN KEY (\"functionID\") REFERENCES \"" + prefix + "_function\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_log_host_fk\" FOREIGN KEY (\"hostnameID\") REFERENCES \"" + prefix + "_hosts\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_log_logger_fk\" FOREIGN KEY (\"loggerID\") REFERENCES \"" + prefix + "_logger\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE UNIQUE INDEX IF NOT EXISTS \"" + prefix + "_log_id_key\" ON \"" + prefix + "_log\" USING btree(\"id\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
		execute("CREATE INDEX IF NOT EXISTS \"" + prefix + "_log_pid_idx\" ON \"" + prefix + "_log\" USING btree(pid \"pg_catalog\".\"int4_ops\" ASC NULLS LAST, \"hostnameID\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");

		// Log->Tag + Indexes
		execute("CREATE TABLE IF NOT EXISTS \"" + prefix + "_log_tag\" ("
			"	\"tagID\" int4 NOT NULL, \"logID\" int4 NOT NULL,"
			"	CONSTRAINT \"" + prefix + "_log_tag_id_key\" PRIMARY KEY (\"tagID\", \"logID\") NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_log_tag_log_fk\" FOREIGN KEY (\"logID\") REFERENCES \"" + prefix + "_log\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE,"
			"	CONSTRAINT \"" + prefix + "_log_tag_tag_fk\" FOREIGN KEY (\"tagID\") REFERENCES \"" + prefix + "_tag\" (\"id\") ON UPDATE CASCADE ON DELETE CASCADE NOT DEFERRABLE INITIALLY IMMEDIATE"
			");"
		);
		execute("CREATE INDEX IF NOT EXISTS \"" + prefix + "_log_tag_id_key\" ON \"" + prefix + "_log_tag\" USING btree(\"tagID\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST, \"logID\" \"pg_catalog\".\"int4_ops\" ASC NULLS LAST);");
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

sqlite3_stmt *prepare_sqlite_statement(string sql, vector<string> parameters, sqlite3 *sqlite) {
	sqlite3_stmt *stmt = NULL;

	int result = sqlite3_prepare_v2(sqlite, sql.c_str(), sql.size(), &stmt, NULL);
	if (result != SQLITE_OK) {
		cerr << "Could not prepare SQL statement " << sql << ": " << sqlite3_errmsg(sqlite) << "\n";
		return NULL;
	}

	int index = 1;
	for (string value : parameters) {
		result = sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
		if (result != SQLITE_OK) {
			cerr << "Could not bind parameter #" << index << ": " << sqlite3_errmsg(sqlite) << "\n";
	 		return NULL;
		}
		index++;
	}

	return stmt;
}

PGresult *execute_pg_statement(string sql, vector<string> parameters, PGconn *pg) {
	int nParams = parameters.size();
	const char **values = NULL;

	if (nParams > 0) {
		values = (const char **)malloc(nParams * sizeof(char *));

		int i = 0;
		for (string value : parameters) {
			if (value == "NULL") {
				values[i] = NULL;
			} else {
				values[i] = (const char *)std::calloc(1, value.length() + 1);
				std::memcpy((void *)values[i], value.c_str(), value.size());
			}
			i++;
		}
	}

	PGresult *result = PQexecParams(
		pg,
		sql.c_str(),
		nParams,
		NULL, // parameter formats inferred from SQL
		values,
		NULL, // only text params, no binaries -> no length needed
		NULL, // only text params -> no format array needed
		0 // return text representation
	);

	if (nParams > 0) {
		for(int i = 0; i < nParams; i++) {
			if (values[i]) {
				free((void *)values[i]);
			}
		}
		free((void *)values);
	}

	return result;
}

/*
 * Public API
 */

int
DBConnection::insert(string sql) {
	return insert(sql, vector<string>());
}

int
DBConnection::insert(string sql, vector<string> parameters) {
	return insert(sql, parameters, false);
}

int
DBConnection::insert(string sql, vector<string> parameters, bool ignore_conflicts) {
	if (!valid) return -1;

	if (db_type == "sqlite") {
		string finished_sql;
		if (ignore_conflicts) {
			finished_sql = "INSERT OR IGNORE " + sql;
		} else{
			finished_sql = "INSERT " + sql;
		}

		sqlite3_mutex* mtx = sqlite3_db_mutex(sqlite);
		sqlite3_mutex_enter(mtx);

		sqlite3_stmt *stmt = prepare_sqlite_statement(finished_sql, parameters, sqlite);
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
	} else if (db_type == "postgres") {
		string finished_sql;
		if (ignore_conflicts) {
			finished_sql = "INSERT " + sql + " ON CONFLICT DO NOTHING RETURNING *";
		} else {
			finished_sql = "INSERT " + sql + " RETURNING *";
		}
		PGresult *result = execute_pg_statement(finished_sql, parameters, pg);

		if (result) {
			int status = PQresultStatus(result);
			if ((status == PGRES_TUPLES_OK) && (PQntuples(result) == 1)) {
				// fetch id from result
				int field_number = PQfnumber(result, "id");
				int id = -1;
				if (field_number >= 0) {
					char *value = PQgetvalue(result, 0, field_number);
					id = std::atoi(value);
				}
				PQclear(result);
				return id;
			} else if (status == PGRES_TUPLES_OK) {
				return -1;
			} else {
				cerr << "PostgreSQL Error: (status = " << status << ") " << PQresultErrorMessage(result);
				PQclear(result);
			}
		}
		cerr << "PostgreSQL Error: Insert failed, Out of memory\n";
	}

	// should not happen
	return -1;
}

bool
DBConnection::execute(string sql) {
	return execute(sql, vector<string>());
}

bool
DBConnection::execute(string sql, vector<string> parameters) {
	if (!valid) return false;

	if (db_type == "sqlite") {
		// SQLite implementation of execute
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
	} else if (db_type == "postgres") {
		// Postgres implementation of execute
		PGresult *result = execute_pg_statement(sql, parameters, pg);

		if (result) {
			int status = PQresultStatus(result);
			PQclear(result);
			if ((status == PGRES_COMMAND_OK) ||
				(status == PGRES_TUPLES_OK) ||
				(status == PGRES_EMPTY_QUERY)) {
				return true;
			}

			cerr << "PostgreSQL Error:" << PQresultErrorMessage(result);
			return false;
		}

		cerr << "PostgreSQL Error: Exec query failed, Out of memory\n";
		return false;
	}

	// Unknown DB type?
	return false;
}

vector< map<string, string> >*
DBConnection::query(string sql) {
	return query(sql, vector<string>());
}

vector< map<string, string> >*
DBConnection::query(string sql, vector<string> parameters) {
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
		PGresult *pg_result = execute_pg_statement(sql, parameters, pg);

		if (pg_result) {
			int status = PQresultStatus(pg_result);
			if (status == PGRES_TUPLES_OK) {
				int cols = PQnfields(pg_result);
				for(int row = 0; row < PQntuples(pg_result); row++) {
					auto row_result = map<string, string>();
					for(int col = 0; col < cols; col++) {
						auto name = string(PQfname(pg_result, col));
						auto value = string(PQgetvalue(pg_result, row, col));
						row_result[name] = value;
					}
					result->push_back(row_result);
				}
			} else {
				cerr << "PostgreSQL Error:" << PQresultErrorMessage(pg_result);
			}
			PQclear(pg_result);
		} else {
			cerr << "PostgreSQL Error: Query failed, Out of memory\n";
		}
	}

	return result;
}
