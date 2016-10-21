#include "register_types.h"
#include "object_type_db.h"
#include "script_language.h"
#include "js_language.h"

JavascriptLanguage *js_duktape_language = NULL;

void register_duktape_types() {

	js_duktape_language = memnew(JavascriptLanguage);
	ScriptServer::register_language(js_duktape_language);
}

void unregister_duktape_types() {

	ScriptServer::unregister_language(js_duktape_language);

	if (js_duktape_language)
		memdelete(js_duktape_language);
}
