
#include "js_language.h"
#include "os/file_access.h"
#include "io/file_access_encrypted.h"

JavaScriptLanguage* JavaScriptLanguage::singleton = NULL;

void JavaScriptLanguage::init() {}

String JavaScriptLanguage::get_type() const {

	return "JavaScript";
}

String JavaScriptLanguage::get_extension() const {
	
	return "js";
}

Error JavaScriptLanguage::execute_file(const String & p_path) {

	return OK;
}

void JavaScriptLanguage::finish() {}

void JavaScriptLanguage::get_reserved_words(List<String>* p_words) const {

	static const char* reserved_words[] = {
		"abstract",
		"else",
		"instanceof",
		"super",
		"boolean",
		"enum",
		"int",
		"switch",
		"break",
		"export",
		"interface",
		"synchronized",
		"byte",
		"extends",
		"let",
		"this",
		"case",
		"false",
		"long",
		"throw",
		"catch",
		"final",
		"native",
		"throws",
		"char",
		"finally",
		"new",
		"transient",
		"class",
		"float",
		"null",
		"true",
		"const",
		"for",
		"package",
		"try",
		"continue",
		"function",
		"private",
		"typeof",
		"debugger",
		"goto",
		"protected",
		"var",
		"default",
		"if",
		"public",
		"void",
		"delete",
		"implements",
		"return",
		"volatile",
		"do",
		"import",
		"short",
		"while",
		"double",
		"in",
		"static",
		"with",

		// not reserved but implementation-specific
		"Array",
		"Date",
		"eval",
		"function",
		"hasOwnProperty",
		"Infinity",
		"isFinite",
		"isNaN",
		"isPrototypeOf",
		"length",
		"Math",
		"NaN",
		"name",
		"Number",
		"Object",
		"prototype",
		"String",
		"toString",
		"undefined",
		"valueOf",
		0
	};

	const char **w = reserved_words;


	while (*w) {

		p_words->push_back(*w);
		w++;
	}
}

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

Variant JavaScriptInstance::call(const StringName & p_method, VARIANT_ARG_LIST) {
	return Variant();
}

Variant JavaScriptInstance::call(const StringName & p_method, const Variant ** p_args, int p_argcount, Variant::CallError & r_error) {
	return Variant();
}

void JavaScriptInstance::call_multilevel(const StringName & p_method, VARIANT_ARG_LIST) {}

void JavaScriptInstance::call_multilevel(const StringName & p_method, const Variant ** p_args, int p_argcount) {}

void JavaScriptInstance::call_multilevel_reversed(const StringName & p_method, const Variant ** p_args, int p_argcount) {}

void JavaScriptInstance::notification(int p_notification) {}

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
