#include <string>
#include <iostream>
#include <unistd.h>
#include <time.h>

#include "logger.h"
#include "db.h"
#include "stdout_logger.h"
#include "db_logger.h"

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;
using v8::Boolean;
using v8::Exception;
using v8::Object;
using v8::EscapableHandleScope;
using v8::StackTrace;
using v8::StackFrame;
using std::string;
using std::cout;

static DBConnection *connection = NULL;
static Persistent<Object> node_path;


/*
 * Helper funcs
 */

 static inline Local<String> local_string(Isolate *isolate, string str) {
	return String::NewFromUtf8(isolate, str.c_str(), NewStringType::kNormal).ToLocalChecked();
}


static inline Local<Value> get_value_from_dict(Isolate *isolate, const Local<Object>obj, string key) {
	Local<Value> result = obj->Get(
		isolate->GetCurrentContext(),
		local_string(isolate, key)
	).ToLocalChecked();

	return result;
}

static inline Local<Object> get_object_from_dict(Isolate *isolate, const Local<Object>obj, string key) {
	return get_value_from_dict(isolate, obj, key)->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
}

static inline string get_string_from_value(Isolate *isolate, const Local<Value>val) {
	return string(
		*String::Utf8Value(
			isolate,
			val
		)
	);
}

static inline string get_string_from_dict(Isolate *isolate, const Local<Object>obj, string key) {
	return get_string_from_value(isolate, get_value_from_dict(isolate, obj, key));
}

static inline int get_int_from_dict(Isolate *isolate, const Local<Object>obj, string key) {
	return get_value_from_dict(isolate, obj, key)->IntegerValue(isolate->GetCurrentContext()).FromMaybe(0);
}

static inline bool get_bool_from_dict(Isolate *isolate, const Local<Object>obj, string key) {
	return get_value_from_dict(isolate, obj, key)->BooleanValue(isolate);
}


// Initialize DB connection, will terminate and overwrite the old connection
static inline void initializeDB(Isolate *isolate, const Local<Object> config) {
	// unpack config object
	string db_host = get_string_from_dict(isolate, config, "host");
	int db_port = get_int_from_dict(isolate, config, "port");
	string db_user = get_string_from_dict(isolate, config, "user");
	string db_password = get_string_from_dict(isolate, config, "password");
	string db_name = get_string_from_dict(isolate, config, "name");
	string db_type = get_string_from_dict(isolate, config, "type");
	string prefix = get_string_from_dict(isolate, config, "tablePrefix");
	string logger_name = get_string_from_dict(isolate, config, "logger");
	string log_level_string = get_string_from_dict(isolate, config, "level");

	int log_level = -1;
	if (log_level_string != "undefined") {
		log_level = get_int_from_dict(isolate, config, "level");
	}

	bool log_to_stdout = false;
	if (get_value_from_dict(isolate, config, "stdout")->IsBoolean()) {
		log_to_stdout = get_bool_from_dict(isolate, config, "stdout");
	} else {
		if (connection) {
			log_to_stdout = connection->log_to_stdout;
		}
	}

	if (prefix == "undefined") {
		prefix = string("logger");
	}

	if (logger_name == "undefined") {
		logger_name = string("default");
	}

	if (db_type == "undefined") {
		// no db type, do not reinitialize
		return;
	}

	// close old connection if set
	if (connection != NULL) {
		if (log_level < 0) {
			log_level = connection->global_log_level;
		}
		delete connection;
		connection = NULL;
	}

	// create new connection
	connection = new DBConnection(db_type, db_host, db_port, db_user, db_password, db_name, prefix, logger_name);
	connection->log_to_stdout = log_to_stdout;
	if (log_level >= 0) {
		connection->global_log_level = log_level;
	}
}

// JSON.stringify() a value
static string JSONStringify(Isolate *isolate, Local<Value> obj) {
	// get the global JSON object
	Local<Object> json = get_object_from_dict(isolate, isolate->GetCurrentContext()->Global(), "JSON");

	// fetch `stringify` function
	Local<Function> stringify = get_value_from_dict(isolate, json, "stringify").As<Function>();

	// call stringify on the obj
	Local<Value> result = stringify->Call(isolate->GetCurrentContext(), json, 1, &obj).ToLocalChecked();

	// return the result
	return string(*String::Utf8Value(isolate, result));
}

// Use the node internal path.relative() function to calculate a relative path
static string relativePath(Isolate *isolate, Local<Value> to, char *from) {
	// get the unbound path module inserted into isolate
	Local<Object> path = Local<Object>::New(isolate, node_path);

	// fetch the `relative` function
	Local<Function> relative = get_value_from_dict(isolate, path, "relative").As<Function>();

	// call path.relative(from, to)
	Local<Value> args[2];
	args[0] = local_string(isolate, from);
	args[1] = to;

	Local<Value> result = relative->Call(isolate->GetCurrentContext(), path, 2, args).ToLocalChecked();

	// If the result is valid return it
	if (result->IsString()) {
		return string(*String::Utf8Value(isolate, result));
	}

	// invalid result return `to`
	return string(*String::Utf8Value(isolate, to));
}

// Save a log entry
static void log(int level, Logger *logger, const FunctionCallbackInfo<Value>& args) {
	if (level < logger->level) {
		return;
	}

	Isolate* isolate = args.GetIsolate();

	// fetch date
	time_t now = time(NULL);

	// fetch host name
	char c_hostname[1024]; gethostname(c_hostname, 1024);
	string hostname = string(c_hostname);

	// fetch pid
	int pid = getpid();

	// fetch stack frame for: filename, source line, function name
	Local<StackFrame> frame = StackTrace::CurrentStackTrace(isolate, 1, StackTrace::kOverview)->GetFrame(isolate, 0);
	char c_path[1024] = {}; getcwd(c_path, 1024);
	string filename = relativePath(isolate, frame->GetScriptName(), c_path);
	string function = string(*String::Utf8Value(isolate, frame->GetFunctionName())) + "()";
	int line = frame->GetLineNumber();
	int column = frame->GetColumn();

	if (function == "()") {
		// if we get no function the call was from the toplevel scope
		function = "<global scope>";
	}

	// convert all arguments to readable values (JSON.stringify objects and arrays)
	vector<string> objs = vector<string>();
	for(int i = 0; i < args.Length(); i++) {
		Local<Value> val = Local<Object>::Cast(args[i]);
		string item;

		if (val->IsArray() || val->IsObject()) {
			// stringify arrays and objects
			item = JSONStringify(isolate, val);
		} else {
			// just convert to string
			item = string(*String::Utf8Value(
				isolate,
				val->ToString(isolate->GetCurrentContext()).ToLocalChecked()
			));
		}

		// add to argument list
		objs.push_back(item);
	}

	// if stdout logging is enabled emit a log line
	if (logger->log_to_stdout) {
		log_stdout(level, now, hostname, pid, filename, function, line, column, objs, logger->tags);
	}

	// log to the database
	if (!connection->valid) {
		logger->rotate();
	}
	log_db(connection, level, now, hostname, pid, filename, function, line, column, objs, logger->tags);
}

/*
 * Logger class
 */

Persistent<Function> Logger::constructor;

Logger::Logger() {
	level = 0;
	log_to_stdout = false;
	tags = set<string>();
}

Logger::~Logger() {}

void Logger::Init(Local<Object> exports, Local<Value> module) {
	Isolate* isolate = exports->GetIsolate();

	// load the path module
	Local<Function> require = get_value_from_dict(isolate, module.As<Object>(), "require").As<Function>();
	Local<Value> args[] = { local_string(isolate, "path") };
	Local<Object> path = require->Call(isolate->GetCurrentContext(), module, 1, args).ToLocalChecked().As<Object>();

	// store path module in persistent handle for further use, this unbinds it from the isolate
	node_path.Reset(isolate, path);

	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
	tpl->SetClassName(local_string(isolate, "Logger"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	// Prototype logging functions
	NODE_SET_PROTOTYPE_METHOD(tpl, "trace", Trace);
	NODE_SET_PROTOTYPE_METHOD(tpl, "debug", Debug);
	NODE_SET_PROTOTYPE_METHOD(tpl, "info", Info);
	NODE_SET_PROTOTYPE_METHOD(tpl, "log", Log);
	NODE_SET_PROTOTYPE_METHOD(tpl, "warn", Warn);
	NODE_SET_PROTOTYPE_METHOD(tpl, "error", Error);
	NODE_SET_PROTOTYPE_METHOD(tpl, "fatal", Fatal);

	// Prototype tagging function
	NODE_SET_PROTOTYPE_METHOD(tpl, "tag", Tag);

	// Prototype log rotation function
	NODE_SET_PROTOTYPE_METHOD(tpl, "rotate", Rotate);

	// Return create function, set class name
	constructor.Reset(isolate, tpl->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
	auto result = exports->Set(
		isolate->GetCurrentContext(),
		local_string(isolate, "Logger"),
		tpl->GetFunction(isolate->GetCurrentContext()).ToLocalChecked()
	);
}

void Logger::New(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new Logger(...)`

		Logger* obj = new Logger();

		// if we have arguments:
		if (args.Length() > 0) {
			Local<Object> config = Local<Object>::Cast(args[0]);
			if (config->IsObject()) {
				// argument is an configuration object, re-initialize DB
				initializeDB(isolate, config);

				if (!connection) {
					// Invoked without configuration
					isolate->ThrowException(Exception::Error(local_string(isolate, "You have to provide a configuration object for the first instanciation of a logger.")));
					return;
				}

				// if the config object contains a `level` set the log level to that value
				Local<Value> level = get_value_from_dict(isolate, config, "level");
				if (level->IsNumber()) {
					obj->level = level->NumberValue(isolate->GetCurrentContext()).FromMaybe(0);
				} else {
					obj->level = connection->global_log_level;
				}

				// additionally log to stdout?
				Local<Value> stdout = get_value_from_dict(isolate, config, "stdout");
				if (stdout->IsBoolean()) {
					obj->log_to_stdout = stdout->BooleanValue(isolate);
				} else {
					obj->log_to_stdout = connection->log_to_stdout;
				}

				if (!connection->valid) {
					obj->log_to_stdout = true;
				}

				// logger name
				Local<Value> logger_name = get_value_from_dict(isolate, config, "logger");
				if (logger_name->IsString()) {
					connection->logger_name = get_string_from_value(isolate, logger_name);
				}
				if (!logger_name->IsString() || (connection->logger_name == "")) {
					connection->logger_name = "default";
				}
			} else if (config->IsNumber()) {
				// first argument is a number, assume this is the log level
				obj->level = config->NumberValue(isolate->GetCurrentContext()).FromMaybe(0);
				obj->log_to_stdout = connection->log_to_stdout;
			} else {
				obj->level = connection->global_log_level;
				obj->log_to_stdout = connection->log_to_stdout;
			}
		} else {
			obj->level = connection->global_log_level;
			obj->log_to_stdout = connection->log_to_stdout;
		}

		// return logger object
		obj->Wrap(args.This());
		args.GetReturnValue().Set(args.This());
	} else {
		// Invoked without `new`: do not allow that here
		isolate->ThrowException(Exception::Error(local_string(isolate, "Use `new` to create a logger object.")));
	}
}

/*
 * logging functions
 */

void Logger::Trace(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(10, logger, args);
}

void Logger::Debug(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(20, logger, args);
}

void Logger::Info(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(30, logger, args);
}

void Logger::Log(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(30, logger, args);
}

void Logger::Warn(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(40, logger, args);
}

void Logger::Error(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(50, logger, args);
}

void Logger::Fatal(const FunctionCallbackInfo<Value>& args) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(args.Holder());
	log(60, logger, args);
}

/*
 * Add tags, return new logger instance
 */

void Logger::Tag(const FunctionCallbackInfo<Value>& context) {
	Logger* logger = ObjectWrap::Unwrap<Logger>(context.Holder());
	Isolate* isolate = context.GetIsolate();

	// create a new logger
	const int argc = 0;
    Local<Value> argv[argc] = { };
    Local<Context> cx = isolate->GetCurrentContext();
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    Local<Object> result =
        cons->NewInstance(cx, argc, argv).ToLocalChecked();

	Logger* obj = ObjectWrap::Unwrap<Logger>(result);;

	// copy tags from parent
	for(auto tag : logger->tags) {
		obj->tags.insert(tag);
	}

	// copy settings from parent
	obj->log_to_stdout = logger->log_to_stdout;
	obj->level = logger->level;

	// add new tags from arguments
	for(int i = 0; i < context.Length(); i++) {
		Local<Value> val = Local<Object>::Cast(context[i]);
		string tag = get_string_from_value(isolate, val);
		obj->tags.insert(tag);
	}

	// return new instance
    context.GetReturnValue().Set(result);
}


/*
 * Log rotation support
 */

void Logger::Rotate(const FunctionCallbackInfo<Value>& context) {
	rotate();
}

void Logger::rotate(void) {
	if (connection != NULL) {
		auto new_connection = new DBConnection(
			connection->db_type,
			connection->db_host,
			connection->db_port,
			connection->db_user,
			connection->db_password,
			connection->db_name,
			connection->prefix,
			connection->logger_name
		);
		new_connection->logger_name = connection->logger_name;
		new_connection->global_log_level = connection->global_log_level;
		new_connection->log_to_stdout = connection->log_to_stdout;
		delete connection;
		connection = new_connection;
	}
}
