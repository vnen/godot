#ifndef JS_FUNCTIONS_H
#define JS_FUNCTIONS_H

#include "v8.h";

class JavaScriptFunctions {

public:

	static void print(const v8::FunctionCallbackInfo<v8::Value>& args);
};

#endif
