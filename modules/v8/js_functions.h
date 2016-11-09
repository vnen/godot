/*************************************************************************/
/*  js_functions.h                                                       */
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

#ifndef JS_FUNCTIONS_H
#define JS_FUNCTIONS_H

#include "object.h"
#include "variant.h"

#include "v8.h"

class JavaScriptFunctions {

	friend class JavaScriptLanguage;
	static Object* unwrap_object(const v8::Local<v8::Object> &p_value);

public:

	/****** VARIANT <-> JAVASCRIPT ******/

	// Convert a Variant type to a JavaScript type
	static v8::Local<v8::Value> variant_to_js(v8::Isolate* p_isolate, const Variant &p_var);
	// Convert a JavaScript type to a Variant type
	static Variant js_to_variant(v8::Isolate* p_isolate, const v8::Local<v8::Value> &p_value);
	// Use Variant get/call depending if it's property or method
	static v8::Local<v8::Value> variant_getter(v8::Isolate* p_isolate, const StringName &p_prop, Object *p_obj);
	// Use Object reflection to get/call depending if it's property or method
	static v8::Local<v8::Value> object_getter(v8::Isolate* p_isolate, const StringName &p_prop, Object *p_obj);
	// Setter for any object
	static v8::Local<v8::Value> object_setter(v8::Isolate* p_isolate, const StringName &p_prop, v8::Local<v8::Value> &p_value, Object *p_obj);
	// Variant call method implementation
	static void variant_call(const v8::FunctionCallbackInfo<v8::Value>& p_args);
	// Object call method implementation
	static void object_call(const v8::FunctionCallbackInfo<v8::Value>& p_args);

	/****** JAVASCRIPT GLOBAL FUNCTIONS ******/

	static void print(const v8::FunctionCallbackInfo<v8::Value>& p_args);
};

#endif
