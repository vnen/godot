/*************************************************************************/
/*  gdscript_analyzer.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "gdscript_analyzer.h"

#include "core/io/resource_loader.h"
#include "core/script_language.h"

Error GDScriptAnalyzer::resolve_inheritance(GDScriptNewParser::ClassNode *p_class, bool p_recursive) {
	GDScriptNewParser::DataType result;

	if (p_class->base_type.is_set()) {
		// Already resolved
		return OK;
	}

	if (!p_class->extends_used) {
		result.type_source = GDScriptNewParser::DataType::ANNOTATED_INFERRED;
		result.kind = GDScriptNewParser::DataType::NATIVE;
		result.native_type = "Reference";
	} else {
		result.type_source = GDScriptNewParser::DataType::ANNOTATED_EXPLICIT;

		GDScriptNewParser::DataType base;

		if (!p_class->extends_path.empty()) {
			base.type_source = GDScriptNewParser::DataType::ANNOTATED_EXPLICIT;
			base.kind = GDScriptNewParser::DataType::GDSCRIPT;
			// TODO: Get this from cache singleton.
			base.gdscript_type = nullptr;
		} else {
			const StringName &name = p_class->extends[0];
			base.type_source = GDScriptNewParser::DataType::ANNOTATED_EXPLICIT;

			if (ScriptServer::is_global_class(name)) {
				base.kind = GDScriptNewParser::DataType::GDSCRIPT;
				// TODO: Get this from cache singleton.
				base.gdscript_type = nullptr;
				// TODO: Try singletons (create main unified source for those).
			} else if (p_class->members_indices.has(name)) {
				GDScriptNewParser::ClassNode::Member member = p_class->get_member(name);

				if (member.type == member.CLASS) {
					base.kind = GDScriptNewParser::DataType::GDSCRIPT;
					base.gdscript_type = member.m_class;
				} else if (member.type == member.CONSTANT) {
					// FIXME: This could also be a native type or preloaded GDScript.
					base.kind = GDScriptNewParser::DataType::GDSCRIPT;
					base.gdscript_type = nullptr;
				}
			} else {
				if (ClassDB::class_exists(name)) {
					base.kind = GDScriptNewParser::DataType::NATIVE;
					base.native_type = name;
				}
			}
		}

		// TODO: Extends with attributes (A.B.C).
		result = base;
	}

	if (!result.is_set()) {
		// TODO: More specific error messages.
		parser->push_error(vformat(R"(Could not resolve inheritance for class "%s".)", p_class->identifier == nullptr ? "<main>" : p_class->identifier->name));
		return ERR_PARSE_ERROR;
	}

	p_class->set_datatype(result);

	if (p_recursive) {
		for (int i = 0; i < p_class->members.size(); i++) {
			if (p_class->members[i].type == GDScriptNewParser::ClassNode::Member::CLASS) {
				resolve_inheritance(p_class->members[i].m_class, true);
			}
		}
	}

	return OK;
}

Error GDScriptAnalyzer::resolve_inheritance() {
	return resolve_inheritance(parser->head);
}

Error GDScriptAnalyzer::analyze() {
	parser->errors.clear();
	return resolve_inheritance(parser->head);
}

GDScriptAnalyzer::GDScriptAnalyzer(GDScriptNewParser *p_parser) {
	parser = p_parser;
}
