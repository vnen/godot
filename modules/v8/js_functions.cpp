
#include "js_functions.h"

#include "print_string.h"

#include "v8.h"

void JavaScriptFunctions::print(const v8::FunctionCallbackInfo<v8::Value>& args) {

	if (args.Length() < 1) return;

	v8::HandleScope scope(args.GetIsolate());
	v8::Local<v8::Value> arg = args[0];
	v8::String::Utf8Value value(arg);
	print_line(*value);
}