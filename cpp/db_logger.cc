#include <iostream>
#include <string>
#include "db_logger.h"

using std::cout;
using std::to_string;

static string *saved_logger_name = NULL;
static string saved_logger_id = "0";
static auto tag_cache = map<string, string>();

void log_db(DBConnection *connection, int level, time_t date, string hostname, int pid, string filename, string function, int line, int column, vector<string>parts, set<string>tags) {
	connection->execute("BEGIN TRANSACTION");

	if (saved_logger_name != &connection->logger_name) {
		// logger name changed
		saved_logger_name = &connection->logger_name;

		auto replacements = vector<string>();
		replacements.push_back(*saved_logger_name);

		// fetch id
		auto result = connection->query("SELECT id FROM " + connection->prefix + "_logger WHERE name = $1", replacements);
		if (result && result->size() > 0) {
			saved_logger_id = result->front()[string("id")];
		} else {
			// insert logger name into DB, will ignore the insert statement when a constraint error occurs
			saved_logger_id = to_string(connection->insert("INTO " + connection->prefix + "_logger (name) VALUES ($1)", replacements));
		}
	}

	// insert host name into DB, will ignore the insert statement when a constraint error occurs
	auto replacements = vector<string>();
	replacements.push_back(hostname);

	auto result = connection->query("SELECT id FROM " + connection->prefix + "_hosts WHERE name = $1", replacements);
	string hostname_id;
	if (result && result->size() > 0) {
		hostname_id = result->front()[string("id")];
	} else {
		hostname_id = to_string(connection->insert("INTO " + connection->prefix + "_hosts (name) VALUES ($1)", replacements));
	}

	// insert source path into DB, will ignore the insert statement when a constraint error occurs
	replacements = vector<string>();
	replacements.push_back(filename);

	result = connection->query("SELECT id FROM " + connection->prefix + "_source WHERE path = $1", replacements);
	string source_id;
	if (result && result->size() > 0) {
		source_id = result->front()[string("id")];
	} else {
		source_id = to_string(connection->insert("INTO " + connection->prefix + "_source (path) VALUES ($1)", replacements));
	}

	// insert function definition into DB, will ignore the insert statement when a constraint error occurs
	replacements = vector<string>();
	replacements.push_back(function);
	replacements.push_back(to_string(line));
	replacements.push_back(source_id);

	result = connection->query("SELECT id FROM " + connection->prefix + "_function WHERE name = $1 AND \"lineNumber\" = $2 AND \"sourceID\" = $3", replacements);
	string function_id;
	if (result && result->size() > 0) {
		function_id = result->front()[string("id")];
	} else {
		function_id = to_string(connection->insert("INTO " + connection->prefix + "_function (name, \"lineNumber\", \"sourceID\") VALUES ($1, $2, $3)", replacements));
	}

	connection->execute("COMMIT TRANSACTION");

	// insert log entry
	string message = "";
	for (string part : parts) {
		message += part + " ";
	}
	replacements = vector<string>();
	replacements.push_back(to_string(level));
	replacements.push_back(message);
	replacements.push_back(to_string(pid));
	replacements.push_back(to_string(date));
	replacements.push_back(saved_logger_id);
	replacements.push_back(hostname_id);
	replacements.push_back(function_id);

	auto entry_id = connection->insert("INTO " + connection->prefix + "_log (level, message, pid, time, \"loggerID\", \"hostnameID\", \"functionID\") VALUES ($1, $2, $3, $4, $5, $6, $7)", replacements);

	// fetch or create Tags
	auto tagIDs = vector<string>();
	for (string tag : tags) {
		auto search = tag_cache.find(tag);
		if (search != tag_cache.end()) {
			tagIDs.push_back(search->second);
			continue;
		}

		// insert tag into DB, will ignore the insert statement when a constraint error occurs
		auto replacements = vector<string>();
		replacements.push_back(tag);

		auto result = connection->query("SELECT id FROM " + connection->prefix + "_tag WHERE name = $1", replacements);
		string tag_id;
		if (result && result->size() > 0) {
			tag_id = result->front()[string("id")];
		} else {
			tag_id = to_string(connection->insert("INTO " + connection->prefix + "_tag (name) VALUES ($1)", replacements));
		}

		tag_cache[tag] = tag_id;
		tagIDs.push_back(tag_id);
	}

	for (string tagID : tagIDs) {
		replacements = vector<string>();
		replacements.push_back(tagID);
		replacements.push_back(to_string(entry_id));
		connection->insert("INTO " + connection->prefix + "_log_tag (\"tagID\", \"logID\") VALUES ($1, $2)", replacements);
	}
}
