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
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;
using v8::Exception;
using v8::Handle;
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

// Initialize DB connection, will terminate and overwrite the old connection
static inline void initializeDB(Isolate *isolate, const Handle<Object> config) {
	// unpack config object
	string db_host = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "host"))
	));
	int db_port = config->Get(String::NewFromUtf8(isolate, "port"))->NumberValue();
	string db_user = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "user"))
	));
	string db_password = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "password"))
	));
	string db_name = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "name"))
	));
	string db_type = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "type"))
	));
	string prefix = string(*String::Utf8Value(
		config->Get(String::NewFromUtf8(isolate, "tablePrefix"))
	));

	if (prefix == "undefined") {
		prefix = string("logger");
	}

	if (db_type == "undefined") {
		// no db type, do not reinitialize
		return;
	}

	// close old connection if set
	if (connection != NULL) {
		delete connection;
		connection = NULL;
	}

	// create new connection
	connection = new DBConnection(db_type, db_host, db_port, db_user, db_password, db_name, prefix);
}

// JSON.stringify() a value
static string JSONStringify(Isolate *isolate, Handle<Value> obj) {
	// get the global JSON object
	Local<Object> json = isolate->GetCurrentContext()->Global()->Get(String::NewFromUtf8(isolate, "JSON"))->ToObject();

	// fetch `stringify` function
	Local<Function> stringify = json->Get(String::NewFromUtf8(isolate, "stringify")).As<Function>();

	// call stringify on the obj
	Local<Value> result = stringify->Call(json, 1, &obj);

	// return the result
	return string(*String::Utf8Value(result));
}

// Use the node internal path.relative() function to calculate a relative path
static string relativePath(Isolate *isolate, Handle<Value> to, char *from) {
	// get the unbound path module inserted into isolate
	Local<Object> path = Local<Object>::New(isolate, node_path);

	// fetch the `relative` function
	Local<Function> relative = path->Get(String::NewFromUtf8(isolate, "relative")).As<Function>();

	// call path.relative(from, to)
	Local<Value> args[2];
	args[0] = String::NewFromUtf8(isolate, from);
	args[1] = to;

	Local<Value> result = relative->Call(path, 2, args);

	// If the result is valid return it
	if (result->IsString()) {
		return string(*String::Utf8Value(result));
	}

	// invalid result return `to`
	return string(*String::Utf8Value(to));
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
	Local<StackFrame> frame = StackTrace::CurrentStackTrace(isolate, 1, StackTrace::kOverview)->GetFrame(0);
	char c_path[1024] = {}; getcwd(c_path, 1024);
	string filename = relativePath(isolate, frame->GetScriptName(), c_path);
	string function = string(*String::Utf8Value(frame->GetFunctionName())) + "()";
	int line = frame->GetLineNumber();
	int column = frame->GetColumn();

	if (function == "()") {
		// if we get no function the call was from the toplevel scope
		function = "<global scope>";
	}

	// convert all arguments to readable values (JSON.stringify objects and arrays)
	vector<string> objs = vector<string>();
	for(int i = 0; i < args.Length(); i++) {
		Handle<Value> val = Handle<Object>::Cast(args[i]);
		string item;

		if (val->IsArray() || val->IsObject()) {
			// stringify arrays and objects
			item = JSONStringify(isolate, val);
		} else {
			// just convert to string
			item = string(*String::Utf8Value(
				val->ToString()
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

void Logger::Init(Local<Object> exports, Local<Object> module) {
	Isolate* isolate = exports->GetIsolate();

	// load the path module
	Local<Function> require = module->Get(String::NewFromUtf8(isolate, "require")).As<Function>();
	Local<Value> args[] = { String::NewFromUtf8(isolate, "path") };
	Local<Object> path = require->Call(module, 1, args).As<Object>();

	// store path module in persistent handle for further use, this unbinds it from the isolate
	node_path.Reset(isolate, path);

	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
	tpl->SetClassName(String::NewFromUtf8(isolate, "Logger"));
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
	constructor.Reset(isolate, tpl->GetFunction());
	exports->Set(String::NewFromUtf8(isolate, "Logger"),
	tpl->GetFunction());
}

void Logger::New(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new Logger(...)`

		Logger* obj = new Logger();

		// if we have arguments:
		if (args.Length() > 0) {
			Handle<Object> config = Handle<Object>::Cast(args[0]);
			if (config->IsObject()) {
				// argument is an configuration object, re-initialize DB
				initializeDB(isolate, config);

				// if the config object contains a `level` set the log level to that value
				Handle<Value> level = config->Get(String::NewFromUtf8(isolate, "level"));
				if (level->IsNumber()) {
					obj->level = level->NumberValue();
				}

				// additionally log to stdout?
				Handle<Value> stdout = config->Get(String::NewFromUtf8(isolate, "stdout"));
				obj->log_to_stdout = stdout->BooleanValue();

				if (!connection->valid) {
					obj->log_to_stdout = true;
				}

				// logger name
				Handle<Value> logger_name = config->Get(String::NewFromUtf8(isolate, "logger"));
				if (logger_name->IsString()) {
					connection->logger_name = string(*String::Utf8Value(logger_name->ToString()));
				}
				if (!logger_name->IsString() || (connection->logger_name == "")) {
					connection->logger_name = "default";
				}
			}
			if (config->IsNumber()) {
				// first argument is a number, assume this is the log level
				obj->level = config->NumberValue();
			}
		}

		// return logger object
		obj->Wrap(args.This());
		args.GetReturnValue().Set(args.This());
	} else {
		// Invoked without `new`: do not allow that here
		isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Use `new` to create a logger object.")));
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
		Handle<Value> val = Handle<Object>::Cast(context[i]);
		string tag = string(*String::Utf8Value(
			val->ToString())
		);
		obj->tags.insert(tag);
	}

	// return new instance
    context.GetReturnValue().Set(result);
}


/*
 * Log rotation support
 */

void Logger::Rotate(const FunctionCallbackInfo<Value>& context) {
	if (connection != NULL) {
		auto new_connection = new DBConnection(
			connection->db_type,
			connection->db_host,
			connection->db_port,
			connection->db_user,
			connection->db_password,
			connection->db_name,
			connection->prefix
		);
		delete connection;
		connection = new_connection;
	}
}
