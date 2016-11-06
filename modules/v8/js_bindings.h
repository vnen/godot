/*************************************************************************/
/*  js_bindings.h                                                        */
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

#ifndef JS_BINDINGS_H
#define JS_BINDINGS_H

#include "v8.h"

class Variant;

class JavaScriptAccessors {

	static JavaScriptAccessors* singleton;

public:

	static void get_string(v8::Local<v8::String> p_name, const v8::PropertyCallbackInfo<v8::Value>& p_info);
	static void set_string(v8::Local<v8::String> p_name, v8::Local<v8::Value> p_value, const v8::PropertyCallbackInfo<void>& p_info);

	static JavaScriptAccessors* get_singleton() { return singleton; }

	static v8::Local<v8::Value> wrap_variant(v8::Isolate *p_isolate, const v8::Local<v8::ObjectTemplate> p_constructor, Variant *p_variant);
	static Variant* unwrap_variant(const v8::Local<v8::Object> &p_object);

	static v8::Local<v8::Value> variant_to_js(v8::Isolate* p_isolate, Variant &p_variant);
	static Variant &js_to_variant(v8::Isolate* p_isolate, v8::Local<v8::Value> p_value);

	JavaScriptAccessors(v8::Isolate* p_isolate);
	~JavaScriptAccessors();

};

class JavaScriptBinding {

public:

	static v8::Global<v8::FunctionTemplate> Node2D_template;
	static void Node2D_constructor(const v8::FunctionCallbackInfo<v8::Value>& p_args);
};

#endif
