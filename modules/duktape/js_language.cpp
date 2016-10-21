
#include "js_language.h"

JavaScriptLanguage* JavaScriptLanguage::singleton = NULL;

void JavaScriptLanguage::init() {}

String JavaScriptLanguage::get_type() const {
	return String();
}

String JavaScriptLanguage::get_extension() const {
	return String();
}

Error JavaScriptLanguage::execute_file(const String & p_path) {
	return Error();
}

void JavaScriptLanguage::finish() {}

void JavaScriptLanguage::get_reserved_words(List<String>* p_words) const {}

void JavaScriptLanguage::get_comment_delimiters(List<String>* p_delimiters) const {}

void JavaScriptLanguage::get_string_delimiters(List<String>* p_delimiters) const {}

Ref<Script> JavaScriptLanguage::get_template(const String & p_class_name, const String & p_base_class_name) const {
	
	Ref<JavaScript> script;
	script.instance();

	return script;
}

bool JavaScriptLanguage::validate(const String & p_script, int & r_line_error, int & r_col_error, String & r_test_error, const String & p_path, List<String>* r_functions) const {
	return false;
}

Script * JavaScriptLanguage::create_script() const {
	return nullptr;
}

bool JavaScriptLanguage::has_named_classes() const {
	return false;
}

int JavaScriptLanguage::find_function(const String & p_function, const String & p_code) const {
	return 0;
}

String JavaScriptLanguage::make_function(const String & p_class, const String & p_name, const StringArray & p_args) const {
	return String();
}

void JavaScriptLanguage::auto_indent_code(String & p_code, int p_from_line, int p_to_line) const {}

void JavaScriptLanguage::add_global_constant(const StringName & p_variable, const Variant & p_value) {}

String JavaScriptLanguage::debug_get_error() const {
	return String();
}

int JavaScriptLanguage::debug_get_stack_level_count() const {
	return 0;
}

int JavaScriptLanguage::debug_get_stack_level_line(int p_level) const {
	return 0;
}

String JavaScriptLanguage::debug_get_stack_level_function(int p_level) const {
	return String();
}

String JavaScriptLanguage::debug_get_stack_level_source(int p_level) const {
	return String();
}

void JavaScriptLanguage::debug_get_stack_level_locals(int p_level, List<String>* p_locals, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

void JavaScriptLanguage::debug_get_stack_level_members(int p_level, List<String>* p_members, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

void JavaScriptLanguage::debug_get_globals(List<String>* p_locals, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

String JavaScriptLanguage::debug_parse_stack_level_expression(int p_level, const String & p_expression, int p_max_subitems, int p_max_depth) {
	return String();
}

void JavaScriptLanguage::reload_all_scripts() {}

void JavaScriptLanguage::reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload) {}

void JavaScriptLanguage::get_recognized_extensions(List<String>* p_extensions) const {

	p_extensions->push_back("js");
}

void JavaScriptLanguage::get_public_functions(List<MethodInfo>* p_functions) const {}

void JavaScriptLanguage::get_public_constants(List<Pair<String, Variant>>* p_constants) const {}

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
	return false;
}

String JavaScript::get_source_code() const {
	return String();
}

void JavaScript::set_source_code(const String & p_code) {}

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
	return nullptr;
}

bool JavaScript::has_script_signal(const StringName & p_signal) const {
	return false;
}

void JavaScript::get_script_signal_list(List<MethodInfo>* r_signals) const {}

bool JavaScript::get_property_default_value(const StringName & p_property, Variant & r_value) const {
	return false;
}

void JavaScript::get_script_method_list(List<MethodInfo>* p_list) const {}

void JavaScript::get_script_property_list(List<PropertyInfo>* p_list) const {}

JavaScript::JavaScript() {}

JavaScript::~JavaScript() {}

RES ResourceFormatLoaderJavaScript::load(const String & p_path, const String & p_original_path, Error * r_error) {

	*r_error = ERR_UNAVAILABLE;
	return RES();
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
	
	return ERR_UNAVAILABLE;
}

void ResourceFormatSaverJavaScript::get_recognized_extensions(const RES & p_resource, List<String>* p_extensions) const {
	
	if (p_resource->cast_to<JavaScript>()) {
		p_extensions->push_back("js");
	}
}

bool ResourceFormatSaverJavaScript::recognize(const RES & p_resource) const {

	return (p_resource->cast_to<JavaScript>() != NULL);
}
