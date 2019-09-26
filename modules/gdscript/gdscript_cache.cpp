/*************************************************************************/
/*  gdscript_cache.cpp                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "gdscript_cache.h"

#include "core/os/file_access.h"
#include "core/pool_vector.h"
#include "gdscript.h"
#include "gdscript_parser.h"

GDScriptCache *GDScriptCache::singleton = NULL;

Error GDScriptCache::parse_script(const String &p_path, GDScriptParser **r_parsed) {
	ERR_FAIL_COND_V_MSG(singleton->parsing_scripts.has(p_path), ERR_ALREADY_IN_USE, "Script \"" + p_path + "\" is already being parsed");

	if (singleton->parsed_scripts.has(p_path)) {
		*r_parsed = singleton->parsed_scripts[p_path];
		return OK;
	}

	String source = get_source_code(p_path);
	ERR_FAIL_COND_V_MSG(source.empty(), ERR_INVALID_DATA, "Couldn't load script source code from \"" + p_path + "\".");

	GDScriptParser *parser = memnew(GDScriptParser);
	singleton->parsing_scripts.insert(p_path);
	Error err = parser->parse(source, p_path.get_base_dir(), false, p_path);
	singleton->parsing_scripts.erase(p_path);
	if (err) {
		memdelete(parser);
		return err;
	}

	singleton->parsed_scripts.insert(p_path, parser);
	*r_parsed = singleton->parsed_scripts[p_path];
	(void)r_parsed; // Suppress unused warning
	return OK;
}

Error GDScriptCache::parse_script_interface(const String &p_path, GDScriptParser **r_parsed) {
	// TODO: Use a reference counting system to dealloc the created parsers when not needed anymore
	ERR_FAIL_COND_V_MSG(singleton->parsing_scripts.has(p_path), ERR_ALREADY_IN_USE, "Script \"" + p_path + "\" is already being parsed");

	if (singleton->parsed_scripts.has(p_path)) {
		*r_parsed = singleton->parsed_scripts[p_path];
		return OK;
	}
	if (singleton->interface_parsed_scripts.has(p_path)) {
		*r_parsed = singleton->interface_parsed_scripts[p_path];
		return OK;
	}

	String source = get_source_code(p_path);
	ERR_FAIL_COND_V_MSG(source.empty(), ERR_INVALID_DATA, "Couldn't load script source code from \"" + p_path + "\".");

	GDScriptParser *parser = memnew(GDScriptParser);
	singleton->parsing_scripts.insert(p_path);
	Error err = parser->parse_interface(source, p_path.get_base_dir(), p_path);
	singleton->parsing_scripts.erase(p_path);
	if (err) {
		memdelete(parser);
		return err;
	}

	singleton->interface_parsed_scripts.insert(p_path, parser);
	*r_parsed = singleton->interface_parsed_scripts[p_path];
	(void)r_parsed; // Suppress unused warning
	return OK;
}

String GDScriptCache::get_source_code(const String &p_path) {
	PoolVector<uint8_t> sourcef;
	Error err;
	FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err) {
		ERR_FAIL_COND_V(err, "");
	}

	int len = f->get_len();
	sourcef.resize(len + 1);
	PoolVector<uint8_t>::Write w = sourcef.write();
	int r = f->get_buffer(w.ptr(), len);
	f->close();
	ERR_FAIL_COND_V(r != len, "");
	w[len] = 0;

	String source;
	if (source.parse_utf8((const char *)w.ptr())) {

		ERR_FAIL_V_MSG("", "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded. Please ensure that scripts are saved in valid UTF-8 unicode.");
	}
	return source;
}

Ref<GDScript> GDScriptCache::get_shallow_script(const String &p_path) {
	if (singleton->compiled_scripts.has(p_path)) {
		return singleton->compiled_scripts[p_path];
	}
	if (singleton->created_scripts.has(p_path)) {
		return singleton->created_scripts[p_path];
	}
	Ref<GDScript> new_script;
	new_script.instance();
	new_script->set_path(p_path);
	new_script->set_script_path(p_path);
	singleton->created_scripts.insert(p_path, new_script);
	return new_script;
}

Ref<GDScript> GDScriptCache::get_full_script(const String &p_path) {
	if (singleton->compiled_scripts.has(p_path)) {
		return singleton->compiled_scripts[p_path];
	}
	Ref<GDScript> full_script = get_shallow_script(p_path);
	full_script->set_source_code(get_source_code(p_path));
	if (!full_script->reload()) {
		return Ref<GDScript>();
	}
	singleton->compiled_scripts.insert(p_path, full_script);
	return full_script;
}

bool GDScriptCache::is_gdscript(const String &p_path) {
	List<String> extensions;
	GDScriptLanguage::get_singleton()->get_recognized_extensions(&extensions);
	return extensions.find(p_path.get_extension().to_lower()) != NULL;
}

GDScriptCache::GDScriptCache() {
	singleton = this;
}
