#include <node.h>
#include "logger.h"

using v8::Local;
using v8::Object;
using v8::Value;

void InitAll(Local<Object> exports, Local<Value> module, void *priv) {
  Logger::Init(exports, module);
}

NODE_MODULE(dblogger, InitAll)

