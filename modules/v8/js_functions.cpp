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
#include "js_language.h"

#include "math_2d.h"
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

	v8::EscapableHandleScope scope(p_isolate);

	v8::Local<v8::Value> val;

	switch (p_var.get_type()) {
		case Variant::BOOL: {
			val = v8::Boolean::New(p_isolate, bool(p_var));
		} break;
		case Variant::STRING: {
			val = v8::String::NewFromUtf8(p_isolate, String(p_var).utf8().get_data());
		} break;
		case Variant::INT: {
			val = v8::Integer::New(p_isolate, int(p_var));
		} break;
		case Variant::REAL: {
			val = v8::Number::New(p_isolate, double(p_var));
		} break;
		case Variant::OBJECT: {
			Object *obj = (Object*)p_var;

			if (obj) {
				v8::Local<v8::Context> ctx = p_isolate->GetCurrentContext();
				v8::Local<v8::Function> constructor = v8::Local<v8::Function>::Cast(ctx->Global()->Get(
					v8::String::NewFromUtf8(p_isolate, obj->get_type().utf8().get_data())));

				// Call the constructor with the object as argument
				v8::Local<v8::Value> cargs[] = { v8::External::New(p_isolate, obj) };
				v8::Local<v8::Object> instance = v8::Local<v8::Object>::Cast(constructor->CallAsConstructor(1, cargs));
				// Set as external to JavaScript
				instance->SetInternalField(1, v8::Boolean::New(p_isolate, false));
				val = instance;
			}
		} break;
		case Variant::VECTOR2: {
			Vector2 vec = p_var;

			v8::Local<v8::Context> ctx = p_isolate->GetCurrentContext();
			v8::Local<v8::Function> constructor = v8::Local<v8::Function>::Cast(ctx->Global()->Get(
				v8::String::NewFromUtf8(p_isolate, "Vector2")));

			// Call the constructor with the Vector2 arguments
			v8::Local<v8::Value> cargs[] = { v8::Number::New(p_isolate, vec.x), v8::Number::New(p_isolate, vec.y) };
			v8::Local<v8::Object> instance = v8::Local<v8::Object>::Cast(constructor->CallAsConstructor(2, cargs));

			val = instance;
		} break;
		default: {
			val = v8::Null(p_isolate);
		}
	}

	return scope.Escape(val);
}

Variant JavaScriptFunctions::js_to_variant(v8::Isolate * p_isolate, const v8::Local<v8::Value>& p_value) {

	if (p_value->IsNull() || p_value->IsUndefined()) {
		return Variant();
	}
	if (p_value->IsBoolean()) {
		return Variant(p_value->BooleanValue());
	}
	if (p_value->IsString()) {
		v8::String::Utf8Value v(p_value);
		return Variant(String(*v));
	}
	if (p_value->IsInt32() || p_value->IsUint32()) {
		return Variant(p_value->IntegerValue());
	}
	if (p_value->IsNumber()) {
		return Variant(p_value->NumberValue());
	}
	if (p_value->IsObject()) {
		if (v8::Local<v8::Object>::Cast(p_value)->InternalFieldCount() == 2) {
			Object *obj = unwrap_object(v8::Local<v8::Object>::Cast(p_value));
			return Variant(obj);
		}
	}

	return Variant();
}

/****** JAVASCRIPT FUNCTIONS ******/

void JavaScriptFunctions::require(const v8::FunctionCallbackInfo<v8::Value>& p_args) {
	v8::Isolate* isolate = p_args.GetIsolate();

	v8::String::Utf8Value name(isolate->GetCallingContext()->Global()->Get(v8::String::NewFromUtf8(isolate, "__dirname")));
	print_line(*name);

	p_args.GetReturnValue().SetUndefined();
}

void JavaScriptFunctions::print(const v8::FunctionCallbackInfo<v8::Value>& args) {

	if (args.Length() < 1) return;

	v8::HandleScope scope(args.GetIsolate());
	v8::Local<v8::Value> arg = args[0];
	v8::String::Utf8Value value(arg);
	if(*value)
		print_line(*value);
}

void JavaScriptFunctions::Vector2_constructor(const v8::FunctionCallbackInfo<v8::Value>& p_args) {
	v8::Isolate* isolate = p_args.GetIsolate();

	if (!p_args.IsConstructCall()) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Can't call type as a function"));
		return;
	}

	switch (p_args.Length()) {
		case 0: {
			p_args.This()->SetInternalField(0, v8::External::New(isolate, memnew(Vector2)));
			return;
		} break;
		case 2: {
			Vector2 *vec = memnew(Vector2);
			vec->x = p_args[0]->NumberValue();
			vec->y = p_args[1]->NumberValue();
			p_args.This()->SetInternalField(0, v8::External::New(isolate, vec));
			return;
		} break;
		default:
			break;
	}
}

void JavaScriptFunctions::Vector2_add(const v8::FunctionCallbackInfo<v8::Value>& p_args) {}

void JavaScriptFunctions::Vector2_length(const v8::FunctionCallbackInfo<v8::Value>& p_args) {}

void JavaScriptFunctions::Vector2_length_squared(const v8::FunctionCallbackInfo<v8::Value>& p_args) {

	v8::Isolate* isolate = p_args.GetIsolate();

	if (p_args.Length() != 0) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Too many arguments"));
		return;
	}

	Vector2* vec = static_cast<Vector2*>(v8::Local<v8::External>::Cast(p_args.This()->GetInternalField(0))->Value());

	p_args.GetReturnValue().Set(vec->length_squared());
}
