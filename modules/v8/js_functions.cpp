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

#include "object_type_db.h"
#include "variant.h"
#include "vector.h"
#include "print_string.h"

#include "v8.h"

Object * JavaScriptFunctions::unwrap_object(const v8::Local<v8::Object>& p_value) {

	v8::Local<v8::External> field = v8::Local<v8::External>::Cast(p_value->GetInternalField(0));
	void* ptr = field->Value();
	return static_cast<Object*>(ptr);
}

v8::Local<v8::Value> JavaScriptFunctions::variant_to_js(v8::Isolate * p_isolate, const Variant & p_var) {
	return v8::Integer::New(p_isolate, 42);
}

Variant JavaScriptFunctions::js_to_variant(v8::Isolate * p_isolate, const v8::Local<v8::Value>& p_value) {
	return Variant(42);
}

v8::Local<v8::Value> JavaScriptFunctions::variant_getter(v8::Isolate * p_isolate, const StringName & p_prop, Variant & p_var) {

	bool valid = false;
	String prop(p_prop);
	Variant result = p_var.get(prop, &valid);

	if (valid) {
		return variant_to_js(p_isolate, result);
	}

	if (p_var.has_method(p_prop)) {
		v8::EscapableHandleScope escape_scope(p_isolate);
		v8::Local<v8::Function> func = v8::FunctionTemplate::New(p_isolate, JavaScriptFunctions::variant_call)->GetFunction();
		func->SetName(v8::String::NewFromUtf8(p_isolate, prop.utf8().get_data()));

		return escape_scope.Escape(func);
	}

	return v8::Local<v8::Value>();
}

v8::Local<v8::Value> JavaScriptFunctions::object_getter(v8::Isolate * p_isolate, const StringName & p_prop, Object * p_obj) {

	bool valid = false;
	String prop(p_prop);
	Variant result = p_obj->get(prop, &valid);

	if (valid) {
		return variant_to_js(p_isolate, result);
	}

	if (p_obj->has_method(p_prop)) {
		v8::EscapableHandleScope escape_scope(p_isolate);
		v8::Local<v8::Function> func = v8::FunctionTemplate::New(p_isolate, JavaScriptFunctions::object_call)->GetFunction();
		func->SetName(v8::String::NewFromUtf8(p_isolate, prop.utf8().get_data()));

		return escape_scope.Escape(func);
	}

	return v8::Local<v8::Value>();
}

v8::Local<v8::Value> JavaScriptFunctions::object_setter(v8::Isolate * p_isolate, const StringName & p_prop, v8::Local<v8::Value>& p_value, Object * p_obj) {

	bool valid = false;
	String prop(p_prop);
	Variant result = p_obj->get(prop, &valid);

	if (valid) {
		p_obj->set(p_prop, js_to_variant(p_isolate, p_value), &valid);

		if (valid) {
			return p_value;
		}
	}

	return v8::Local<v8::Value>();
}

void JavaScriptFunctions::variant_call(const v8::FunctionCallbackInfo<v8::Value>& p_args) {

	v8::Isolate *isolate = p_args.GetIsolate();

	if (p_args.This()->InternalFieldCount() != 2) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Invalid JavaScript object"));
		return;
	}

	Object* obj = unwrap_object(p_args.This());

	if (!obj) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Empty JavaScript object"));
		return;
	}

	v8::String::Utf8Value method_val(p_args.Callee()->GetName());
	StringName method(*method_val);

	Vector<Variant*> args;
	Variant::CallError err;

	Variant result = Variant(obj).call(method, (const Variant**)args.ptr(), args.size(), err);
	if (err.error == Variant::CallError::CALL_OK) {
		p_args.GetReturnValue().Set(variant_to_js(isolate, result));
	}
}

void JavaScriptFunctions::object_call(const v8::FunctionCallbackInfo<v8::Value>& p_args) {

	v8::Isolate *isolate = p_args.GetIsolate();

	if (p_args.This()->InternalFieldCount() != 2) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Invalid JavaScript object"));
		return;
	}

	Object* obj = unwrap_object(p_args.This());

	if (!obj) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Empty JavaScript object"));
		return;
	}

	v8::String::Utf8Value method_val(p_args.Callee()->GetName());
	StringName method_name(*method_val);

	MethodBind *method = ObjectTypeDB::get_method(obj->get_type_name(), method_name);
	if (method) {
		Vector<Variant*> args;
		Variant::CallError err;

		Variant result = method->call(obj, (const Variant**)args.ptr(), args.size(), err);
		if (err.error == Variant::CallError::CALL_OK) {
			p_args.GetReturnValue().Set(variant_to_js(isolate, result));
		}
	}
}

/****** JAVASCRIPT FUNCTIONS ******/

void JavaScriptFunctions::print(const v8::FunctionCallbackInfo<v8::Value>& args) {

	if (args.Length() < 1) return;

	v8::HandleScope scope(args.GetIsolate());
	v8::Local<v8::Value> arg = args[0];
	v8::String::Utf8Value value(arg);
	print_line(*value);
}