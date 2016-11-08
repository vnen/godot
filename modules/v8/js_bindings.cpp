/*************************************************************************/
/*  js_bindings.cpp                                                      */
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

#include "js_bindings.h"
#include "variant.h"
#include "scene/2d/node_2d.h"
#include "object_type_db.h"
#include "core_string_names.h"
#include "v8.h"

JavaScriptAccessors* JavaScriptAccessors::singleton = NULL;
//v8::Global<v8::FunctionTemplate> JavaScriptBinding::Node2D_type;
v8::Global<v8::FunctionTemplate> JavaScriptBinding::Node2D_template;

void JavaScriptAccessors::get_string(v8::Local<v8::String> p_name, const v8::PropertyCallbackInfo<v8::Value>& p_info) {

	v8::Local<v8::Object> self = p_info.Holder();
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();

	v8::String::Utf8Value str(p_name);
	String prop(*str);
	bool valid = false;

	Variant s = static_cast<Variant*>(ptr)->get(prop, &valid);

	if (valid) {
		v8::Local<v8::String> result = v8::String::NewFromUtf8(p_info.GetIsolate(), String(s).utf8().get_data());
		p_info.GetReturnValue().Set(result);
	}
}

void JavaScriptAccessors::set_string(v8::Local<v8::String> p_name, v8::Local<v8::Value> p_value, const v8::PropertyCallbackInfo<void>& p_info) {

	v8::Local<v8::Object> self = p_info.Holder();
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();

	v8::String::Utf8Value str(p_name);
	v8::String::Utf8Value strval(p_value);
	String prop(*str);
	String val(*strval);
	bool valid = false;

	static_cast<Variant*>(ptr)->set(prop, val, &valid);
}

v8::Local<v8::Value> JavaScriptAccessors::wrap_variant(v8::Isolate *p_isolate, const v8::Local<v8::ObjectTemplate> p_constructor, Variant *p_variant) {

	v8::EscapableHandleScope scope(p_isolate);

	v8::Local<v8::Object> obj = p_constructor->NewInstance();
	obj->SetInternalField(0, v8::External::New(p_isolate, p_variant));

	return scope.Escape(obj);
}

Variant* JavaScriptAccessors::unwrap_variant(const v8::Local<v8::Object>& p_object) {

	v8::Local<v8::External> ext = v8::Local<v8::External>::Cast(p_object->GetInternalField(0));
	void* ptr = ext->Value();
	return static_cast<Variant*>(ptr);
}

v8::Local<v8::Value> JavaScriptAccessors::variant_to_js(v8::Isolate * p_isolate, Variant & p_variant) {

	v8::EscapableHandleScope scope(p_isolate);

	v8::Local<v8::Value> val;

	switch (p_variant.get_type()) {
		case Variant::BOOL: {
			val = v8::Boolean::New(p_isolate, bool(p_variant));
		} break;
		case Variant::STRING: {
			val = v8::String::NewFromUtf8(p_isolate, String(p_variant).utf8().get_data());
		} break;
		case Variant::INT: {
			val = v8::Integer::New(p_isolate, int(p_variant));
		} break;
		case Variant::REAL: {
			val = v8::Number::New(p_isolate, double(p_variant));
		} break;
		default: {
			val = v8::Null(p_isolate);
		}
	}

	return scope.Escape(val);
}

JavaScriptAccessors::JavaScriptAccessors(v8::Isolate* p_isolate) {

	singleton = this;

	v8::HandleScope scope(p_isolate);
	v8::EscapableHandleScope handle_scope(p_isolate);

	v8::Local<v8::FunctionTemplate> Node2D_template_func = v8::FunctionTemplate::New(p_isolate, JavaScriptBinding::Node2D_constructor);
	v8::Local<v8::ObjectTemplate> Node2D_template = Node2D_template_func->PrototypeTemplate();
	Node2D_template_func->InstanceTemplate()->SetInternalFieldCount(2);
	Node2D_template->SetAccessor(v8::String::NewFromUtf8(p_isolate, "name"), JavaScriptAccessors::get_string, JavaScriptAccessors::set_string);

	//v8::Local<v8::Signature> sig = v8::Signature::New(p_isolate);


	Node2D_template->Set(v8::String::NewFromUtf8(p_isolate, "get_name"), v8::FunctionTemplate::New(p_isolate, JavaScriptBinding::get_name
		, v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kThrow)
	, v8::PropertyAttribute::DontEnum);

	JavaScriptBinding::Node2D_template.Reset(p_isolate, handle_scope.Escape(Node2D_template_func));

}

JavaScriptAccessors::~JavaScriptAccessors() {}

void JavaScriptBinding::Node2D_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {

	v8::Isolate *isolate = args.GetIsolate();

	if (!args.IsConstructCall()) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Cannot call constructor as function"));
		return;
	}

	v8::HandleScope scope(isolate);

	args.This()->SetInternalField(1, v8::Boolean::New(isolate, false));
	args.GetReturnValue().Set(args.This());
}

void JavaScriptBinding::get_name(const v8::FunctionCallbackInfo<v8::Value>& p_args) {

	v8::Isolate *isolate = p_args.GetIsolate();

	if (p_args.This()->InternalFieldCount() != 2) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "invalid call"));
	}

	v8::Local<v8::External> field = v8::Local<v8::External>::Cast(p_args.This()->GetInternalField(0));
	void* ptr = field->Value();
	Node2D* node2d = static_cast<Node2D*>(ptr);

	v8::String::Utf8Value holder(p_args.Callee()->GetName());
	StringName method_name(*holder);

	// Is internal
	if (v8::Local<v8::Boolean>::Cast(p_args.This()->GetInternalField(1))->IsTrue()) {
		MethodBind* method = ObjectTypeDB::get_method(node2d->get_type_name(), method_name);
		Variant::CallError err;

		if (method) {
			v8::Local<v8::Value> js_result = JavaScriptAccessors::variant_to_js(isolate, method->call(node2d, NULL, 0, err));
			p_args.GetReturnValue().Set(js_result);
		}
		p_args.GetReturnValue().Set(v8::Undefined(isolate));
	} else {
		v8::Local<v8::Value> js_result = JavaScriptAccessors::variant_to_js(isolate, Variant(node2d).call(method_name));
		p_args.GetReturnValue().Set(js_result);
	}
}
