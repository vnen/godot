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

v8::Local<v8::Value> JavaScriptFunctions::variant_getter(v8::Isolate * p_isolate, const StringName & p_prop, Object *p_obj) {

	bool valid = false;
	String prop(p_prop);
	Variant result = p_obj->get(prop, &valid);

	if (valid) {
		return variant_to_js(p_isolate, result);
	}

	if ( p_obj->has_method(p_prop)) {
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

	Variant obj(unwrap_object(p_args.This()));

	if (!obj) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Empty JavaScript object"));
		return;
	}

	v8::String::Utf8Value method_val(p_args.Callee()->GetName());
	StringName method(*method_val);

	Vector<Variant*> args;
	Variant::CallError err;

	for (int i = 0; i < p_args.Length(); i++) {
		Variant arg = js_to_variant(isolate, p_args[i]);
		args.push_back(&arg);
	}

	Variant result = obj.call(method, (const Variant**)args.ptr(), args.size(), err);
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

		if (method->get_argument_count() != p_args.Length()) {
			isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Wrong number of arguments"));
			return;
		}

		for (int i = 0; i < p_args.Length(); i++) {
			Variant arg = js_to_variant(isolate, p_args[i]);
			args.push_back(new Variant(arg));
		}
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
	if(*value)
		print_line(*value);
}

void JavaScriptFunctions::range(const v8::FunctionCallbackInfo<v8::Value>& p_args) {

	v8::Isolate *isolate = p_args.GetIsolate();

	switch (p_args.Length()) {
		case 0:
			isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Too few arguments"));
			return;
		case 1: {

			if (!p_args[0]->IsInt32()) return;
			int count = p_args[0]->IntegerValue();
			if (count < 0) {
				p_args.GetReturnValue().Set(v8::Array::New(isolate));
				return;
			}

			v8::Local<v8::Array> arr = v8::Array::New(isolate, count);
			for (int i = 0; i < count; i++) {
				arr->Set(i, v8::Integer::New(isolate, i));
			}

			p_args.GetReturnValue().Set(arr);
		} break;

		case 2: {
			if (!p_args[0]->IsInt32()) return;
			if (!p_args[1]->IsInt32()) return;

			int from = p_args[0]->IntegerValue();
			int to = p_args[1]->IntegerValue();
			if (to < from) {
				p_args.GetReturnValue().Set(v8::Array::New(isolate));
				return;
			}

			v8::Local<v8::Array> arr = v8::Array::New(isolate, to - from);
			for (int i = from; i < to; i++) {
				arr->Set(i - from, v8::Integer::New(isolate, i));
			}

			p_args.GetReturnValue().Set(arr);
		} break;

		case 3: {
			if (!p_args[0]->IsInt32()) return;
			if (!p_args[1]->IsInt32()) return;
			if (!p_args[2]->IsInt32()) return;

			int from = p_args[0]->IntegerValue();
			int to = p_args[1]->IntegerValue();
			int incr = p_args[2]->IntegerValue();

			if (incr == 0) {
				isolate->ThrowException(v8::String::NewFromUtf8(isolate, "step argument is zero!"));
				return;
			}

			if (from >= to && incr>0) {
				p_args.GetReturnValue().Set(v8::Array::New(isolate));
				return;
			}
			if (from <= to && incr<0) {
				p_args.GetReturnValue().Set(v8::Array::New(isolate));
				return;
			}

			//calculate how many
			int count = 0;
			if (incr>0) {
				count = ((to - from - 1) / incr) + 1;
			} else {
				count = ((from - to - 1) / -incr) + 1;
			}

			v8::Local<v8::Array> arr = v8::Array::New(isolate, count);

			int idx = 0;
			if (incr>0) {
				for (int i = from;i<to;i += incr) {
					arr->Set(idx++, v8::Integer::New(isolate, i));
				}
			} else {
				for (int i = from;i>to;i += incr) {
					arr->Set(idx++, v8::Integer::New(isolate, i));
				}
			}
			p_args.GetReturnValue().Set(arr);
		} break;
		default: {
			isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Too much arguments"));
			return;
		}
	}
}
