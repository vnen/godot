
#include "js_language.h"
#include "os/file_access.h"
#include "io/file_access_encrypted.h"
#include "globals.h"
#include "core/os/os.h"

#include "v8.h"
#include "libplatform/libplatform.h"

JavaScriptLanguage* JavaScriptLanguage::singleton = NULL;

void JavaScriptLanguage::init() {

	using namespace v8;

	// Initialize V8.
	V8::InitializeICUDefaultLocation(OS::get_singleton()->get_executable_path().utf8().get_data());
	V8::InitializeExternalStartupData(OS::get_singleton()->get_executable_path().utf8().get_data());
	platform = platform::CreateDefaultPlatform();
	V8::InitializePlatform(platform);
	V8::Initialize();

	// Create the isolate
	Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &allocator;

	isolate = Isolate::New(create_params);
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

void JavaScript::_bind_methods() {}

bool JavaScript::can_instance() const {
	return false;
}

Ref<Script> JavaScript::get_base_script() const {
	return Ref<Script>();
}

StringName JavaScript::get_instance_base_type() const {
	return StringName();
}

ScriptInstance * JavaScript::instance_create(Object * p_this) {
	return nullptr;
}

bool JavaScript::instance_has(const Object * p_this) const {
	return false;
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
	return Error();
}

bool JavaScript::has_method(const StringName & p_method) const {
	return false;
}

MethodInfo JavaScript::get_method_info(const StringName & p_method) const {
	return MethodInfo();
}

bool JavaScript::is_tool() const {
	return false;
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
#ifdef TOOLS_ENABLED
	//source_changed_cache = true;
#endif
	//print_line("LSC :"+get_path());
	path = p_path;
	return OK;

}

JavaScript::JavaScript() {}

JavaScript::~JavaScript() {}

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

void JavaScriptInstance::get_method_list(List<MethodInfo>* p_list) const {}

bool JavaScriptInstance::has_method(const StringName & p_method) const {
	return false;
}

Variant JavaScriptInstance::call(const StringName & p_method, const Variant ** p_args, int p_argcount, Variant::CallError & r_error) {
	return Variant();
}

void JavaScriptInstance::call_multilevel(const StringName & p_method, const Variant ** p_args, int p_argcount) {}

void JavaScriptInstance::call_multilevel_reversed(const StringName & p_method, const Variant ** p_args, int p_argcount) {}

void JavaScriptInstance::notification(int p_notification) {}

Ref<Script> JavaScriptInstance::get_script() const {
	return Ref<Script>();
}

ScriptInstance::RPCMode JavaScriptInstance::get_rpc_mode(const StringName & p_method) const {
	return RPCMode();
}

ScriptInstance::RPCMode JavaScriptInstance::get_rset_mode(const StringName & p_variable) const {
	return RPCMode();
}

ScriptLanguage * JavaScriptInstance::get_language() {

	return JavaScriptLanguage::get_singleton();
}

JavaScriptInstance::JavaScriptInstance() {}

JavaScriptInstance::~JavaScriptInstance() {}
