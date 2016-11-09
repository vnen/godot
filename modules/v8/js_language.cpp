/*************************************************************************/
/*  js_language.cpp                                                      */
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

#include "js_language.h"
#include "js_functions.h"
#include "js_bindings.h"
#include "os/file_access.h"
#include "io/file_access_encrypted.h"
#include "globals.h"
#include "core_string_names.h"
#include "core/os/os.h"
#include "object_type_db.h"

#include "v8.h"
#include "libplatform/libplatform.h"

JavaScriptLanguage* JavaScriptLanguage::singleton = NULL;
Map<String, StringName> JavaScriptLanguage::types;

void JavaScriptLanguage::_add_class(const StringName &p_type, const v8::Local<v8::FunctionTemplate> &p_parent) {

	String type(p_type);
	// Ignore proxy classes
	if (type.begins_with("_")) return;
	// Change object name to avoid conflict with JavaScript's own Object
	if (p_type == "Object") type = String("GodotObject");

	types.insert(type, p_type);

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);
	v8::EscapableHandleScope escapable_scope(isolate);

	v8::Local<v8::FunctionTemplate> local_constructor = v8::FunctionTemplate::New(isolate, Bindings::js_constructor);
	local_constructor->InstanceTemplate()->SetInternalFieldCount(2);
	local_constructor->PrototypeTemplate()->SetHandler(v8::NamedPropertyHandlerConfiguration(Bindings::js_getter, Bindings::js_setter));

	local_constructor = escapable_scope.Escape(local_constructor);

	if (!p_parent.IsEmpty()) {
		local_constructor->Inherit(p_parent);
	}

	global_template.Get(isolate)->Set(v8::String::NewFromUtf8(isolate, type.utf8().get_data()), local_constructor, v8::PropertyAttribute::ReadOnly);

	List<StringName> sub_types;
	ObjectTypeDB::get_inheriters_from(p_type, &sub_types);

	for (List<StringName>::Element *E = sub_types.front(); E; E = E->next()) {
		StringName parent = ObjectTypeDB::type_inherits_from(E->get());
		if (parent != p_type) continue;
		_add_class(E->get(), local_constructor);
	}
}

void JavaScriptLanguage::init() {

	// Initialize V8.
	v8::V8::InitializeICUDefaultLocation(OS::get_singleton()->get_executable_path().utf8().get_data());
	v8::V8::InitializeExternalStartupData(OS::get_singleton()->get_executable_path().utf8().get_data());
	platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(platform);
	v8::V8::Initialize();

	// Create the isolate
	create_params.array_buffer_allocator = &allocator;

	isolate = v8::Isolate::New(create_params);
	isolate->Enter();

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope scope(isolate);
	//v8::EscapableHandleScope escapable_scope(isolate);

	// Global template
	v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate, v8::FunctionTemplate::New(isolate, Bindings::js_constructor));

	// Add global functions
	global->Set(v8::String::NewFromUtf8(isolate, "print"), v8::FunctionTemplate::New(isolate, JavaScriptFunctions::print), v8::PropertyAttribute::ReadOnly);

	global_template.Set(isolate, global);
	v8::Local<v8::ObjectTemplate> shallow = v8::ObjectTemplate::New(isolate);

	// Register type bindings in a tree fashion
	_add_class(StringName("Object"));
}

void JavaScriptLanguage::finish() {

	using namespace v8;

	isolate->Dispose();
	V8::Dispose();
	V8::ShutdownPlatform();
	delete platform;
}

String JavaScriptLanguage::get_type() const {

	return "JavaScript";
}

String JavaScriptLanguage::get_extension() const {

	return "js";
}

Error JavaScriptLanguage::execute_file(const String & p_path) {

	return OK;
}

void JavaScriptLanguage::add_global_constant(const StringName & p_variable, const Variant & p_value) {}

void JavaScriptLanguage::reload_all_scripts() {}

void JavaScriptLanguage::reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload) {}

void JavaScriptLanguage::profiling_start() {}

void JavaScriptLanguage::profiling_stop() {}

int JavaScriptLanguage::profiling_get_accumulated_data(ProfilingInfo * p_info_arr, int p_info_max) {
	return 0;
}

int JavaScriptLanguage::profiling_get_frame_data(ProfilingInfo * p_info_arr, int p_info_max) {
	return 0;
}

void JavaScriptLanguage::frame() {}

JavaScriptLanguage::JavaScriptLanguage() {

	ERR_FAIL_COND(singleton);
	singleton = this;
}

JavaScriptLanguage::~JavaScriptLanguage() {

	singleton = NULL;
}

void JavaScript::_update_exports() {


}

void JavaScript::_bind_methods() {}

bool JavaScript::can_instance() const {

	return compiled || (!tool && !ScriptServer::is_scripting_enabled());
}

Variant JavaScript::call(const StringName & p_method, const Variant ** p_args, int p_argcount, Variant::CallError & r_error) {
	return Script::call(p_method, p_args, p_argcount, r_error);
}

Ref<Script> JavaScript::get_base_script() const {
	return Ref<Script>();
}

StringName JavaScript::get_instance_base_type() const {
	return StringName();
}

ScriptInstance * JavaScript::instance_create(Object * p_this) {

	if (!tool && !ScriptServer::is_scripting_enabled()) {

#ifdef TOOLS_ENABLED

		PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(JavaScriptLanguage::get_singleton(), Ref<Script>(this), p_this));
		placeholders.insert(si);
		_update_exports();
		return si;
#else
		return NULL;
#endif
	}

	ERR_FAIL_COND_V(!compiled, NULL);

	JavaScriptInstance* instance = memnew(JavaScriptInstance);

	instance->owner = p_this;
	instance->script = Ref<JavaScript>(this);

	instance->_run();

	instances.insert(p_this);

	return instance;
}

bool JavaScript::instance_has(const Object * p_this) const {

	return instances.has((Object*)p_this);
}

bool JavaScript::has_source_code() const {

	return !source.empty();
}

String JavaScript::get_source_code() const {

	return source;
}

void JavaScript::set_source_code(const String & p_code) {

	if (source == p_code)
		return;
	source = p_code;
}

Error JavaScript::reload(bool p_keep_state) {

	ERR_FAIL_COND_V(!p_keep_state && instances.size(), ERR_ALREADY_IN_USE);

	compiled = false;

	if (!has_source_code()) {
		return ERR_COMPILATION_FAILED;
	}

	v8::Isolate* isolate = JavaScriptLanguage::get_singleton()->isolate;
	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);

	v8::Local<v8::Context> ctx = v8::Context::New(isolate, NULL, JavaScriptLanguage::get_singleton()->global_template.Get(isolate));

	v8::Context::Scope scope(ctx);

	v8::Local<v8::String> source =
		v8::String::NewFromUtf8(
			isolate,
			get_source_code().utf8().get_data(),
			v8::NewStringType::kNormal).ToLocalChecked();

	v8::TryCatch trycatch(isolate);
	v8::MaybeLocal<v8::Script> script = v8::Script::Compile(ctx, source);

	if (script.IsEmpty()) {
		return ERR_COMPILATION_FAILED;
	}

	v8::MaybeLocal<v8::Value> temp_result = script.ToLocalChecked()->Run(ctx);

	if (temp_result.IsEmpty()) {
		v8::Local<v8::Value> exception = trycatch.Exception();
		v8::String::Utf8Value exception_str(exception->ToDetailString());
		print_line(*exception_str);
		return ERR_COMPILATION_FAILED;
	}

	v8::Local<v8::Value> result = temp_result.ToLocalChecked();
	v8::MaybeLocal<v8::Value> exported = ctx->Global()->Get(ctx, v8::String::NewFromUtf8(isolate, "exports"));

	if (exported.IsEmpty() || !exported.ToLocalChecked()->IsFunction()) {
		if (trycatch.HasCaught()) {
			v8::Local<v8::Value> exception = trycatch.Exception();
			v8::String::Utf8Value exception_str(exception->ToDetailString());
			print_line(*exception_str);
		}
		return ERR_COMPILATION_FAILED;
	}

	constructor = v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function> >(isolate, v8::Local<v8::Function>::Cast(exported.ToLocalChecked()));

	compiled = true;

	return OK;
}

bool JavaScript::has_method(const StringName & p_method) const {
	return false;
}

MethodInfo JavaScript::get_method_info(const StringName & p_method) const {
	return MethodInfo();
}

bool JavaScript::is_tool() const {
	return tool;
}

String JavaScript::get_node_type() const {
	return String();
}

ScriptLanguage * JavaScript::get_language() const {

	return JavaScriptLanguage::get_singleton();
}

bool JavaScript::has_script_signal(const StringName & p_signal) const {
	return false;
}

void JavaScript::get_script_signal_list(List<MethodInfo>* r_signals) const {}

bool JavaScript::get_property_default_value(const StringName & p_property, Variant & r_value) const {
	return false;
}

void JavaScript::get_script_method_list(List<MethodInfo>* p_list) const {}

void JavaScript::get_script_property_list(List<PropertyInfo>* p_list) const {

	p_list->push_back(PropertyInfo(Variant::STRING, "script/source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
}

Error JavaScript::load_source_code(const String & p_path) {

	DVector<uint8_t> sourcef;
	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err) {

		ERR_FAIL_COND_V(err, err);
	}

	int len = f->get_len();
	sourcef.resize(len + 1);
	DVector<uint8_t>::Write w = sourcef.write();
	int r = f->get_buffer(w.ptr(), len);
	f->close();
	memdelete(f);
	ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);
	w[len] = 0;

	String s;
	if (s.parse_utf8((const char*)w.ptr())) {

		ERR_EXPLAIN("Script '" + p_path + "' contains invalid unicode (utf-8), so it was not loaded. Please ensure that scripts are saved in valid utf-8 unicode.");
		ERR_FAIL_V(ERR_INVALID_DATA);
	}

	source = s;
	path = p_path;
	return OK;

}

JavaScript::JavaScript() {
	compiled = false;
	tool = false;
}

JavaScript::~JavaScript() {
	if(!constructor.IsEmpty())
		constructor.SetWeak();
}

RES ResourceFormatLoaderJavaScript::load(const String & p_path, const String & p_original_path, Error * r_error) {

	if (r_error)
		*r_error = ERR_FILE_CANT_OPEN;

	JavaScript *script = memnew(JavaScript);

	Ref<JavaScript> scriptres(script);

	Error err = script->load_source_code(p_path);

	if (err != OK) {

		ERR_FAIL_COND_V(err != OK, RES());
	}

	script->set_path(p_original_path);
	script->set_name(p_path.get_file());

	script->reload();

	if (r_error)
		*r_error = OK;

	return scriptres;
}

void ResourceFormatLoaderJavaScript::get_recognized_extensions(List<String>* p_extensions) const {

	p_extensions->push_back("js");
}

bool ResourceFormatLoaderJavaScript::handles_type(const String & p_type) const {

	return (p_type == "Script" || p_type == "JavaScript");
}

String ResourceFormatLoaderJavaScript::get_resource_type(const String & p_path) const {

	if (p_path.extension().to_lower() == "js") {
		return "JavaScript";
	}
	return "";
}

Error ResourceFormatSaverJavaScript::save(const String & p_path, const RES & p_resource, uint32_t p_flags) {

	Ref<JavaScript> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_source_code();

	Error err;
	FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);


	if (err) {

		ERR_FAIL_COND_V(err, err);
	}

	file->store_string(source);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		memdelete(file);
		return ERR_CANT_CREATE;
	}
	file->close();
	memdelete(file);

	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		JavaScriptLanguage::get_singleton()->reload_tool_script(p_resource, false);
	}

	return OK;
}

void ResourceFormatSaverJavaScript::get_recognized_extensions(const RES & p_resource, List<String>* p_extensions) const {

	if (p_resource->cast_to<JavaScript>() != NULL) {
		p_extensions->push_back("js");
	}
}

bool ResourceFormatSaverJavaScript::recognize(const RES & p_resource) const {

	return (p_resource->cast_to<JavaScript>() != NULL);
}

void JavaScriptInstance::_run() {

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);

	v8::Local<v8::Context> ctx = context.Get(isolate);

	v8::Context::Scope context_scope(ctx);

	v8::Local<v8::Function> cons = script->constructor.Get(isolate);
	v8::MaybeLocal<v8::Object> maybe_inst = cons->NewInstance(ctx);

	if (maybe_inst.IsEmpty()) return;

	v8::Local<v8::Object> inst = maybe_inst.ToLocalChecked();
	v8::Local<v8::External> field = v8::Local<v8::External>::Cast(inst->GetInternalField(0));
	Variant::CallError err;
	void* ptr = field->Value();
	if (ptr) {
		static_cast<Object*>(ptr)->call(CoreStringNames::get_singleton()->_free, NULL, 0, err);
	}
	inst->SetInternalField(0, v8::External::New(isolate, owner));
	inst->SetInternalField(1, v8::Boolean::New(isolate, true));

	instance = v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >(isolate, inst);

	compiled = true;
}

bool JavaScriptInstance::set(const StringName & p_name, const Variant & p_value) {
	return false;
}

bool JavaScriptInstance::get(const StringName & p_name, Variant & r_ret) const {
	return false;
}

void JavaScriptInstance::get_property_list(List<PropertyInfo>* p_properties) const {}

Variant::Type JavaScriptInstance::get_property_type(const StringName & p_name, bool * r_is_valid) const {
	return Variant::Type();
}

void JavaScriptInstance::get_property_state(List<Pair<StringName, Variant>>& state) {}

void JavaScriptInstance::get_method_list(List<MethodInfo>* p_list) const {

	MethodInfo m;
	m.name = "_ready";
	p_list->push_back(m);
}

bool JavaScriptInstance::has_method(const StringName & p_method) const {
	return true;
}

Variant JavaScriptInstance::call(const StringName & p_method, const Variant ** p_args, int p_argcount, Variant::CallError & r_error) {

	if (!compiled) {

		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scole(isolate);

	v8::Local<v8::Context> ctx = context.Get(isolate);
	v8::Local<v8::Object> inst = instance.Get(isolate);

	String m = p_method;

	v8::MaybeLocal<v8::Value> func_val = inst->Get(ctx, v8::String::NewFromUtf8(isolate, m.utf8().get_data()));

	if (func_val.IsEmpty() || !func_val.ToLocalChecked()->IsFunction()) {
		if (owner) {
			MethodBind* method = ObjectTypeDB::get_method(owner->get_type_name(), p_method);
			if (method) {
				return method->call(owner, p_args, p_argcount, r_error);
			} else {
				r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
				return Variant();
			}
		}
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(func_val.ToLocalChecked());
	v8::MaybeLocal<v8::Value> func_result = func->CallAsFunction(ctx, inst, 0, NULL);

	if (func_result.IsEmpty()) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Variant::CallError::CALL_OK;
	return Variant();

}

void JavaScriptInstance::call_multilevel(const StringName & p_method, const Variant ** p_args, int p_argcount) {

	Variant::CallError err;
	call(p_method, p_args, p_argcount, err);
}

void JavaScriptInstance::call_multilevel_reversed(const StringName & p_method, const Variant ** p_args, int p_argcount) {

	Variant::CallError err;
	call(p_method, p_args, p_argcount, err);
}

void JavaScriptInstance::notification(int p_notification) {}

Ref<Script> JavaScriptInstance::get_script() const {

	return script;
}

ScriptInstance::RPCMode JavaScriptInstance::get_rpc_mode(const StringName & p_method) const {
	return RPC_MODE_DISABLED;
}

ScriptInstance::RPCMode JavaScriptInstance::get_rset_mode(const StringName & p_variable) const {
	return RPC_MODE_DISABLED;
}

ScriptLanguage * JavaScriptInstance::get_language() {

	return JavaScriptLanguage::get_singleton();
}

JavaScriptInstance::JavaScriptInstance() {

	isolate = JavaScriptLanguage::get_singleton()->isolate;

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope scope(isolate);
	v8::EscapableHandleScope handle_scope(isolate);

	// Create a context for this instance
	v8::Local<v8::Context> local_context = v8::Context::New(isolate, NULL, JavaScriptLanguage::get_singleton()->global_template.Get(isolate));
	v8::Context::Scope local_context_scope(local_context);
	context = v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>>(isolate, handle_scope.Escape(local_context));

	compiled = false;

}

JavaScriptInstance::~JavaScriptInstance() {
	if(!context.IsEmpty())
		context.SetWeak();
	if(!instance.IsEmpty())
		instance.SetWeak();
}

void JavaScriptLanguage::Bindings::js_constructor(const v8::FunctionCallbackInfo<v8::Value>& p_args) {
	print_line("constructor");
	// Set the object as JS created by default
	p_args.This()->SetInternalField(1, v8::Boolean::New(p_args.GetIsolate(), true));
}

void JavaScriptLanguage::Bindings::js_method(const v8::FunctionCallbackInfo<v8::Value>& p_args) {
	print_line("js_method");
}

void JavaScriptLanguage::Bindings::js_getter(v8::Local<v8::Name> p_name, const v8::PropertyCallbackInfo<v8::Value>& p_args) {

	v8::Isolate *isolate = p_args.GetIsolate();

	if (p_args.This()->InternalFieldCount() != 2) {
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Invalid JavaScript object"));
		return;
	}

	Object* obj = JavaScriptFunctions::unwrap_object(p_args.This());

	v8::String::Utf8Value name(p_name);
	StringName prop(*name);

	v8::Local<v8::Value> js_result;

	// If the object is external to JavaScript, just use variant get/call
	if (v8::Local<v8::Boolean>::Cast(p_args.This()->GetInternalField(1))->IsFalse()) {
		js_result = JavaScriptFunctions::variant_getter(isolate, prop, Variant(obj));

	// Internal object, looks for the property/function internally
	} else {
		js_result = JavaScriptFunctions::object_getter(isolate, prop, obj);
	}

	if (!js_result.IsEmpty()) {
		p_args.GetReturnValue().Set(js_result);
	}

	print_line("js_getter " + String(*name));

	// Let this handler do nothing so it pass to other handlers
}

void JavaScriptLanguage::Bindings::js_setter(v8::Local<v8::Name> p_name, v8::Local<v8::Value> p_value, const v8::PropertyCallbackInfo<v8::Value>& p_args) {
	v8::String::Utf8Value name(p_name);
	print_line("js_setter " + String(*name));
}
