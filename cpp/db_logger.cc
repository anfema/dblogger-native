#include <iostream>
#include <string>
#include "db_logger.h"

using std::cout;
using std::to_string;

static string *saved_logger_name = NULL;
static string saved_logger_id = "0";
static auto tag_cache = map<string, string>();

void log_db(DBConnection *connection, int level, time_t date, string hostname, int pid, string filename, string function, int line, int column, vector<string>parts, set<string>tags) {
	connection->execute("BEGIN TRANSACTION"); // TODO: Fix for postgres

	if (saved_logger_name != &connection->logger_name) {
		// logger name changed
		saved_logger_name = &connection->logger_name;

		// insert logger name into DB, will ignore the insert statement when a constraint error occurs
		auto replacements = map<string, string>();
		replacements[":logger"] = *saved_logger_name;
		connection->execute("INSERT OR IGNORE INTO `" + connection->prefix + "_logger` (name) VALUES (:logger);", replacements); // TODO: Fix for postgres

		// fetch id
		auto result = connection->query("SELECT id FROM `" + connection->prefix + "_logger` WHERE name = :logger", replacements);
		saved_logger_id = result->front()[string("id")];
	}

	// insert host name into DB, will ignore the insert statement when a constraint error occurs
	auto replacements = map<string, string>();
	replacements[":host"] = hostname;
	connection->execute("INSERT OR IGNORE INTO `" + connection->prefix + "_hosts` (name) VALUES (:host);", replacements); // TODO: Fix for postgres

	// fetch id
	auto result = connection->query("SELECT id FROM `" + connection->prefix + "_hosts` WHERE name = :host", replacements);
	string hostname_id = result->front()[string("id")];


	// insert source path into DB, will ignore the insert statement when a constraint error occurs
	replacements = map<string, string>();
	replacements[":path"] = filename;
	connection->execute("INSERT OR IGNORE INTO `" + connection->prefix + "_source` (path) VALUES (:path);", replacements); // TODO: Fix for postgres

	// fetch id
	result = connection->query("SELECT id FROM `" + connection->prefix + "_source` WHERE path = :path", replacements);
	string source_id = result->front()[string("id")];

	// insert function definition into DB, will ignore the insert statement when a constraint error occurs
	replacements = map<string, string>();
	replacements[":name"] = function;
	replacements[":line"] = to_string(line);
	replacements[":sourceID"] = source_id;
	connection->execute("INSERT OR IGNORE INTO `" + connection->prefix + "_function` (name, lineNumber, sourceID) VALUES (:name, :line, :sourceID);", replacements); // TODO: Fix for postgres

	// fetch id
	result = connection->query("SELECT id FROM `" + connection->prefix + "_function` WHERE name = :name AND lineNumber = :line AND sourceID = :sourceID", replacements);
	string function_id = result->front()[string("id")];

	// insert log entry
	string message = "";
	for (string part : parts) {
		message += part + " ";
	}
	replacements = map<string, string>();
	replacements[":level"] = to_string(level);
	replacements[":message"] = message;
	replacements[":pid"] = to_string(pid);
	replacements[":date"] = to_string(date);
	replacements[":loggerID"] = saved_logger_id;
	replacements[":hostID"] = hostname_id;
	replacements[":functionID"] = function_id;

	auto entry_id = connection->insert("INSERT INTO `" + connection->prefix + "_log` (`level`, `message`, `pid`, `time`, `loggerID`, `hostnameID`, `functionID`) VALUES (:level, :message, :pid, :date, :loggerID, :hostID, :functionID);", replacements);

	// fetch or create Tags
	auto tagIDs = vector<string>();
	for (string tag : tags) {
		auto search = tag_cache.find(tag);
		if (search != tag_cache.end()) {
			tagIDs.push_back(search->second);
			continue;
		}

		// insert tag into DB, will ignore the insert statement when a constraint error occurs
		auto replacements = map<string, string>();
		replacements[":name"] = tag;
		connection->execute("INSERT OR IGNORE INTO `" + connection->prefix + "_tag` (name) VALUES (:name);", replacements); // TODO: Fix for postgres

		// fetch id
		auto result = connection->query("SELECT id FROM `" + connection->prefix + "_tag` WHERE name = :name", replacements);
		string tag_id = result->front()[string("id")];

		tag_cache[tag] = tag_id;
		tagIDs.push_back(tag_id);
	}

	replacements = map<string, string>();
	replacements[":logID"] = to_string(entry_id);
	for (string tagID : tagIDs) {
		replacements[":tagID"] = tagID;
		connection->execute("INSERT INTO `" + connection->prefix + "_log_tag` (`tagID`, `logID`) VALUES (:tagID, :logID);", replacements);
	}

	connection->execute("COMMIT TRANSACTION"); // TODO: Fix for postgres
}
