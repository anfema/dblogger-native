#include <iostream>
#include "stdout_logger.h"

using std::cout;
using std::cerr;
static locale_t locale = newlocale(LC_ALL_MASK, "C", NULL);

void log_stdout(int level, time_t date, string hostname, int pid, string filename, string function, int line, int column, vector<string>parts, set<string>tags) {
	const struct tm *tstruct = localtime(&date);
	char c_date[256]; strftime_l(c_date, 256, "%Y-%m-%dT%H:%M:%S", tstruct, locale);
	string date_string = string(c_date);

	if (level >= 50) {
		cerr << date_string << " ";
		cerr << filename << "@";
		cerr << function << ":";
		cerr << line << ":";
		cerr << column;
		for (string tag: tags) {
			cerr << " [" << tag << "]";
		}
		cerr << ": ";
		for (string item: parts) {
			cerr << item << " ";
		}
		cerr << "\n";
	} else {
		cout << date_string << " ";
		cout << filename << "@";
		cout << function << ":";
		cout << line << ":";
		cout << column;
		for (string tag: tags) {
			cout << " [" << tag << "]";
		}
		cout << ": ";
		for (string item: parts) {
			cout << item << " ";
		}
		cout << "\n";

		cout.flush();
	}
}
