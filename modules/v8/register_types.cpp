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
