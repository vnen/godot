/*************************************************************************/
/*  js_functions.cpp                                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "js_functions.h"

#include "print_string.h"

#include "v8.h"

v8::Local<v8::Value> JavaScriptFunctions::variant_to_js(v8::Isolate * p_isolate, const Variant & p_var) {
	return v8::Undefined(p_isolate);
}

Variant & JavaScriptFunctions::js_to_variant(v8::Isolate * p_isolate, const v8::Local<v8::Value>& p_value) {
	return Variant();
}

v8::Local<v8::Value> JavaScriptFunctions::variant_getter(v8::Isolate * p_isolate, const StringName & p_prop, const Variant & p_var) {
	return v8::Undefined(p_isolate);
}

v8::Local<v8::Value> JavaScriptFunctions::object_getter(v8::Isolate * p_isolate, const StringName & p_prop, const Object * p_var) {
	return v8::Undefined(p_isolate);
}

void JavaScriptFunctions::variant_call(const v8::FunctionCallbackInfo<v8::Value>& args) {}

void JavaScriptFunctions::print(const v8::FunctionCallbackInfo<v8::Value>& args) {

	if (args.Length() < 1) return;

	v8::HandleScope scope(args.GetIsolate());
	v8::Local<v8::Value> arg = args[0];
	v8::String::Utf8Value value(arg);
	print_line(*value);
}