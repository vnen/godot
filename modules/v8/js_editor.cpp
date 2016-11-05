/*************************************************************************/
/*  js_editor.cpp                                                        */
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

#include "v8.h"

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

		// Godot specific
		"print",
		0
	};

	const char **w = reserved_words;


	while (*w) {

		p_words->push_back(*w);
		w++;
	}
}

void JavaScriptLanguage::get_comment_delimiters(List<String>* p_delimiters) const {

	p_delimiters->push_back("//");
	p_delimiters->push_back("/* */");
}

void JavaScriptLanguage::get_string_delimiters(List<String>* p_delimiters) const {

	p_delimiters->push_back("' '");
	p_delimiters->push_back("\" \"");
}

Ref<Script> JavaScriptLanguage::get_template(const String & p_class_name, const String & p_base_class_name) const {

	Ref<JavaScript> script;
	script.instance();

	String _template = String() +
		"class %CLASS% extends %BASE% {\n\n"
		"\t_ready() {\n" +
		"\t\t// Called every time the node is added to the scene.\n" +
		"\t\t// Initialization here\n" +
		"\t}\n" +
		"}\n\n" +
		"exports = %CLASS%;\n";

	_template = _template.replace("%CLASS%", p_class_name);
	_template = _template.replace("%BASE%", p_base_class_name);

	script->set_source_code(_template);

	return script;
}

bool JavaScriptLanguage::validate(const String & p_script, int & r_line_error, int & r_col_error, String & r_test_error, const String & p_path, List<String>* r_functions) const {

	using namespace v8;

	// Create a stack-allocated handle scope.
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);

	Local<Context> context = Context::New(isolate);
	Context::Scope context_scope(context);

	Local<v8::String> source = v8::String::NewFromUtf8(isolate, p_script.utf8().get_data(),
		NewStringType::kNormal
		).ToLocalChecked();

	TryCatch trycatch(isolate);
	MaybeLocal<v8::Script> script = v8::Script::Compile(context, source);

	if (script.IsEmpty()) {
		Local<Message> ex = trycatch.Message();

		v8::String::Utf8Value extext(trycatch.Exception());
		r_test_error = ::String(*extext);

		if (ex.IsEmpty()) {
			r_line_error = 0;
			r_col_error = 0;
		} else {
			r_line_error = ex->GetLineNumber();
			r_col_error = ex->GetStartColumn();
		}

		return false;
	}

	MaybeLocal<Value> result = script.ToLocalChecked()->Run(context);


	if (result.IsEmpty()) {
		Local<Message> ex = trycatch.Message();

		v8::String::Utf8Value extext(trycatch.Exception());
		r_test_error = ::String(*extext);

		if (ex.IsEmpty()) {
			r_line_error = 0;
			r_col_error = 0;
		} else {
			r_line_error = ex->GetLineNumber();
			r_col_error = ex->GetStartColumn();
		}

		return false;
	} else {

		Local<v8::Object> global = context->Global();
		Local<v8::Array> props = global->GetOwnPropertyNames();

		for (int i = 0; i < props->Length(); i++) {

			if (global->Get(props->Get(i))->IsFunction()) {

				Local<Function> func = Local<Function>::Cast(global->Get(props->Get(i)));
				int lineno = 1 + func->GetScriptLineNumber();
				v8::String::Utf8Value func_name(func->GetDebugName());

				r_functions->push_back(::String(*func_name) + ":" + itos(lineno));
			}

		}
	}

	return true;
}

Script * JavaScriptLanguage::create_script() const {

	return memnew(JavaScript);
}

bool JavaScriptLanguage::has_named_classes() const {

	return true;
}

int JavaScriptLanguage::find_function(const String & p_function, const String & p_code) const {

	return -1;
}

void JavaScriptLanguage::auto_indent_code(String & p_code, int p_from_line, int p_to_line) const {}


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


void JavaScriptLanguage::get_recognized_extensions(List<String>* p_extensions) const {

	p_extensions->push_back("js");
}

void JavaScriptLanguage::get_public_functions(List<MethodInfo>* p_functions) const {}

void JavaScriptLanguage::get_public_constants(List<Pair<String, Variant>>* p_constants) const {}

String JavaScriptLanguage::make_function(const String & p_class, const String & p_name, const StringArray & p_args) const {

	String s = "function " + p_name + "(";
	if (p_args.size()) {
		s += " ";
		for (int i = 0;i<p_args.size();i++) {
			if (i>0)
				s += ", ";
			s += p_args[i].get_slice(":", 0);
		}
		s += " ";
	}
	s += ") {\n\t// replace with function body\n}\n";

	return s;
}

Error JavaScriptLanguage::complete_code(const String& p_code, const String& p_base_path, Object*p_owner, List<String>* r_options, String& r_call_hint) {

	return ERR_UNCONFIGURED;
}

#ifdef TOOLS_ENABLED

Error JavaScriptLanguage::lookup_code(const String& p_code, const String& p_symbol, const String& p_base_path, Object*p_owner, LookupResult& r_result) {

	return ERR_UNCONFIGURED;
}

#endif // TOOLS_ENABLED

