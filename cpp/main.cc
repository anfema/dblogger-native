#include <node.h>
#include "logger.h"

using v8::Local;
using v8::Object;

void InitAll(Local<Object> exports, Local<Object> module) {
  Logger::Init(exports, module);
}

NODE_MODULE(dblogger, InitAll)

