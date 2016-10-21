
#include "js_language.h"

JavascriptLanguage* JavascriptLanguage::singleton = NULL;

void JavascriptLanguage::init() {}

String JavascriptLanguage::get_type() const {
	return String();
}

String JavascriptLanguage::get_extension() const {
	return String();
}

Error JavascriptLanguage::execute_file(const String & p_path) {
	return Error();
}

void JavascriptLanguage::finish() {}

void JavascriptLanguage::get_reserved_words(List<String>* p_words) const {}

void JavascriptLanguage::get_comment_delimiters(List<String>* p_delimiters) const {}

void JavascriptLanguage::get_string_delimiters(List<String>* p_delimiters) const {}

Ref<Script> JavascriptLanguage::get_template(const String & p_class_name, const String & p_base_class_name) const {
	return Ref<Script>();
}

bool JavascriptLanguage::validate(const String & p_script, int & r_line_error, int & r_col_error, String & r_test_error, const String & p_path, List<String>* r_functions) const {
	return false;
}

Script * JavascriptLanguage::create_script() const {
	return nullptr;
}

bool JavascriptLanguage::has_named_classes() const {
	return false;
}

int JavascriptLanguage::find_function(const String & p_function, const String & p_code) const {
	return 0;
}

String JavascriptLanguage::make_function(const String & p_class, const String & p_name, const StringArray & p_args) const {
	return String();
}

void JavascriptLanguage::auto_indent_code(String & p_code, int p_from_line, int p_to_line) const {}

void JavascriptLanguage::add_global_constant(const StringName & p_variable, const Variant & p_value) {}

String JavascriptLanguage::debug_get_error() const {
	return String();
}

int JavascriptLanguage::debug_get_stack_level_count() const {
	return 0;
}

int JavascriptLanguage::debug_get_stack_level_line(int p_level) const {
	return 0;
}

String JavascriptLanguage::debug_get_stack_level_function(int p_level) const {
	return String();
}

String JavascriptLanguage::debug_get_stack_level_source(int p_level) const {
	return String();
}

void JavascriptLanguage::debug_get_stack_level_locals(int p_level, List<String>* p_locals, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

void JavascriptLanguage::debug_get_stack_level_members(int p_level, List<String>* p_members, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

void JavascriptLanguage::debug_get_globals(List<String>* p_locals, List<Variant>* p_values, int p_max_subitems, int p_max_depth) {}

String JavascriptLanguage::debug_parse_stack_level_expression(int p_level, const String & p_expression, int p_max_subitems, int p_max_depth) {
	return String();
}

void JavascriptLanguage::reload_all_scripts() {}

void JavascriptLanguage::reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload) {}

void JavascriptLanguage::get_recognized_extensions(List<String>* p_extensions) const {}

void JavascriptLanguage::get_public_functions(List<MethodInfo>* p_functions) const {}

void JavascriptLanguage::get_public_constants(List<Pair<String, Variant>>* p_constants) const {}

void JavascriptLanguage::profiling_start() {}

void JavascriptLanguage::profiling_stop() {}

int JavascriptLanguage::profiling_get_accumulated_data(ProfilingInfo * p_info_arr, int p_info_max) {
	return 0;
}

int JavascriptLanguage::profiling_get_frame_data(ProfilingInfo * p_info_arr, int p_info_max) {
	return 0;
}

void JavascriptLanguage::frame() {}

JavascriptLanguage::JavascriptLanguage() {

	ERR_FAIL_COND(singleton);
	singleton = this;
}

JavascriptLanguage::~JavascriptLanguage() {

	singleton = NULL;
}
