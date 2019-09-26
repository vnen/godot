/*************************************************************************/
/*  gdscript_cache.h                                                     */
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

#include "core/ordered_hash_map.h"
#include "core/reference.h"

class GDScript;
class GDScriptParser;

class GDScriptCache {

	Set<String> parsing_scripts;
	OrderedHashMap<String, GDScriptParser *> interface_parsed_scripts; // Key is path
	OrderedHashMap<String, GDScriptParser *> parsed_scripts; // Key is path
	OrderedHashMap<String, Ref<GDScript> > compiled_scripts; // Key is path
	OrderedHashMap<String, Ref<GDScript> > created_scripts; // Key is path

	static GDScriptCache *singleton;

public:
	static Error parse_script(const String &p_path, GDScriptParser **r_parsed);
	static Error parse_script_interface(const String &p_path, GDScriptParser **r_parsed);

	static String get_source_code(const String &p_path);

	static Ref<GDScript> get_shallow_script(const String &p_path);
	static Ref<GDScript> get_full_script(const String &p_path);

	static bool is_gdscript(const String &p_path);

	GDScriptCache();
};
