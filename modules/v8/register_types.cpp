/*************************************************************************/
/*  register_types.cpp                                                   */
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

#include "register_types.h"
#include "object_type_db.h"
#include "io/resource_loader.h"
#include "io/resource_saver.h"
#include "script_language.h"
#include "js_language.h"

JavaScriptLanguage *js_duktape_language = NULL;
ResourceFormatLoaderJavaScript *js_resource_loader = NULL;
ResourceFormatSaverJavaScript *js_resource_saver = NULL;

void register_v8_types() {

	ObjectTypeDB::register_type<JavaScript>();

	js_duktape_language = memnew(JavaScriptLanguage);
	ScriptServer::register_language(js_duktape_language);

	js_resource_loader = memnew(ResourceFormatLoaderJavaScript);
	ResourceLoader::add_resource_format_loader(js_resource_loader);
	js_resource_saver = memnew(ResourceFormatSaverJavaScript);
	ResourceSaver::add_resource_format_saver(js_resource_saver);
}

void unregister_v8_types() {

	ScriptServer::unregister_language(js_duktape_language);

	if (js_duktape_language)
		memdelete(js_duktape_language);
	if (js_resource_loader)
		memdelete(js_resource_loader);
	if (js_resource_saver)
		memdelete(js_resource_saver);
}
