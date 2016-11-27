/*************************************************************************/
/*  js_language.h                                                        */
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

#ifndef JS_LANGUAGE_H
#define JS_LANGUAGE_H

#include "script_language.h"
#include "vector.h"
#include "list.h"
#include "ustring.h"
#include "io/resource_loader.h"
#include "io/resource_saver.h"

#include "v8.h"
#include "libplatform/libplatform.h"

class GodotV8ArrayAllocator : public v8::ArrayBuffer::Allocator {

	virtual void* Allocate(size_t length) {
		void* data = AllocateUninitialized(length);
		return data == NULL ? data : memset(data, 0, length);
	}
	virtual void* AllocateUninitialized(size_t length) { return memalloc(length); }
	virtual void Free(void* data, size_t) { memfree(data); }
};

class JavaScript : public Script {

	OBJ_TYPE(JavaScript, Script);

	friend class JavaScriptInstance;
	Set<Object*> instances;
	v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function> > constructor;
	v8::ScriptOrigin *origin;
	bool compiled;

	bool tool;

	Set<PlaceHolderScriptInstance*> placeholders;
	void _update_exports();

protected:

	static void _bind_methods();

public:

	String source;
	String path;

	virtual bool can_instance() const;

	virtual Variant call(const StringName& p_method, const Variant** p_args, int p_argcount, Variant::CallError &r_error);

	virtual Ref<Script> get_base_script() const; //for script inheritance

	virtual StringName get_instance_base_type() const; // this may not work in all scripts, will return empty if so
	virtual ScriptInstance* instance_create(Object *p_this);
	virtual bool instance_has(const Object *p_this) const;


	virtual bool has_source_code() const;
	virtual String get_source_code() const;
	virtual void set_source_code(const String& p_code);
	virtual Error reload(bool p_keep_state = false);

	virtual bool has_method(const StringName& p_method) const;
	virtual MethodInfo get_method_info(const StringName& p_method) const;

	virtual bool is_tool() const;

	virtual String get_node_type() const;

	virtual ScriptLanguage *get_language() const;

	virtual bool has_script_signal(const StringName& p_signal) const;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const;

	virtual bool get_property_default_value(const StringName& p_property, Variant& r_value) const;

	virtual void update_exports() {} //editor tool
	virtual void get_script_method_list(List<MethodInfo> *p_list) const;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const;

	virtual int get_member_line(const StringName& p_member) const { return 0; }

	Error load_source_code(const String& p_path);

	JavaScript();
	~JavaScript();
};

class JavaScriptInstance : public ScriptInstance {

	friend class JavaScript;

	Object *owner;
	Ref<JavaScript> script;

	v8::Isolate *isolate;

	v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context> > context;
	v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> > instance;

	bool compiled;

	void _run();

public:


	virtual bool set(const StringName& p_name, const Variant& p_value);
	virtual bool get(const StringName& p_name, Variant &r_ret) const;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const;
	virtual Variant::Type get_property_type(const StringName& p_name, bool *r_is_valid = NULL) const;

	virtual void get_property_state(List<Pair<StringName, Variant> > &state);

	virtual void get_method_list(List<MethodInfo> *p_list) const;
	virtual bool has_method(const StringName& p_method) const;
	virtual Variant call(const StringName& p_method, const Variant** p_args, int p_argcount, Variant::CallError &r_error);
	virtual void call_multilevel(const StringName& p_method, const Variant** p_args, int p_argcount);
	virtual void call_multilevel_reversed(const StringName& p_method, const Variant** p_args, int p_argcount);
	virtual void notification(int p_notification);

	//this is used by script languages that keep a reference counter of their own
	//you can make make Ref<> not die when it reaches zero, so deleting the reference
	//depends entirely from the script

	virtual void refcount_incremented() {}
	virtual bool refcount_decremented() { return true; } //return true if it can die

	virtual Ref<Script> get_script() const;

	virtual bool is_placeholder() const { return false; }

	virtual RPCMode get_rpc_mode(const StringName& p_method) const;
	virtual RPCMode get_rset_mode(const StringName& p_variable) const;

	virtual ScriptLanguage *get_language();
	JavaScriptInstance();
	virtual ~JavaScriptInstance();
};

class JavaScriptLanguage : public ScriptLanguage {

	static JavaScriptLanguage *singleton;

	friend class JavaScriptInstance;
	friend class JavaScript;
	v8::Isolate::CreateParams create_params;
	v8::Platform *platform;
	v8::Isolate *isolate;
	v8::Eternal<v8::ObjectTemplate> global_template;
	v8::Eternal<v8::ObjectTemplate> shallow_template;
	v8::Eternal<v8::UnboundScript> global_script;

	GodotV8ArrayAllocator allocator;

	friend class JavaScriptFunctions;

	void _add_class(const StringName &p_type, const v8::Local<v8::FunctionTemplate> &p_parent = v8::Local<v8::FunctionTemplate>());
	void _add_builtin_type(Variant::Type p_type);
	
public:

	class Bindings {

	public:
		static void js_constructor(const v8::FunctionCallbackInfo<v8::Value>& p_args);
		static void js_method(const v8::FunctionCallbackInfo<v8::Value>& p_args);
		static void js_singleton_method(const v8::FunctionCallbackInfo<v8::Value>& p_args);
		static void js_builtin_constructor(const v8::FunctionCallbackInfo<v8::Value>& p_args);
		static void js_builtin_method(const v8::FunctionCallbackInfo<v8::Value>& p_args);
		static void js_builtin_getter(v8::Local<v8::Name> p_prop, const v8::PropertyCallbackInfo<v8::Value>& p_args);
		static void js_builtin_setter(v8::Local<v8::Name> p_prop, v8::Local<v8::Value> p_value, const v8::PropertyCallbackInfo<void>& p_args);
		static void js_builtin_op(const v8::FunctionCallbackInfo<v8::Value>& p_args);
	};

	_FORCE_INLINE_ static JavaScriptLanguage* get_singleton() { return singleton; }

	virtual String get_name() const { return "JavaScript"; }

	/* LANGUAGE FUNCTIONS */
	virtual void init();
	virtual String get_type() const;
	virtual String get_extension() const;
	virtual Error execute_file(const String& p_path);
	virtual void finish();

	/* EDITOR FUNCTIONS */
	virtual void get_reserved_words(List<String> *p_words) const;
	virtual void get_comment_delimiters(List<String> *p_delimiters) const;
	virtual void get_string_delimiters(List<String> *p_delimiters) const;
	virtual Ref<Script> get_template(const String& p_class_name, const String& p_base_class_name) const;
	virtual bool validate(const String& p_script, int &r_line_error, int &r_col_error, String& r_test_error, const String& p_path = "", List<String> *r_functions = NULL) const;
	virtual Script *create_script() const;
	virtual bool has_named_classes() const;
	virtual int find_function(const String& p_function, const String& p_code) const;
	virtual String make_function(const String& p_class, const String& p_name, const StringArray& p_args) const;

	virtual Error complete_code(const String& p_code, const String& p_base_path, Object*p_owner, List<String>* r_options, String& r_call_hint);

#ifdef TOOLS_ENABLED
	virtual Error lookup_code(const String& p_code, const String& p_symbol, const String& p_base_path, Object*p_owner, LookupResult& r_result);
#endif // TOOLS_ENABLED

	virtual void auto_indent_code(String& p_code, int p_from_line, int p_to_line) const;
	virtual void add_global_constant(const StringName& p_variable, const Variant& p_value);

	/* MULTITHREAD FUNCTIONS */

	//some VMs need to be notified of thread creation/exiting to allocate a stack
	virtual void thread_enter() {}
	virtual void thread_exit() {}

	/* DEBUGGER FUNCTIONS */

	virtual String debug_get_error() const;
	virtual int debug_get_stack_level_count() const;
	virtual int debug_get_stack_level_line(int p_level) const;
	virtual String debug_get_stack_level_function(int p_level) const;
	virtual String debug_get_stack_level_source(int p_level) const;
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
	virtual void debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1);
	virtual String debug_parse_stack_level_expression(int p_level, const String& p_expression, int p_max_subitems = -1, int p_max_depth = -1);

	virtual Vector<StackInfo> debug_get_current_stack_info() { return Vector<StackInfo>(); }

	virtual void reload_all_scripts();
	virtual void reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload);

	/* LOADER FUNCTIONS */

	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual void get_public_functions(List<MethodInfo> *p_functions) const;
	virtual void get_public_constants(List<Pair<String, Variant> > *p_constants) const;

	virtual void profiling_start();
	virtual void profiling_stop();

	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max);
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max);

	virtual void frame();

	JavaScriptLanguage();
	~JavaScriptLanguage();

};

class ResourceFormatLoaderJavaScript : public ResourceFormatLoader {
public:

	virtual RES load(const String &p_path, const String& p_original_path = "", Error *r_error = NULL);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String& p_type) const;
	virtual String get_resource_type(const String &p_path) const;

};

class ResourceFormatSaverJavaScript : public ResourceFormatSaver {
public:

	virtual Error save(const String &p_path, const RES& p_resource, uint32_t p_flags);
	virtual void get_recognized_extensions(const RES& p_resource, List<String> *p_extensions) const;
	virtual bool recognize(const RES& p_resource) const;

};

#endif
