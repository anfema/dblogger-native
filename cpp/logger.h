#ifndef LOGGER_H
#define LOGGER_H

#include <set>
#include <string>

#include <node.h>
#include <node_object_wrap.h>

using v8::Local;
using v8::Object;
using v8::Function;
using v8::Value;
using v8::Persistent;
using v8::FunctionCallbackInfo;
using std::set;
using std::string;

class Logger : public node::ObjectWrap {
	public:
		static void Init(Local<Object> exports, Local<Object> module);
		bool log_to_stdout;
		set<string> tags;
		int level;

	private:
		explicit Logger();
		~Logger();

		static Persistent<Function> constructor;
		static void New(const FunctionCallbackInfo<Value>& info);

		static void Tag(const FunctionCallbackInfo<Value>& info);
		static void Rotate(const FunctionCallbackInfo<Value>& info);

		static void Trace(const FunctionCallbackInfo<Value>& info);
		static void Debug(const FunctionCallbackInfo<Value>& info);
		static void Info(const FunctionCallbackInfo<Value>& info);
		static void Log(const FunctionCallbackInfo<Value>& info);
		static void Warn(const FunctionCallbackInfo<Value>& info);
		static void Error(const FunctionCallbackInfo<Value>& info);
		static void Fatal(const FunctionCallbackInfo<Value>& info);
};

#endif
