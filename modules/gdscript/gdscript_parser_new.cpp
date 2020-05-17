/*************************************************************************/
/*  gdscript_parser.cpp                                                  */
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

#include "gdscript_parser_new.h"

#ifdef DEBUG_ENABLED
#include "core/os/os.h"
#include "core/string_builder.h"
#endif // DEBUG_ENABLED

GDScriptNewParser::GDScriptNewParser() {
	// Register valid annotations.
	// TODO: Should this be static?
	// TODO: Validate applicable types (e.g. a VARIABLE annotation that only applies to string variables).
	register_annotation(MethodInfo("@tool"), AnnotationInfo::SCRIPT, &GDScriptNewParser::tool_annotation);
	register_annotation(MethodInfo("@icon", { Variant::STRING, "icon_path" }), AnnotationInfo::SCRIPT, &GDScriptNewParser::icon_annotation);
	// Export annotations.
	register_annotation(MethodInfo("@export"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_TYPE_STRING>);
	register_annotation(MethodInfo("@export_enum", { Variant::STRING, "names" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_ENUM>, 0, true);
	register_annotation(MethodInfo("@export_file", { Variant::STRING, "filter" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_FILE>, 1, true);
	register_annotation(MethodInfo("@export_dir"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_DIR>);
	register_annotation(MethodInfo("@export_global_file", { Variant::STRING, "filter" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_GLOBAL_FILE>, 1, true);
	register_annotation(MethodInfo("@export_global_dir"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_GLOBAL_DIR>);
	register_annotation(MethodInfo("@export_multiline"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_MULTILINE_TEXT>);
	register_annotation(MethodInfo("@export_placeholder"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_PLACEHOLDER_TEXT>);
	register_annotation(MethodInfo("@export_range", { Variant::FLOAT, "min" }, { Variant::FLOAT, "max" }, { Variant::FLOAT, "step" }, { Variant::STRING, "slider1" }, { Variant::STRING, "slider2" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_RANGE>, 3);
	register_annotation(MethodInfo("@export_exp_range", { Variant::FLOAT, "min" }, { Variant::FLOAT, "max" }, { Variant::FLOAT, "step" }, { Variant::STRING, "slider1" }, { Variant::STRING, "slider2" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_EXP_RANGE>, 3);
	register_annotation(MethodInfo("@export_exp_easing", { Variant::STRING, "hint1" }, { Variant::STRING, "hint2" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_EXP_EASING>, 2);
	register_annotation(MethodInfo("@export_color_no_alpha"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_COLOR_NO_ALPHA>);
	register_annotation(MethodInfo("@export_node_path", { Variant::STRING, "type" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_NODE_PATH_VALID_TYPES>, 1, true);
	register_annotation(MethodInfo("@export_flags", { Variant::STRING, "names" }), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_FLAGS>, 0, true);
	register_annotation(MethodInfo("@export_flags_2d_render"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_LAYERS_2D_RENDER>);
	register_annotation(MethodInfo("@export_flags_2d_physics"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_LAYERS_2D_PHYSICS>);
	register_annotation(MethodInfo("@export_flags_3d_render"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_LAYERS_3D_RENDER>);
	register_annotation(MethodInfo("@export_flags_3d_physics"), AnnotationInfo::VARIABLE, &GDScriptNewParser::export_annotations<PROPERTY_HINT_LAYERS_3D_PHYSICS>);
	// Networking.
	register_annotation(MethodInfo("@remote"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_REMOTE>);
	register_annotation(MethodInfo("@master"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_MASTER>);
	register_annotation(MethodInfo("@puppet"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_PUPPET>);
	register_annotation(MethodInfo("@remotesync"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_REMOTESYNC>);
	register_annotation(MethodInfo("@mastersync"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_MASTERSYNC>);
	register_annotation(MethodInfo("@puppetsync"), AnnotationInfo::VARIABLE | AnnotationInfo::FUNCTION, &GDScriptNewParser::network_annotations<MultiplayerAPI::RPC_MODE_PUPPETSYNC>);
	// TODO: Warning annotations.
}

GDScriptNewParser::~GDScriptNewParser() {
	clear();
}

template <class T>
T *GDScriptNewParser::alloc_node() {
	T *node = memnew(T);

	node->next = list;
	list = node;

	return node;
}

void GDScriptNewParser::clear() {
	while (list != nullptr) {
		Node *element = list;
		list = list->next;
		memdelete(element);
	}

	head = nullptr;
	list = nullptr;
	_is_tool = false;
	for_completion = false;
	errors.clear();
}

void GDScriptNewParser::push_error(const String &p_message) {
	// TODO: Improve error reporting by pointing at source code.
	// TODO: Errors might point at more than one place at once (e.g. show previous declaration).
	panic_mode = true;
	// TODO: Improve positional information.
	errors.push_back({ p_message, current.start_line, current.start_column });
}

Error GDScriptNewParser::parse(const String &p_source_code, const String &p_script_path, bool p_for_completion) {
	clear();
	tokenizer.set_source_code(p_source_code);
	current = tokenizer.scan();

	parse_program();

	if (errors.empty()) {
		return OK;
	} else {
		return ERR_PARSE_ERROR;
	}
}

GDScriptNewTokenizer::Token GDScriptNewParser::advance() {
	if (current.type == GDScriptNewTokenizer::Token::TK_EOF) {
		ERR_FAIL_COND_V_MSG(current.type == GDScriptNewTokenizer::Token::TK_EOF, current, "GDScript parser bug: Trying to advance past the end of stream.");
	}
	previous = current;
	current = tokenizer.scan();
	return previous;
}

bool GDScriptNewParser::match(GDScriptNewTokenizer::Token::Type p_token_type) {
	if (!check(p_token_type)) {
		return false;
	}
	advance();
	return true;
}

bool GDScriptNewParser::check(GDScriptNewTokenizer::Token::Type p_token_type) {
	if (p_token_type == GDScriptNewTokenizer::Token::IDENTIFIER) {
		return current.is_identifier();
	}
	return current.type == p_token_type;
}

bool GDScriptNewParser::consume(GDScriptNewTokenizer::Token::Type p_token_type, const String &p_error_message) {
	if (match(p_token_type)) {
		return true;
	}
	push_error(p_error_message);
	return false;
}

bool GDScriptNewParser::is_at_end() {
	return check(GDScriptNewTokenizer::Token::TK_EOF);
}

void GDScriptNewParser::synchronize() {
	panic_mode = false;
	while (!is_at_end()) {
		if (previous.type == GDScriptNewTokenizer::Token::NEWLINE || previous.type == GDScriptNewTokenizer::Token::SEMICOLON) {
			return;
		}

		switch (current.type) {
			case GDScriptNewTokenizer::Token::CLASS:
			case GDScriptNewTokenizer::Token::FUNC:
			case GDScriptNewTokenizer::Token::VAR:
			case GDScriptNewTokenizer::Token::FOR:
			case GDScriptNewTokenizer::Token::WHILE:
			case GDScriptNewTokenizer::Token::RETURN:
			case GDScriptNewTokenizer::Token::CONST:
			case GDScriptNewTokenizer::Token::SIGNAL:
			case GDScriptNewTokenizer::Token::ANNOTATION:
				return;
			default:
				// Do nothing.
				break;
		}

		advance();
	}
}

bool GDScriptNewParser::is_statement_end() {
	return check(GDScriptNewTokenizer::Token::NEWLINE) || check(GDScriptNewTokenizer::Token::SEMICOLON);
}

void GDScriptNewParser::end_statement(const String &p_context) {
	if (is_statement_end()) {
		advance();
	} else {
		push_error(vformat(R"(Expected end of statement after %s, found "%s" instead.)", p_context, current.get_name()));
	}
}

void GDScriptNewParser::parse_program() {
	if (current.type == GDScriptNewTokenizer::Token::TK_EOF) {
		// Empty file.
		push_error("Source file is empty.");
		return;
	}

	head = alloc_node<ClassNode>();
	current_class = head;

	if (match(GDScriptNewTokenizer::Token::ANNOTATION)) {
		// Check for @tool annotation.
		AnnotationNode *annotation = parse_annotation(AnnotationInfo::SCRIPT | AnnotationInfo::CLASS_LEVEL);
		if (annotation->name == "@tool") {
			// TODO: don't allow @tool anywhere else. (Should all script annotations be the first thing?).
			_is_tool = true;
			if (previous.type != GDScriptNewTokenizer::Token::NEWLINE) {
				push_error(R"(Expected newline after "@tool" annotation.)");
			}
			// @tool annotation has no specific target.
			annotation->apply(this, nullptr);
		} else {
			annotation_stack.push_back(annotation);
		}
	}

	for (bool should_break = false; !should_break;) {
		// Order here doesn't matter, but there should be only one of each at most.
		switch (current.type) {
			case GDScriptNewTokenizer::Token::CLASS_NAME:
				if (!annotation_stack.empty()) {
					push_error(R"("class_name" should be used before annotations.)");
				}
				advance();
				if (head->identifier != nullptr) {
					push_error(R"("class_name" can only be used once.)");
				} else {
					parse_class_name();
				}
				break;
			case GDScriptNewTokenizer::Token::EXTENDS:
				if (!annotation_stack.empty()) {
					push_error(R"("extends" should be used before annotations.)");
				}
				advance();
				if (head->extends_used) {
					push_error(R"("extends" can only be used once.)");
				} else {
					parse_extends();
				}
				break;
			default:
				should_break = true;
				break;
		}

		if (panic_mode) {
			synchronize();
		}
	}

	if (match(GDScriptNewTokenizer::Token::ANNOTATION)) {
		// Check for @icon annotation.
		AnnotationNode *annotation = parse_annotation(AnnotationInfo::SCRIPT | AnnotationInfo::CLASS_LEVEL);
		if (annotation != nullptr) {
			if (annotation->name == "@icon") {
				if (previous.type != GDScriptNewTokenizer::Token::NEWLINE) {
					push_error(R"(Expected newline after "@icon" annotation.)");
				}
				annotation->apply(this, head);
			} else {
				annotation_stack.push_back(annotation);
			}
		}
	}

	parse_class_body();

	if (!check(GDScriptNewTokenizer::Token::TK_EOF)) {
		push_error("Expected end of file.");
	}

	clear_unused_annotations();
}

GDScriptNewParser::ClassNode *GDScriptNewParser::parse_class() {
	ClassNode *n_class = alloc_node<ClassNode>();

	ClassNode *previous_class = current_class;
	current_class = n_class;

	if (consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected identifier for the class name after "class".)")) {
		n_class->identifier = parse_identifier();
	}

	if (check(GDScriptNewTokenizer::Token::EXTENDS)) {
		parse_extends();
	}

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after class declaration.)");
	consume(GDScriptNewTokenizer::Token::NEWLINE, R"(Expected newline after class declaration.)");

	if (!consume(GDScriptNewTokenizer::Token::INDENT, R"(Expected indented block after class declaration.)")) {
		current_class = previous_class;
		return n_class;
	}

	parse_class_body();

	consume(GDScriptNewTokenizer::Token::DEDENT, R"(Missing unindent at the end of the class body.)");

	current_class = previous_class;
	return n_class;
}

void GDScriptNewParser::parse_class_name() {
	if (consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected identifier for the global class name after "class_name".)")) {
		current_class->identifier = parse_identifier();
	}

	// TODO: Move this to annotation
	if (match(GDScriptNewTokenizer::Token::COMMA)) {
		// Icon path.
		if (consume(GDScriptNewTokenizer::Token::LITERAL, R"(Expected class icon path string after ",".)")) {
			if (previous.literal.get_type() != Variant::STRING) {
				push_error(vformat(R"(Only strings can be used for the class icon path, found "%s" instead.)", Variant::get_type_name(previous.literal.get_type())));
			}
			current_class->icon_path = previous.literal;
		}
	}

	if (match(GDScriptNewTokenizer::Token::EXTENDS)) {
		// Allow extends on the same line.
		parse_extends();
	} else {
		end_statement("class_name statement");
	}
}

void GDScriptNewParser::parse_extends() {
	current_class->extends_used = true;

	if (match(GDScriptNewTokenizer::Token::LITERAL)) {
		if (previous.literal.get_type() != Variant::STRING) {
			push_error(vformat(R"(Only strings or identifiers can be used after "extends", found "%s" instead.)", Variant::get_type_name(previous.literal.get_type())));
		}
		current_class->extends_path = previous.literal;

		if (!match(GDScriptNewTokenizer::Token::PERIOD)) {
			end_statement("superclass path");
			return;
		}
	}

	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected superclass name after "extends".)")) {
		return;
	}
	current_class->extends.push_back(previous.literal);

	while (match(GDScriptNewTokenizer::Token::PERIOD)) {
		if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected superclass name after ".".)")) {
			return;
		}
		current_class->extends.push_back(previous.literal);
	}

	end_statement("superclass");
}

template <class T>
void GDScriptNewParser::parse_class_member(T *(GDScriptNewParser::*p_parse_function)(), AnnotationInfo::TargetKind p_target, const String &p_member_kind) {
	advance();
	T *member = (this->*p_parse_function)();
	if (member == nullptr) {
		return;
	}
	// Consume annotations.
	while (!annotation_stack.empty()) {
		AnnotationNode *last_annotation = annotation_stack.back()->get();
		if (last_annotation->applies_to(p_target)) {
			last_annotation->apply(this, member);
			member->annotations.push_front(last_annotation);
			annotation_stack.pop_back();
		} else {
			push_error(vformat(R"(Annotation "%s" cannot be applied to a %s.)", last_annotation->name, p_member_kind));
			clear_unused_annotations();
			return;
		}
	}
	if (current_class->members_indices.has(member->identifier->name)) {
		push_error(vformat(R"(%s "%s" has the same name as a previously declared %s.)", p_member_kind.capitalize(), member->identifier->name, current_class->get_member(member->identifier->name).get_type_name()));
	} else {
		current_class->add_member(member);
	}
}

void GDScriptNewParser::parse_class_body() {
	bool class_end = false;
	while (!class_end && !is_at_end()) {
		switch (current.type) {
			case GDScriptNewTokenizer::Token::VAR:
				parse_class_member(&GDScriptNewParser::parse_variable, AnnotationInfo::VARIABLE, "variable");
				break;
			case GDScriptNewTokenizer::Token::CONST:
				parse_class_member(&GDScriptNewParser::parse_constant, AnnotationInfo::CONSTANT, "constant");
				break;
			case GDScriptNewTokenizer::Token::SIGNAL:
				parse_class_member(&GDScriptNewParser::parse_signal, AnnotationInfo::SIGNAL, "signal");
				break;
			case GDScriptNewTokenizer::Token::STATIC:
			case GDScriptNewTokenizer::Token::FUNC:
				parse_class_member(&GDScriptNewParser::parse_function, AnnotationInfo::FUNCTION, "function");
				break;
			case GDScriptNewTokenizer::Token::CLASS:
				parse_class_member(&GDScriptNewParser::parse_class, AnnotationInfo::CLASS, "class");
				break;
			case GDScriptNewTokenizer::Token::ENUM:
				parse_class_member(&GDScriptNewParser::parse_enum, AnnotationInfo::NONE, "enum");
				break;
			case GDScriptNewTokenizer::Token::ANNOTATION: {
				advance();
				AnnotationNode *annotation = parse_annotation(AnnotationInfo::CLASS_LEVEL);
				if (annotation != nullptr) {
					annotation_stack.push_back(annotation);
				}
				break;
			}
			case GDScriptNewTokenizer::Token::PASS:
				end_statement(R"("pass")");
				break;
			case GDScriptNewTokenizer::Token::DEDENT:
				class_end = true;
				break;
			default:
				push_error(vformat(R"(Unexpected "%s" in class body.)", current.get_name()));
				advance();
				break;
		}
		if (panic_mode) {
			synchronize();
		}
	}
}

GDScriptNewParser::VariableNode *GDScriptNewParser::parse_variable() {
	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected variable name after "var".)")) {
		return nullptr;
	}

	VariableNode *variable = alloc_node<VariableNode>();
	variable->identifier = parse_identifier();

	if (match(GDScriptNewTokenizer::Token::COLON)) {
		if (check((GDScriptNewTokenizer::Token::EQUAL))) {
			// Infer type.
			variable->infer_datatype = true;
		} else {
			// Parse type.
			variable->datatype_specifier = parse_type();
		}
	}

	if (match(GDScriptNewTokenizer::Token::EQUAL)) {
		// Initializer.
		variable->initializer = parse_expression(false);
	}

	end_statement("variable declaration");

	return variable;
}

GDScriptNewParser::ConstantNode *GDScriptNewParser::parse_constant() {
	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected constant name after "const".)")) {
		return nullptr;
	}

	ConstantNode *constant = alloc_node<ConstantNode>();
	constant->identifier = parse_identifier();

	if (match(GDScriptNewTokenizer::Token::COLON)) {
		if (check((GDScriptNewTokenizer::Token::EQUAL))) {
			// Infer type.
			constant->infer_datatype = true;
		} else {
			// Parse type.
			constant->datatype_specifier = parse_type();
		}
	}

	if (consume(GDScriptNewTokenizer::Token::EQUAL, R"(Expected initializer after constant name.)")) {
		// Initializer.
		constant->initializer = parse_expression(false);
	}

	end_statement("constant declaration");

	return constant;
}

GDScriptNewParser::ParameterNode *GDScriptNewParser::parse_parameter() {
	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected parameter name.)")) {
		return nullptr;
	}

	ParameterNode *parameter = alloc_node<ParameterNode>();
	parameter->identifier = parse_identifier();

	if (match(GDScriptNewTokenizer::Token::COLON)) {
		if (check((GDScriptNewTokenizer::Token::EQUAL))) {
			// Infer type.
			parameter->infer_datatype = true;
		} else {
			// Parse type.
			parameter->datatype_specifier = parse_type();
		}
	}

	if (match(GDScriptNewTokenizer::Token::EQUAL)) {
		// Default value.
		parameter->default_value = parse_expression(false);
	}

	return parameter;
}

GDScriptNewParser::SignalNode *GDScriptNewParser::parse_signal() {
	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected signal name after "signal".)")) {
		return nullptr;
	}

	SignalNode *signal = alloc_node<SignalNode>();
	signal->identifier = parse_identifier();

	if (match(GDScriptNewTokenizer::Token::PARENTHESIS_OPEN)) {
		while (!check(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE) && !is_at_end()) {
			ParameterNode *parameter = parse_parameter();
			if (parameter == nullptr) {
				break;
			}
			if (parameter->default_value != nullptr) {
				push_error(R"(Signal parameters cannot have a default value.)");
			}
			if (signal->parameters_indices.has(parameter->identifier->name)) {
				push_error(vformat(R"(Parameter with name "%s" was already declared for this signal.)", parameter->identifier->name));
			} else {
				signal->parameters_indices[parameter->identifier->name] = signal->parameters.size();
				signal->parameters.push_back(parameter);
			}
		}
	}

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after signal parameters.)*");

	end_statement("signal declaration");

	return signal;
}

GDScriptNewParser::EnumNode *GDScriptNewParser::parse_enum() {
	EnumNode *enum_node = alloc_node<EnumNode>();
	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected identifier as enum name after "enum".)")) {
		return nullptr;
	}

	enum_node->identifier = parse_identifier();

	consume(GDScriptNewTokenizer::Token::BRACE_OPEN, R"(Expected "{" after "enum".)");

	do {
		if (consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected identifer for enum key.)")) {
			EnumNode::Value item;
			item.name = parse_identifier();

			if (match(GDScriptNewTokenizer::Token::EQUAL)) {
				if (consume(GDScriptNewTokenizer::Token::LITERAL, R"(Expected integer value after "=".)")) {
					item.value = parse_literal();

					if (item.value->value.get_type() != Variant::INT) {
						push_error(R"(Expected integer value after "=".)");
						item.value = nullptr;
					}
				}
			}
			enum_node->values.push_back(item);
		}
	} while (match(GDScriptNewTokenizer::Token::COMMA));

	match(GDScriptNewTokenizer::Token::COMMA); // Allow trailing comma.

	consume(GDScriptNewTokenizer::Token::BRACE_CLOSE, R"(Expected closing "}" for enum.)");

	end_statement("enum");

	return enum_node;
}

GDScriptNewParser::FunctionNode *GDScriptNewParser::parse_function() {
	bool _static = false;
	if (previous.type == GDScriptNewTokenizer::Token::STATIC) {
		// TODO: Improve message if user uses "static" with "var" or "const"
		if (!consume(GDScriptNewTokenizer::Token::FUNC, R"(Expected "func" after "static".)")) {
			return nullptr;
		}
		_static = true;
	}

	if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected function name after "func".)")) {
		return nullptr;
	}

	FunctionNode *function = alloc_node<FunctionNode>();
	FunctionNode *previous_function = current_function;
	current_function = function;

	function->identifier = parse_identifier();
	function->is_static = _static;

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_OPEN, R"(Expected opening "(" after function name.)");

	if (!check(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE) && !is_at_end()) {
		do {
			ParameterNode *parameter = parse_parameter();
			if (parameter == nullptr) {
				break;
			}
			if (function->parameters_indices.has(parameter->identifier->name)) {
				push_error(vformat(R"(Parameter with name "%s" was already declared for this function.)", parameter->identifier->name));
			} else {
				function->parameters_indices[parameter->identifier->name] = function->parameters.size();
				function->parameters.push_back(parameter);
			}
		} while (match(GDScriptNewTokenizer::Token::COMMA));
	}

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after function parameters.)*");

	if (match(GDScriptNewTokenizer::Token::FORWARD_ARROW)) {
		function->return_type = parse_type(true);
	}

	// TODO: Improve token consumption so it synchronizes to a statement boundary. This way we can get into the function body with unrecognized tokens.
	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after function declaration.)");

	function->body = parse_suite("function declaration");

	current_function = previous_function;
	return function;
}

GDScriptNewParser::AnnotationNode *GDScriptNewParser::parse_annotation(uint32_t p_valid_targets) {
	AnnotationNode *annotation = alloc_node<AnnotationNode>();

	annotation->name = previous.literal;

	bool valid = true;

	if (!valid_annotations.has(annotation->name)) {
		push_error(vformat(R"(Unrecognized annotation: "%s".)", annotation->name));
		valid = false;
	}

	annotation->info = &valid_annotations[annotation->name];

	if (!annotation->applies_to(p_valid_targets)) {
		push_error(vformat(R"(Annotation "%s" is not allowed in this level.)", annotation->name));
		valid = false;
	}

	if (match(GDScriptNewTokenizer::Token::PARENTHESIS_OPEN)) {
		// Arguments.
		if (!check(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE) && !is_at_end()) {
			do {
				ExpressionNode *argument = parse_expression(false);
				if (argument == nullptr) {
					valid = false;
					continue;
				}
				annotation->arguments.push_back(argument);
			} while (match(GDScriptNewTokenizer::Token::COMMA));

			consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after annotation arguments.)*");
		}
	}

	match(GDScriptNewTokenizer::Token::NEWLINE); // Newline after annotation is optional.

	if (valid) {
		valid = validate_annotation_arguments(annotation);
	}

	return valid ? annotation : nullptr;
}

void GDScriptNewParser::clear_unused_annotations() {
	for (const List<AnnotationNode *>::Element *E = annotation_stack.front(); E != nullptr; E = E->next()) {
		AnnotationNode *annotation = E->get();
		push_error(vformat(R"(Annotation "%s" does not precedes a valid target, so it will have no effect.)", annotation->name));
	}

	annotation_stack.clear();
}

bool GDScriptNewParser::register_annotation(const MethodInfo &p_info, uint32_t p_target_kinds, AnnotationAction p_apply, int p_optional_arguments, bool p_is_vararg) {
	ERR_FAIL_COND_V_MSG(valid_annotations.has(p_info.name), false, vformat(R"(Annotation "%s" already registered.)", p_info.name));

	AnnotationInfo new_annotation;
	new_annotation.info = p_info;
	new_annotation.info.default_arguments.resize(p_optional_arguments);
	if (p_is_vararg) {
		new_annotation.info.flags |= METHOD_FLAG_VARARG;
	}
	new_annotation.apply = p_apply;
	new_annotation.target_kind = p_target_kinds;

	valid_annotations[p_info.name] = new_annotation;
	return true;
}

GDScriptNewParser::SuiteNode *GDScriptNewParser::parse_suite(const String &p_context) {
	SuiteNode *suite = alloc_node<SuiteNode>();
	suite->parent_block = current_suite;
	current_suite = suite;

	// TODO: Allow single-line suites.
	consume(GDScriptNewTokenizer::Token::NEWLINE, vformat(R"(Expected newline after %s.)", p_context));

	if (!consume(GDScriptNewTokenizer::Token::INDENT, vformat(R"(Expected indented block after %s.)", p_context))) {
		current_suite = suite->parent_block;
		return suite;
	}

	do {
		Node *statement = parse_statement();
		if (statement == nullptr) {
			continue;
		}
		suite->statements.push_back(statement);

		// Register locals.
		switch (statement->type) {
			case Node::VARIABLE: {
				VariableNode *variable = static_cast<VariableNode *>(statement);
				const SuiteNode::Local &local = current_suite->get_local(variable->identifier->name);
				if (local.type != SuiteNode::Local::UNDEFINED) {
					String name;
					if (local.type == SuiteNode::Local::CONSTANT) {
						name = "constant";
					} else {
						name = "variable";
					}
					push_error(vformat(R"(There is already a %s named "%s" declared in this scope.)", name, variable->identifier->name));
				}
				current_suite->add_local(variable);
				break;
			}
			case Node::CONSTANT: {
				ConstantNode *constant = static_cast<ConstantNode *>(statement);
				const SuiteNode::Local &local = current_suite->get_local(constant->identifier->name);
				if (local.type != SuiteNode::Local::UNDEFINED) {
					String name;
					if (local.type == SuiteNode::Local::CONSTANT) {
						name = "constant";
					} else {
						name = "variable";
					}
					push_error(vformat(R"(There is already a %s named "%s" declared in this scope.)", name, constant->identifier->name));
				}
				current_suite->add_local(constant);
				break;
			}
			default:
				break;
		}

	} while (!check(GDScriptNewTokenizer::Token::DEDENT) && !is_at_end());

	consume(GDScriptNewTokenizer::Token::DEDENT, vformat(R"(Missing unindent at the end of %s.)", p_context));

	current_suite = suite->parent_block;
	return suite;
}

GDScriptNewParser::Node *GDScriptNewParser::parse_statement() {
	Node *result = nullptr;
	switch (current.type) {
		case GDScriptNewTokenizer::Token::PASS:
			advance();
			result = alloc_node<PassNode>();
			end_statement(R"("pass")");
			break;
		case GDScriptNewTokenizer::Token::VAR:
			advance();
			result = parse_variable();
			break;
		case GDScriptNewTokenizer::Token::CONST:
			advance();
			result = parse_constant();
			break;
		case GDScriptNewTokenizer::Token::IF:
			advance();
			result = parse_if();
			break;
		case GDScriptNewTokenizer::Token::FOR:
			advance();
			result = parse_for();
			break;
		case GDScriptNewTokenizer::Token::WHILE:
			advance();
			result = parse_while();
			break;
		case GDScriptNewTokenizer::Token::MATCH:
			advance();
			result = parse_match();
			break;
		case GDScriptNewTokenizer::Token::BREAK:
			advance();
			result = parse_break();
			break;
		case GDScriptNewTokenizer::Token::CONTINUE:
			advance();
			result = parse_continue();
			break;
		case GDScriptNewTokenizer::Token::RETURN: {
			advance();
			ReturnNode *n_return = alloc_node<ReturnNode>();
			if (!is_statement_end()) {
				n_return->return_value = parse_expression(false);
			}
			end_statement("return statement");
			break;
		}
		case GDScriptNewTokenizer::Token::BREAKPOINT:
			advance();
			result = alloc_node<BreakpointNode>();
			end_statement(R"("breakpoint")");
			break;
		case GDScriptNewTokenizer::Token::ASSERT:
			advance();
			result = parse_assert();
			break;
		case GDScriptNewTokenizer::Token::ANNOTATION: {
			advance();
			AnnotationNode *annotation = parse_annotation(AnnotationInfo::STATEMENT);
			if (annotation != nullptr) {
				annotation_stack.push_back(annotation);
			}
			break;
		}
		default: {
			// Expression statement.
			ExpressionNode *expression = parse_expression(true); // Allow assignment here.
			end_statement("expression statement");
			result = expression;
			break;
		}
	}

	if (panic_mode) {
		synchronize();
	}

	return result;
}

GDScriptNewParser::AssertNode *GDScriptNewParser::parse_assert() {
	// TODO: Add assert message.
	AssertNode *assert = alloc_node<AssertNode>();

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_OPEN, R"(Expected "(" after "assert".)");
	assert->to_assert = parse_expression(false);
	if (assert->to_assert == nullptr) {
		push_error("Expected expression to assert.");
	}

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after assert expression.)*");

	end_statement(R"("assert")");

	return assert;
}

GDScriptNewParser::BreakNode *GDScriptNewParser::parse_break() {
	if (!can_break) {
		push_error(R"(Cannot use "break" outside of a loop.)");
	}
	end_statement(R"("break")");
	return alloc_node<BreakNode>();
}

GDScriptNewParser::ContinueNode *GDScriptNewParser::parse_continue() {
	if (!can_continue) {
		push_error(R"(Cannot use "continue" outside of a loop or pattern matching block.)");
	}
	end_statement(R"("continue")");
	return alloc_node<ContinueNode>();
}

GDScriptNewParser::ForNode *GDScriptNewParser::parse_for() {
	ForNode *n_for = alloc_node<ForNode>();

	if (consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected loop variable name after "for".)")) {
		n_for->variable = parse_identifier();
	}

	consume(GDScriptNewTokenizer::Token::IN, R"(Expected "in" after "for" variable name.)");

	n_for->list = parse_expression(false);

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "for" condition.)");

	// Save break/continue state.
	bool could_break = can_break;
	bool could_continue = can_continue;

	// Allow break/continue.
	can_break = true;
	can_continue = true;

	n_for->loop = parse_suite(R"("for" block)");

	// Reset break/continue state.
	can_break = could_break;
	can_continue = could_continue;

	return n_for;
}

GDScriptNewParser::IfNode *GDScriptNewParser::parse_if() {
	IfNode *n_if = alloc_node<IfNode>();

	n_if->condition = parse_expression(false);
	if (n_if->condition == nullptr) {
		push_error(R"(Expected conditional expression after "if".)");
	}

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "if" condition.)");

	n_if->true_block = parse_suite(R"("if" block)");

	if (match(GDScriptNewTokenizer::Token::ELIF)) {
		n_if->elif = parse_if();
	} else if (match(GDScriptNewTokenizer::Token::ELSE)) {
		consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "else".)");
		n_if->false_block = parse_suite(R"("else" block)");
	}

	return n_if;
}

GDScriptNewParser::MatchNode *GDScriptNewParser::parse_match() {
	MatchNode *match = alloc_node<MatchNode>();

	match->test = parse_expression(false);
	if (match->test == nullptr) {
		push_error(R"(Expected expression to test after "match".)");
	}

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "match" expression.)");
	consume(GDScriptNewTokenizer::Token::NEWLINE, R"(Expected a newline after "match" statement.)");

	if (!consume(GDScriptNewTokenizer::Token::INDENT, R"(Expected an indented block after "match" statement.)")) {
		return match;
	}

	while (!check(GDScriptNewTokenizer::Token::DEDENT) && !is_at_end()) {
		MatchBranchNode *branch = parse_match_branch();
		if (branch == nullptr) {
			continue;
		}
		match->branches.push_back(branch);
	}

	consume(GDScriptNewTokenizer::Token::DEDENT, R"(Expected an indented block after "match" statement.)");

	return match;
}

GDScriptNewParser::MatchBranchNode *GDScriptNewParser::parse_match_branch() {
	MatchBranchNode *branch = alloc_node<MatchBranchNode>();

	do {
		PatternNode *pattern = parse_match_pattern();
		if (pattern == nullptr) {
			continue;
		}
		if (branch->patterns.size() > 1 && pattern->pattern_type == PatternNode::PT_BIND) {
			push_error(R"(Cannot use a variable bind with multiple patterns.)");
		}
		branch->patterns.push_back(pattern);
	} while (match(GDScriptNewTokenizer::Token::COMMA));

	if (branch->patterns.empty()) {
		push_error(R"(No pattern found for "match" branch.)");
	}

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "match" patterns.)");

	// Save continue state.
	bool could_continue = can_continue;
	// Allow continue for match.
	can_continue = true;

	branch->block = parse_suite("match pattern block");

	// Restore continue state.
	can_continue = could_continue;

	return branch;
}

GDScriptNewParser::PatternNode *GDScriptNewParser::parse_match_pattern() {
	PatternNode *pattern = alloc_node<PatternNode>();

	switch (current.type) {
		case GDScriptNewTokenizer::Token::LITERAL:
			advance();
			pattern->pattern_type = PatternNode::PT_LITERAL;
			pattern->literal = parse_literal();
			if (pattern->literal == nullptr) {
				// Error happened.
				return nullptr;
			}
			break;
		case GDScriptNewTokenizer::Token::VAR:
			// Bind.
			advance();
			if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected bind name after "var".)")) {
				return nullptr;
			}
			pattern->pattern_type = PatternNode::PT_BIND;
			pattern->bind = parse_identifier();
			break;
		case GDScriptNewTokenizer::Token::UNDERSCORE:
			// Wildcard.
			advance();
			pattern->pattern_type = PatternNode::PT_WILDCARD;
			break;
		case GDScriptNewTokenizer::Token::PERIOD_PERIOD:
			// Rest.
			advance();
			pattern->pattern_type = PatternNode::PT_REST;
			break;
		case GDScriptNewTokenizer::Token::BRACKET_OPEN: {
			// Array.
			advance();
			pattern->pattern_type = PatternNode::PT_ARRAY;

			bool has_rest = false;
			if (!check(GDScriptNewTokenizer::Token::BRACKET_CLOSE)) {
				do {
					PatternNode *sub_pattern = parse_match_pattern();
					if (sub_pattern == nullptr) {
						continue;
					}
					if (has_rest) {
						push_error(R"(The ".." pattern must be the last element in the pattern array.)");
					} else if (sub_pattern->pattern_type == PatternNode::PT_REST) {
						has_rest = true;
					}
					pattern->array.push_back(sub_pattern);
				} while (match(GDScriptNewTokenizer::Token::COMMA));
			}
			consume(GDScriptNewTokenizer::Token::BRACKET_CLOSE, R"(Expected "]" to close the array pattern.)");
			break;
		}
		case GDScriptNewTokenizer::Token::BRACE_OPEN: {
			// Dictionary.
			advance();
			pattern->pattern_type = PatternNode::PT_DICTIONARY;

			bool has_rest = false;
			if (!check(GDScriptNewTokenizer::Token::BRACE_CLOSE) && !is_at_end()) {
				do {
					if (match(GDScriptNewTokenizer::Token::PERIOD_PERIOD)) {
						// Rest.
						if (has_rest) {
							push_error(R"(The ".." pattern must be the last element in the pattern dictionary.)");
						} else {
							PatternNode *sub_pattern = alloc_node<PatternNode>();
							sub_pattern->pattern_type = PatternNode::PT_REST;
							pattern->dictionary.push_back({ nullptr, sub_pattern });
							has_rest = true;
						}
					} else if (consume(GDScriptNewTokenizer::Token::LITERAL, R"(Expected key for dictionary pattern.)")) {
						LiteralNode *key = parse_literal();
						if (key == nullptr) {
							push_error(R"(Expected literal as key for dictionary pattern.)");
							continue;
						}
						PatternNode *sub_pattern = nullptr;
						if (consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after dictionary pattern key)")) {
							// Value pattern.
							sub_pattern = parse_match_pattern();
							if (sub_pattern == nullptr) {
								continue;
							}
							if (has_rest) {
								push_error(R"(The ".." pattern must be the last element in the pattern dictionary.)");
							} else if (sub_pattern->pattern_type == PatternNode::PT_REST) {
								push_error(R"(The ".." pattern cannot be used as a value.)");
							} else {
								pattern->dictionary.push_back({ key, sub_pattern });
							}
						}
					}
				} while (match(GDScriptNewTokenizer::Token::COMMA));
			}
			consume(GDScriptNewTokenizer::Token::BRACE_CLOSE, R"(Expected "}" to close the dictionary pattern.)");
			break;
		}
		default: {
			// Expression.
			ExpressionNode *expression = parse_expression(false);
			if (expression == nullptr) {
				push_error(R"(Expected expression for match pattern.)");
			} else {
				pattern->pattern_type = PatternNode::PT_EXPRESSION;
				pattern->expression = expression;
			}
			break;
		}
	}

	return pattern;
}

GDScriptNewParser::WhileNode *GDScriptNewParser::parse_while() {
	WhileNode *n_while = alloc_node<WhileNode>();

	n_while->condition = parse_expression(false);
	if (n_while->condition == nullptr) {
		push_error(R"(Expected conditional expression after "while".)");
	}

	consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after "while" condition.)");

	// Save break/continue state.
	bool could_break = can_break;
	bool could_continue = can_continue;

	// Allow break/continue.
	can_break = true;
	can_continue = true;

	n_while->loop = parse_suite(R"("while" block)");

	// Reset break/continue state.
	can_break = could_break;
	can_continue = could_continue;

	return n_while;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_precedence(Precedence p_precedence, bool p_can_assign) {
	GDScriptNewTokenizer::Token token = advance();
	ParseFunction prefix_rule = get_rule(token.type)->prefix;

	if (prefix_rule == nullptr) {
		// Expected expression. Let the caller give the proper error message.
		return nullptr;
	}

	ExpressionNode *previous_operand = (this->*prefix_rule)(nullptr, p_can_assign);

	while (p_precedence <= get_rule(current.type)->precedence) {
		token = advance();
		ParseFunction infix_rule = get_rule(token.type)->infix;
		previous_operand = (this->*infix_rule)(previous_operand, p_can_assign);
	}

	return previous_operand;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_expression(bool p_can_assign) {
	return parse_precedence(PREC_ASSIGNMENT, p_can_assign);
}

GDScriptNewParser::IdentifierNode *GDScriptNewParser::parse_identifier() {
	return static_cast<IdentifierNode *>(parse_identifier(nullptr, false));
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_identifier(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (!previous.is_identifier()) {
		ERR_FAIL_V_MSG(nullptr, "Parser bug: parsing literal node without literal token.");
	}
	IdentifierNode *identifier = alloc_node<IdentifierNode>();
	identifier->name = previous.literal;
	return identifier;
}

GDScriptNewParser::LiteralNode *GDScriptNewParser::parse_literal() {
	return static_cast<LiteralNode *>(parse_literal(nullptr, false));
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_literal(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (previous.type != GDScriptNewTokenizer::Token::LITERAL) {
		push_error("Parser bug: parsing literal node without literal token.");
		ERR_FAIL_V_MSG(nullptr, "Parser bug: parsing literal node without literal token.");
	}

	LiteralNode *literal = alloc_node<LiteralNode>();
	literal->value = previous.literal;
	return literal;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_self(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// FIXME: Don't allow "self" in a static context.
	SelfNode *self = alloc_node<SelfNode>();
	self->current_class = current_class;
	return self;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_unary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	GDScriptNewTokenizer::Token::Type op_type = previous.type;
	UnaryOpNode *operation = alloc_node<UnaryOpNode>();

	switch (op_type) {
		case GDScriptNewTokenizer::Token::MINUS:
			operation->operation = UnaryOpNode::OP_NEGATIVE;
			operation->operand = parse_precedence(PREC_SIGN, false);
			break;
		case GDScriptNewTokenizer::Token::PLUS:
			operation->operation = UnaryOpNode::OP_POSITIVE;
			operation->operand = parse_precedence(PREC_SIGN, false);
			break;
		case GDScriptNewTokenizer::Token::TILDE:
			operation->operation = UnaryOpNode::OP_COMPLEMENT;
			operation->operand = parse_precedence(PREC_BIT_NOT, false);
			break;
		case GDScriptNewTokenizer::Token::NOT:
			operation->operation = UnaryOpNode::OP_LOGIC_NOT;
			operation->operand = parse_precedence(PREC_LOGIC_NOT, false);
			break;
		default:
			return nullptr; // Unreachable.
	}

	return operation;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_binary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	GDScriptNewTokenizer::Token::Type op_type = previous.type;
	BinaryOpNode *operation = alloc_node<BinaryOpNode>();

	Precedence precedence = (Precedence)(get_rule(op_type)->precedence + 1);
	operation->left_operand = p_previous_operand;
	operation->right_operand = parse_precedence(precedence, false);

	switch (op_type) {
		case GDScriptNewTokenizer::Token::PLUS:
			operation->operation = BinaryOpNode::OP_ADDITION;
			break;
		case GDScriptNewTokenizer::Token::MINUS:
			operation->operation = BinaryOpNode::OP_SUBTRACTION;
			break;
		case GDScriptNewTokenizer::Token::STAR:
			operation->operation = BinaryOpNode::OP_MULTIPLICATION;
			break;
		case GDScriptNewTokenizer::Token::SLASH:
			operation->operation = BinaryOpNode::OP_DIVISION;
			break;
		case GDScriptNewTokenizer::Token::PERCENT:
			operation->operation = BinaryOpNode::OP_MODULO;
			break;
		case GDScriptNewTokenizer::Token::LESS_LESS:
			operation->operation = BinaryOpNode::OP_BIT_LEFT_SHIFT;
			break;
		case GDScriptNewTokenizer::Token::GREATER_GREATER:
			operation->operation = BinaryOpNode::OP_BIT_RIGHT_SHIFT;
			break;
		case GDScriptNewTokenizer::Token::AMPERSAND:
			operation->operation = BinaryOpNode::OP_BIT_AND;
			break;
		case GDScriptNewTokenizer::Token::PIPE:
			operation->operation = BinaryOpNode::OP_BIT_AND;
			break;
		case GDScriptNewTokenizer::Token::CARET:
			operation->operation = BinaryOpNode::OP_BIT_XOR;
			break;
		case GDScriptNewTokenizer::Token::AND:
			operation->operation = BinaryOpNode::OP_LOGIC_AND;
			break;
		case GDScriptNewTokenizer::Token::OR:
			operation->operation = BinaryOpNode::OP_LOGIC_OR;
			break;
		case GDScriptNewTokenizer::Token::IS:
			operation->operation = BinaryOpNode::OP_TYPE_TEST;
			break;
		case GDScriptNewTokenizer::Token::IN:
			operation->operation = BinaryOpNode::OP_CONTENT_TEST;
			break;
		case GDScriptNewTokenizer::Token::EQUAL_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_EQUAL;
			break;
		case GDScriptNewTokenizer::Token::BANG_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_NOT_EQUAL;
			break;
		case GDScriptNewTokenizer::Token::LESS:
			operation->operation = BinaryOpNode::OP_COMP_LESS;
			break;
		case GDScriptNewTokenizer::Token::LESS_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_LESS_EQUAL;
			break;
		case GDScriptNewTokenizer::Token::GREATER:
			operation->operation = BinaryOpNode::OP_COMP_GREATER;
			break;
		case GDScriptNewTokenizer::Token::GREATER_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_GREATER_EQUAL;
			break;
		default:
			return nullptr; // Unreachable.
	}

	return operation;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_ternary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// Only one ternary operation exists, so no abstraction here.
	TernaryOpNode *operation = alloc_node<TernaryOpNode>();
	operation->true_expr = p_previous_operand;

	consume(GDScriptNewTokenizer::Token::ELSE, R"(Expected "else" after ternary operator condition.)");

	operation->false_expr = parse_precedence(PREC_TERNARY, false);

	return operation;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_assignment(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (!p_can_assign) {
		push_error("Assignment is not allowed inside an expression.");
		return parse_expression(false); // Return the following expression.
	}

	switch (p_previous_operand->type) {
		case Node::IDENTIFIER:
		case Node::ATTRIBUTE:
		case Node::SUBSCRIPT:
			// Okay.
			break;
		default:
			push_error(R"(Only identifier, attribute access, and subscription access can be used as assignment target.)");
			return parse_expression(false); // Return the following expression.
	}

	AssignmentNode *assignment = alloc_node<AssignmentNode>();
	switch (previous.type) {
		case GDScriptNewTokenizer::Token::EQUAL:
			assignment->operation = AssignmentNode::OP_NONE;
			break;
		case GDScriptNewTokenizer::Token::PLUS_EQUAL:
			assignment->operation = AssignmentNode::OP_ADDITION;
			break;
		case GDScriptNewTokenizer::Token::MINUS_EQUAL:
			assignment->operation = AssignmentNode::OP_SUBTRACTION;
			break;
		case GDScriptNewTokenizer::Token::STAR_EQUAL:
			assignment->operation = AssignmentNode::OP_MULTIPLICATION;
			break;
		case GDScriptNewTokenizer::Token::SLASH_EQUAL:
			assignment->operation = AssignmentNode::OP_DIVISION;
			break;
		case GDScriptNewTokenizer::Token::PERCENT_EQUAL:
			assignment->operation = AssignmentNode::OP_MODULO;
			break;
		case GDScriptNewTokenizer::Token::LESS_LESS_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_SHIFT_LEFT;
			break;
		case GDScriptNewTokenizer::Token::GREATER_GREATER_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_SHIFT_RIGHT;
			break;
		case GDScriptNewTokenizer::Token::AMPERSAND_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_AND;
			break;
		case GDScriptNewTokenizer::Token::PIPE_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_OR;
			break;
		case GDScriptNewTokenizer::Token::CARET_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_XOR;
			break;
		default:
			break; // Unreachable.
	}
	assignment->assignee = p_previous_operand;
	assignment->assigned_value = parse_expression(false);

	return assignment;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_await(ExpressionNode *p_previous_operand, bool p_can_assign) {
	AwaitNode *await = alloc_node<AwaitNode>();
	await->to_await = parse_precedence(PREC_AWAIT, false);

	return await;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_array(ExpressionNode *p_previous_operand, bool p_can_assign) {
	ArrayNode *array = alloc_node<ArrayNode>();

	if (!check(GDScriptNewTokenizer::Token::BRACKET_CLOSE)) {
		do {
			if (check(GDScriptNewTokenizer::Token::BRACKET_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			ExpressionNode *element = parse_expression(false);
			if (element == nullptr) {
				push_error(R"(Expected expression as array element.)");
			} else {
				array->elements.push_back(element);
			}
		} while (match(GDScriptNewTokenizer::Token::COMMA) && !is_at_end());
	}
	consume(GDScriptNewTokenizer::Token::BRACKET_CLOSE, R"(Expected closing "]" after array elements.)");

	return array;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_dictionary(ExpressionNode *p_previous_operand, bool p_can_assign) {
	DictionaryNode *dictionary = alloc_node<DictionaryNode>();

	if (!check(GDScriptNewTokenizer::Token::BRACE_CLOSE)) {
		do {
			if (check(GDScriptNewTokenizer::Token::BRACE_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			// Key.
			ExpressionNode *key = parse_expression(false);
			if (key == nullptr) {
				push_error(R"(Expected expression as dictionary key.)");
			}

			consume(GDScriptNewTokenizer::Token::COLON, R"(Expected ":" after dictionary key.)");

			// Value.
			ExpressionNode *value = parse_expression(false);
			if (key == nullptr) {
				push_error(R"(Expected expression as dictionary value.)");
			}

			if (key != nullptr && value != nullptr) {
				dictionary->elements.push_back({ key, value });
			}
		} while (match(GDScriptNewTokenizer::Token::COMMA) && !is_at_end());
	}
	consume(GDScriptNewTokenizer::Token::BRACE_CLOSE, R"(Expected closing "}" after dictionary elements.)");

	return dictionary;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_grouping(ExpressionNode *p_previous_operand, bool p_can_assign) {
	ExpressionNode *grouped = parse_expression(false);
	consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after grouping expression.)*");
	return grouped;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_attribute(ExpressionNode *p_previous_operand, bool p_can_assign) {
	AttributeNode *attribute = alloc_node<AttributeNode>();

	attribute->base = p_previous_operand;

	do {
		if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expected identifier after "." for attribute access.)")) {
			break;
		}
		IdentifierNode *identifier = parse_identifier();
		attribute->attribute_chain.push_back(identifier);
	} while (match(GDScriptNewTokenizer::Token::PERIOD));

	return attribute;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_subscript(ExpressionNode *p_previous_operand, bool p_can_assign) {
	SubscriptNode *subscript = alloc_node<SubscriptNode>();

	subscript->base = p_previous_operand;
	subscript->index = parse_expression(false);

	consume(GDScriptNewTokenizer::Token::BRACKET_CLOSE, R"(Expected "]" after subscription index.)");

	return subscript;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_cast(ExpressionNode *p_previous_operand, bool p_can_assign) {
	CastNode *cast = alloc_node<CastNode>();

	cast->operand = p_previous_operand;
	cast->cast_type = parse_type();

	if (cast->cast_type == nullptr) {
		push_error(R"(Expected type specifier after "as".)");
		return p_previous_operand;
	}

	return cast;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_call(ExpressionNode *p_previous_operand, bool p_can_assign) {
	CallNode *call = alloc_node<CallNode>();

	call->callee = p_previous_operand;

	if (!check(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE)) {
		// Arguments.
		do {
			ExpressionNode *argument = parse_expression(false);
			if (argument == nullptr) {
				push_error(R"(Expected expression as the function argument.)");
			} else {
				call->arguments.push_back(argument);
			}
		} while (match(GDScriptNewTokenizer::Token::COMMA));
	}

	consume(GDScriptNewTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after call arguments.)*");

	return call;
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_get_node(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (match(GDScriptNewTokenizer::Token::LITERAL)) {
		if (previous.literal.get_type() != Variant::STRING) {
			push_error(R"(Expect node path as string or identifer after "$".)");
			return nullptr;
		}
		GetNodeNode *get_node = alloc_node<GetNodeNode>();
		get_node->string = parse_literal();
		return get_node;
	} else if (check(GDScriptNewTokenizer::Token::IDENTIFIER)) {
		GetNodeNode *get_node = alloc_node<GetNodeNode>();
		do {
			if (!consume(GDScriptNewTokenizer::Token::IDENTIFIER, R"(Expect node identifer after "/".)")) {
				return nullptr;
			}
			IdentifierNode *identifier = parse_identifier();
			get_node->chain.push_back(identifier);
		} while (match(GDScriptNewTokenizer::Token::SLASH));
		return get_node;
	} else {
		push_error(R"(Expect node path as string or identifer after "$".)");
		return nullptr;
	}
}

GDScriptNewParser::ExpressionNode *GDScriptNewParser::parse_invalid_token(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// Just for better error messages.
	GDScriptNewTokenizer::Token::Type invalid = previous.type;

	switch (invalid) {
		case GDScriptNewTokenizer::Token::QUESTION_MARK:
			push_error(R"(Unexpected "?" in source. If you want a ternary operator, use "truthy_value if true_condition else falsy_value".)");
			break;
		default:
			return nullptr; // Unreachable.
	}

	// Return the previous expression.
	return p_previous_operand;
}

GDScriptNewParser::TypeNode *GDScriptNewParser::parse_type(bool p_allow_void) {
	if (!match(GDScriptNewTokenizer::Token::IDENTIFIER)) {
		if (match(GDScriptNewTokenizer::Token::VOID)) {
			if (p_allow_void) {
				TypeNode *type = alloc_node<TypeNode>();
				return type;
			} else {
				push_error(R"("void" is only allowed for a function return type.)");
			}
		}
		// Leave error message to the caller who knows the context.
		return nullptr;
	}
	TypeNode *type = alloc_node<TypeNode>();
	IdentifierNode *type_base = parse_identifier();

	if (match(GDScriptNewTokenizer::Token::PERIOD)) {
		type->type_specifier = static_cast<AttributeNode *>(parse_attribute(type_base, false));
		if (type->type_specifier->attribute_chain.size() == 0) {
			return nullptr;
		}
	} else {
		type->type_base = type_base;
	}

	return type;
}

GDScriptNewParser::ParseRule *GDScriptNewParser::get_rule(GDScriptNewTokenizer::Token::Type p_token_type) {
	// Function table for expression parsing.
	// clang-format destroys the alignment here, so turn off for the table.
	/* clang-format off */
	static ParseRule rules[] = {
		// PREFIX                                           INFIX                                           PRECEDENCE (for binary)
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // EMPTY,
		// Basic
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ANNOTATION,
		{ &GDScriptNewParser::parse_identifier,             nullptr,                                        PREC_NONE }, // IDENTIFIER,
		{ &GDScriptNewParser::parse_literal,                nullptr,                                        PREC_NONE }, // LITERAL,
		// Comparison
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // LESS,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // LESS_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // GREATER,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // GREATER_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // EQUAL_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_COMPARISON }, // BANG_EQUAL,
		// Logical
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_LOGIC_AND }, // AND,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_LOGIC_NOT }, // OR,
		{ &GDScriptNewParser::parse_unary_operator,         nullptr,                                        PREC_NONE }, // NOT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // AMPERSAND_AMPERSAND,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PIPE_PIPE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BANG,
		// Bitwise
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_BIT_AND }, // AMPERSAND,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_BIT_OR }, // PIPE,
		{ &GDScriptNewParser::parse_unary_operator,         nullptr,                                        PREC_NONE }, // TILDE,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_BIT_XOR }, // CARET,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_BIT_SHIFT }, // LESS_LESS,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_BIT_SHIFT }, // GREATER_GREATER,
		// Math
		{ &GDScriptNewParser::parse_unary_operator,         &GDScriptNewParser::parse_binary_operator,      PREC_ADDITION }, // PLUS,
		{ &GDScriptNewParser::parse_unary_operator,         &GDScriptNewParser::parse_binary_operator,      PREC_SUBTRACTION }, // MINUS,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_FACTOR }, // STAR,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_FACTOR }, // SLASH,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_FACTOR }, // PERCENT,
		// Assignment
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // PLUS_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // MINUS_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // STAR_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // SLASH_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // PERCENT_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // LESS_LESS_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // GREATER_GREATER_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // AMPERSAND_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // PIPE_EQUAL,
		{ nullptr,                                          &GDScriptNewParser::parse_assignment,           PREC_ASSIGNMENT }, // CARET_EQUAL,
		// Control flow
		{ nullptr,                                          &GDScriptNewParser::parse_ternary_operator,     PREC_TERNARY }, // IF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ELIF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ELSE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // FOR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // WHILE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BREAK,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONTINUE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PASS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // RETURN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // MATCH,
		// Keywords
		{ nullptr,                                          &GDScriptNewParser::parse_cast,                 PREC_CAST }, // AS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ASSERT,
		{ &GDScriptNewParser::parse_await,                  nullptr,                                        PREC_NONE }, // AWAIT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BREAKPOINT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CLASS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CLASS_NAME,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ENUM,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // EXTENDS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // FUNC,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_CONTENT_TEST }, // IN,
		{ nullptr,                                          &GDScriptNewParser::parse_binary_operator,      PREC_TYPE_TEST }, // IS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // NAMESPACE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PRELOAD,
		{ &GDScriptNewParser::parse_self,                   nullptr,                                        PREC_NONE }, // SELF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // SIGNAL,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // STATIC,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VAR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VOID,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // YIELD,
		// Punctuation
		{ &GDScriptNewParser::parse_array,                  &GDScriptNewParser::parse_subscript,            PREC_SUBSCRIPT }, // BRACKET_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BRACKET_CLOSE,
		{ &GDScriptNewParser::parse_dictionary,             nullptr,                                        PREC_NONE }, // BRACE_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BRACE_CLOSE,
		{ &GDScriptNewParser::parse_grouping,               &GDScriptNewParser::parse_call,                 PREC_CALL }, // PARENTHESIS_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PARENTHESIS_CLOSE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // COMMA,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // SEMICOLON,
		{ nullptr,                                          &GDScriptNewParser::parse_attribute,            PREC_ATTRIBUTE }, // PERIOD,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PERIOD_PERIOD,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // COLON,
		{ &GDScriptNewParser::parse_get_node,               nullptr,                                        PREC_NONE }, // DOLLAR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // FORWARD_ARROW,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // UNDERSCORE,
		// Whitespace
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // NEWLINE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // INDENT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // DEDENT,
		// Constants
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST_PI,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST_TAU,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST_INF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST_NAN,
		// Error message improvement
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VCS_CONFLICT_MARKER,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BACKTICK,
		{ nullptr,                                          &GDScriptNewParser::parse_invalid_token,        PREC_CAST }, // QUESTION_MARK,
		// Special
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ERROR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // TK_EOF,
	};
	/* clang-format on */
	// Avoid desync.
	static_assert(sizeof(rules) / sizeof(rules[0]) == GDScriptNewTokenizer::Token::TK_MAX, "Amount of parse rules don't match the amount of token types.");

	// Let's assume this this never invalid, since nothing generates a TK_MAX.
	return &rules[p_token_type];
}

bool GDScriptNewParser::SuiteNode::has_local(const StringName &p_name) const {
	if (locals_indices.has(p_name)) {
		return true;
	}
	if (parent_block != nullptr) {
		return parent_block->has_local(p_name);
	}
	return false;
}

const GDScriptNewParser::SuiteNode::Local &GDScriptNewParser::SuiteNode::get_local(const StringName &p_name) const {
	if (locals_indices.has(p_name)) {
		return locals[locals_indices[p_name]];
	}
	if (parent_block != nullptr) {
		return parent_block->get_local(p_name);
	}
	return empty;
}

bool GDScriptNewParser::AnnotationNode::apply(GDScriptNewParser *p_this, Node *p_target) const {
	return (p_this->*(p_this->valid_annotations[name].apply))(this, p_target);
}

bool GDScriptNewParser::AnnotationNode::applies_to(uint32_t p_target_kinds) const {
	return (info->target_kind & p_target_kinds) > 0;
}

bool GDScriptNewParser::validate_annotation_arguments(AnnotationNode *p_annotation) {
	ERR_FAIL_COND_V_MSG(!valid_annotations.has(p_annotation->name), false, vformat(R"(Annotation "%s" not found to validate.)", p_annotation->name));

	const MethodInfo &info = valid_annotations[p_annotation->name].info;

	if (((info.flags & METHOD_FLAG_VARARG) == 0) && p_annotation->arguments.size() > info.arguments.size()) {
		push_error(vformat(R"(Annotation "%s" requires at most %d arguments, but %d were given.)", p_annotation->name, info.arguments.size(), p_annotation->arguments.size()));
		return false;
	}

	if (p_annotation->arguments.size() < info.arguments.size() - info.default_arguments.size()) {
		push_error(vformat(R"(Annotation "%s" requires at least %d arguments, but %d were given.)", p_annotation->name, info.arguments.size() - info.default_arguments.size(), p_annotation->arguments.size()));
		return false;
	}

	const List<PropertyInfo>::Element *E = info.arguments.front();
	for (int i = 0; i < p_annotation->arguments.size(); i++) {
		ExpressionNode *argument = p_annotation->arguments[i];
		const PropertyInfo &parameter = E->get();

		if (E->next() != nullptr) {
			E = E->next();
		}

		switch (parameter.type) {
			case Variant::STRING:
			case Variant::STRING_NAME:
			case Variant::NODE_PATH:
				// Allow "quote-less strings", as long as they are recognized as identifiers.
				if (argument->type == Node::IDENTIFIER) {
					IdentifierNode *string = static_cast<IdentifierNode *>(argument);
					Callable::CallError error;
					Vector<Variant> args = varray(string->name);
					const Variant *name = args.ptr();
					p_annotation->resolved_arguments.push_back(Variant::construct(parameter.type, &(name), 1, error));
					if (error.error != Callable::CallError::CALL_OK) {
						push_error(vformat(R"(Expected %s as argument %d of annotation "%s").)", Variant::get_type_name(parameter.type), i + 1, p_annotation->name));
						p_annotation->resolved_arguments.remove(p_annotation->resolved_arguments.size() - 1);
						return false;
					}
					break;
				}
				[[fallthrough]];
			default: {
				if (argument->type != Node::LITERAL) {
					push_error(vformat(R"(Expected %s as argument %d of annotation "%s").)", Variant::get_type_name(parameter.type), i + 1, p_annotation->name));
					return false;
				}

				Variant value = static_cast<LiteralNode *>(argument)->value;
				if (!Variant::can_convert_strict(value.get_type(), parameter.type)) {
					push_error(vformat(R"(Expected %s as argument %d of annotation "%s").)", Variant::get_type_name(parameter.type), i + 1, p_annotation->name));
					return false;
				}
				Callable::CallError error;
				const Variant *args = &value;
				p_annotation->resolved_arguments.push_back(Variant::construct(parameter.type, &(args), 1, error));
				if (error.error != Callable::CallError::CALL_OK) {
					push_error(vformat(R"(Expected %s as argument %d of annotation "%s").)", Variant::get_type_name(parameter.type), i + 1, p_annotation->name));
					p_annotation->resolved_arguments.remove(p_annotation->resolved_arguments.size() - 1);
					return false;
				}
				break;
			}
		}
	}

	return true;
}

bool GDScriptNewParser::tool_annotation(const AnnotationNode *p_annotation, Node *p_node) {
	this->_is_tool = true;
	return true;
}

bool GDScriptNewParser::icon_annotation(const AnnotationNode *p_annotation, Node *p_node) {
	ERR_FAIL_COND_V_MSG(p_node->type != Node::CLASS, false, R"("@icon" annotation can only be applied to classes.)");
	ClassNode *p_class = static_cast<ClassNode *>(p_node);
	p_class->icon_path = p_annotation->resolved_arguments[0];
	return true;
}

template <PropertyHint t_hint>
bool GDScriptNewParser::export_annotations(const AnnotationNode *p_annotation, Node *p_node) {
	ERR_FAIL_COND_V_MSG(p_node->type != Node::VARIABLE, false, vformat(R"("%s" annotation can only be applied to variables.)", p_annotation->name));

	VariableNode *variable = static_cast<VariableNode *>(p_node);
	if (variable->exported) {
		push_error(vformat(R"(Annotation "%s" cannot be used with another "@export" annotation.)", p_annotation->name));
		return false;
	}

	variable->exported = true;

	if (p_annotation->name == "@export") {
		if (variable->datatype_specifier == nullptr && variable->initializer == nullptr) {
			push_error(R"(Cannot use bare "@export" annotation with variable without type or initializer, since type can't be inferred.)");
			return false;
		}
		// Actual type will be set by the analyzer, which can infer the proper type.
	}

	StringBuilder hint_string;
	for (int i = 0; i < p_annotation->resolved_arguments.size(); i++) {
		if (i > 0) {
			hint_string += ",";
		}
		hint_string += String(p_annotation->resolved_arguments[i]);
	}

	variable->export_info.hint = t_hint;
	variable->export_info.hint_string = hint_string;

	return true;
}

bool GDScriptNewParser::warning_annotations(const AnnotationNode *p_annotation, Node *p_node) {
	ERR_FAIL_V_MSG(false, "Not implemented.");
}

template <MultiplayerAPI::RPCMode t_mode>
bool GDScriptNewParser::network_annotations(const AnnotationNode *p_annotation, Node *p_node) {
	ERR_FAIL_COND_V_MSG(p_node->type != Node::VARIABLE && p_node->type != Node::FUNCTION, false, vformat(R"("%s" annotation can only be applied to variables and functions.)", p_annotation->name));

	switch (p_node->type) {
		case Node::VARIABLE: {
			VariableNode *variable = static_cast<VariableNode *>(p_node);
			if (variable->rpc_mode != MultiplayerAPI::RPC_MODE_DISABLED) {
				push_error(R"(RPC annotations can only be used once per variable.)");
			}
			variable->rpc_mode = t_mode;
			break;
		}
		case Node::FUNCTION: {
			FunctionNode *function = static_cast<FunctionNode *>(p_node);
			if (function->rpc_mode != MultiplayerAPI::RPC_MODE_DISABLED) {
				push_error(R"(RPC annotations can only be used once per function.)");
			}
			function->rpc_mode = t_mode;
			break;
		}
		default:
			return false; // Unreachable.
	}

	return true;
}

/*---------- PRETTY PRINT FOR DEBUG ----------*/

#ifdef DEBUG_ENABLED

void GDScriptNewParser::TreePrinter::increase_indent() {
	indent_level++;
	indent = "";
	for (int i = 0; i < indent_level * 4; i++) {
		if (i % 4 == 0) {
			indent += "|";
		} else {
			indent += " ";
		}
	}
}

void GDScriptNewParser::TreePrinter::decrease_indent() {
	indent_level--;
	indent = "";
	for (int i = 0; i < indent_level * 4; i++) {
		if (i % 4 == 0) {
			indent += "|";
		} else {
			indent += " ";
		}
	}
}

void GDScriptNewParser::TreePrinter::push_line(const String &p_line) {
	if (!p_line.empty()) {
		push_text(p_line);
	}
	printed += "\n";
	pending_indent = true;
}

void GDScriptNewParser::TreePrinter::push_text(const String &p_text) {
	if (pending_indent) {
		printed += indent;
		pending_indent = false;
	}
	printed += p_text;
}

void GDScriptNewParser::TreePrinter::print_annotation(AnnotationNode *p_annotation) {
	push_text(p_annotation->name);
	push_text(" (");
	for (int i = 0; i < p_annotation->arguments.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_annotation->arguments[i]);
	}
	push_line(")");
}

void GDScriptNewParser::TreePrinter::print_array(ArrayNode *p_array) {
	push_text("[ ");
	for (int i = 0; i < p_array->elements.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_array->elements[i]);
	}
	push_text(" ]");
}

void GDScriptNewParser::TreePrinter::print_assert(AssertNode *p_assert) {
	push_text("Assert ( ");
	print_expression(p_assert->to_assert);
	push_line(" )");
}

void GDScriptNewParser::TreePrinter::print_assignment(AssignmentNode *p_assignment) {
	switch (p_assignment->assignee->type) {
		case Node::ATTRIBUTE:
			print_attribute(static_cast<AttributeNode *>(p_assignment->assignee));
			break;
		case Node::IDENTIFIER:
			print_identifier(static_cast<IdentifierNode *>(p_assignment->assignee));
			break;
		case Node::SUBSCRIPT:
			print_subscript(static_cast<SubscriptNode *>(p_assignment->assignee));
			break;
		default:
			break; // Unreachable.
	}

	push_text(" ");
	switch (p_assignment->operation) {
		case AssignmentNode::OP_ADDITION:
			push_text("+");
			break;
		case AssignmentNode::OP_SUBTRACTION:
			push_text("-");
			break;
		case AssignmentNode::OP_MULTIPLICATION:
			push_text("*");
			break;
		case AssignmentNode::OP_DIVISION:
			push_text("/");
			break;
		case AssignmentNode::OP_MODULO:
			push_text("%");
			break;
		case AssignmentNode::OP_BIT_SHIFT_LEFT:
			push_text("<<");
			break;
		case AssignmentNode::OP_BIT_SHIFT_RIGHT:
			push_text(">>");
			break;
		case AssignmentNode::OP_BIT_AND:
			push_text("&");
			break;
		case AssignmentNode::OP_BIT_OR:
			push_text("|");
			break;
		case AssignmentNode::OP_BIT_XOR:
			push_text("^");
			break;
		case AssignmentNode::OP_NONE:
			break;
	}
	push_text("= ");
	print_expression(p_assignment->assigned_value);
	push_line();
}

void GDScriptNewParser::TreePrinter::print_attribute(AttributeNode *p_attribute) {
	print_expression(p_attribute->base);
	for (int i = 0; i < p_attribute->attribute_chain.size(); i++) {
		push_text(".");
		print_identifier(p_attribute->attribute_chain[i]);
	}
}

void GDScriptNewParser::TreePrinter::print_await(AwaitNode *p_await) {
	push_text("Await ");
	print_expression(p_await->to_await);
}

void GDScriptNewParser::TreePrinter::print_binary_op(BinaryOpNode *p_binary_op) {
	print_expression(p_binary_op->left_operand);
	switch (p_binary_op->operation) {
		case BinaryOpNode::OP_ADDITION:
			push_text(" + ");
			break;
		case BinaryOpNode::OP_SUBTRACTION:
			push_text(" - ");
			break;
		case BinaryOpNode::OP_MULTIPLICATION:
			push_text(" * ");
			break;
		case BinaryOpNode::OP_DIVISION:
			push_text(" / ");
			break;
		case BinaryOpNode::OP_MODULO:
			push_text(" % ");
			break;
		case BinaryOpNode::OP_BIT_LEFT_SHIFT:
			push_text(" << ");
			break;
		case BinaryOpNode::OP_BIT_RIGHT_SHIFT:
			push_text(" >> ");
			break;
		case BinaryOpNode::OP_BIT_AND:
			push_text(" & ");
			break;
		case BinaryOpNode::OP_BIT_OR:
			push_text(" | ");
			break;
		case BinaryOpNode::OP_BIT_XOR:
			push_text(" ^ ");
			break;
		case BinaryOpNode::OP_LOGIC_AND:
			push_text(" AND ");
			break;
		case BinaryOpNode::OP_LOGIC_OR:
			push_text(" OR ");
			break;
		case BinaryOpNode::OP_TYPE_TEST:
			push_text(" IS ");
			break;
		case BinaryOpNode::OP_CONTENT_TEST:
			push_text(" IN ");
			break;
		case BinaryOpNode::OP_COMP_EQUAL:
			push_text(" == ");
			break;
		case BinaryOpNode::OP_COMP_NOT_EQUAL:
			push_text(" != ");
			break;
		case BinaryOpNode::OP_COMP_LESS:
			push_text(" < ");
			break;
		case BinaryOpNode::OP_COMP_LESS_EQUAL:
			push_text(" <= ");
			break;
		case BinaryOpNode::OP_COMP_GREATER:
			push_text(" > ");
			break;
		case BinaryOpNode::OP_COMP_GREATER_EQUAL:
			push_text(" >= ");
			break;
	}
	print_expression(p_binary_op->right_operand);
}

void GDScriptNewParser::TreePrinter::print_call(CallNode *p_call) {
	print_expression(p_call->callee);
	push_text("( ");
	for (int i = 0; i < p_call->arguments.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_call->arguments[i]);
	}
	push_text(" )");
}

void GDScriptNewParser::TreePrinter::print_cast(CastNode *p_cast) {
	print_expression(p_cast->operand);
	push_text(" AS ");
	print_type(p_cast->cast_type);
}

void GDScriptNewParser::TreePrinter::print_class(ClassNode *p_class) {
	push_text("Class ");
	if (p_class->identifier == nullptr) {
		push_text("<unnamed>");
	} else {
		print_identifier(p_class->identifier);
	}

	if (p_class->extends_used) {
		bool first = true;
		push_text(" Extends ");
		if (!p_class->extends_path.empty()) {
			push_text(vformat(R"("%s")", p_class->extends_path));
			first = false;
		}
		for (int i = 0; i < p_class->extends.size(); i++) {
			if (!first) {
				push_text(".");
			} else {
				first = false;
			}
			push_text(p_class->extends[i]);
		}
	}

	push_line(" :");

	increase_indent();

	for (int i = 0; i < p_class->members.size(); i++) {
		const ClassNode::Member &m = p_class->members[i];

		switch (m.type) {
			case ClassNode::Member::CLASS:
				print_class(m.m_class);
				break;
			case ClassNode::Member::VARIABLE:
				print_variable(m.variable);
				break;
			case ClassNode::Member::CONSTANT:
				print_constant(m.constant);
				break;
			case ClassNode::Member::SIGNAL:
				print_signal(m.signal);
				break;
			case ClassNode::Member::FUNCTION:
				print_function(m.function);
				break;
			case ClassNode::Member::ENUM:
				print_enum(m.m_enum);
				break;
			case ClassNode::Member::UNDEFINED:
				push_line("<unknown member>");
				break;
		}
	}

	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_constant(ConstantNode *p_constant) {
	push_text("Constant ");
	print_identifier(p_constant->identifier);

	increase_indent();

	push_line();
	push_text("= ");
	if (p_constant->initializer == nullptr) {
		push_text("<missing value>");
	} else {
		print_expression(p_constant->initializer);
	}
	decrease_indent();
	push_line();
}

void GDScriptNewParser::TreePrinter::print_dictionary(DictionaryNode *p_dictionary) {
	push_line("{");
	increase_indent();
	for (int i = 0; i < p_dictionary->elements.size(); i++) {
		print_expression(p_dictionary->elements[i].key);
		push_text(" : ");
		print_expression(p_dictionary->elements[i].value);
		push_line(" ,");
	}
	decrease_indent();
	push_text("}");
}

void GDScriptNewParser::TreePrinter::print_expression(ExpressionNode *p_expression) {
	// Surround in parenthesis for disambiguation.
	push_text("(");
	switch (p_expression->type) {
		case Node::ARRAY:
			print_array(static_cast<ArrayNode *>(p_expression));
			break;
		case Node::ASSIGNMENT:
			print_assignment(static_cast<AssignmentNode *>(p_expression));
			break;
		case Node::ATTRIBUTE:
			print_attribute(static_cast<AttributeNode *>(p_expression));
			break;
		case Node::AWAIT:
			print_await(static_cast<AwaitNode *>(p_expression));
			break;
		case Node::BINARY_OPERATOR:
			print_binary_op(static_cast<BinaryOpNode *>(p_expression));
			break;
		case Node::CALL:
			print_call(static_cast<CallNode *>(p_expression));
			break;
		case Node::CAST:
			print_cast(static_cast<CastNode *>(p_expression));
			break;
		case Node::DICTIONARY:
			print_dictionary(static_cast<DictionaryNode *>(p_expression));
			break;
		case Node::GET_NODE:
			print_get_node(static_cast<GetNodeNode *>(p_expression));
			break;
		case Node::IDENTIFIER:
			print_identifier(static_cast<IdentifierNode *>(p_expression));
			break;
		case Node::LITERAL:
			print_literal(static_cast<LiteralNode *>(p_expression));
			break;
		case Node::SELF:
			print_self(static_cast<SelfNode *>(p_expression));
			break;
		case Node::SUBSCRIPT:
			print_subscript(static_cast<SubscriptNode *>(p_expression));
			break;
		case Node::TERNARY_OPERATOR:
			print_ternary_op(static_cast<TernaryOpNode *>(p_expression));
			break;
		case Node::UNARY_OPERATOR:
			print_unary_op(static_cast<UnaryOpNode *>(p_expression));
			break;
		default:
			push_text(vformat("<unknown expression %d>", p_expression->type));
			break;
	}
	// Surround in parenthesis for disambiguation.
	push_text(")");
}

void GDScriptNewParser::TreePrinter::print_enum(EnumNode *p_enum) {
	push_text("Enum ");
	if (p_enum->identifier != nullptr) {
		print_identifier(p_enum->identifier);
	} else {
		push_text("<unnamed>");
	}

	push_line(" {");
	increase_indent();
	int last_value = 0;
	for (int i = 0; i < p_enum->values.size(); i++) {
		const EnumNode::Value &item = p_enum->values[i];
		print_identifier(item.name);
		push_text(" = ");
		if (item.value != nullptr) {
			print_literal(item.value);
			last_value = item.value->value;
		} else {
			push_text(itos(last_value++));
		}
		push_line(" ,");
	}
	decrease_indent();
	push_line("}");
}

void GDScriptNewParser::TreePrinter::print_for(ForNode *p_for) {
	push_text("For ");
	print_identifier(p_for->variable);
	push_text(" IN ");
	print_expression(p_for->list);
	push_line(" :");

	increase_indent();

	print_suite(p_for->loop);

	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_function(FunctionNode *p_function) {
	for (const List<AnnotationNode *>::Element *E = p_function->annotations.front(); E != nullptr; E = E->next()) {
		print_annotation(E->get());
	}
	push_text("Function ");
	print_identifier(p_function->identifier);
	push_text("( ");
	for (int i = 0; i < p_function->parameters.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_parameter(p_function->parameters[i]);
	}
	push_line(" ) :");
	increase_indent();
	print_suite(p_function->body);
	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_get_node(GetNodeNode *p_get_node) {
	push_text("$");
	if (p_get_node->string != nullptr) {
		print_literal(p_get_node->string);
	} else {
		for (int i = 0; i < p_get_node->chain.size(); i++) {
			if (i > 0) {
				push_text("/");
			}
			print_identifier(p_get_node->chain[i]);
		}
	}
}

void GDScriptNewParser::TreePrinter::print_identifier(IdentifierNode *p_identifier) {
	push_text(p_identifier->name);
}

void GDScriptNewParser::TreePrinter::print_if(IfNode *p_if, bool p_is_elif) {
	if (p_is_elif) {
		push_text("Elif ");
	} else {
		push_text("If ");
	}
	print_expression(p_if->condition);
	push_line(" :");

	increase_indent();
	print_suite(p_if->true_block);
	decrease_indent();

	if (p_if->elif != nullptr) {
		print_if(p_if->elif, true);
	} else if (p_if->false_block != nullptr) {
		push_line("Else :");
		increase_indent();
		print_suite(p_if->false_block);
		decrease_indent();
	}
}

void GDScriptNewParser::TreePrinter::print_literal(LiteralNode *p_literal) {
	// Prefix for string types.
	switch (p_literal->value.get_type()) {
		case Variant::NODE_PATH:
			push_text("^\"");
			break;
		case Variant::STRING:
			push_text("\"");
			break;
		case Variant::STRING_NAME:
			push_text("&\"");
			break;
		default:
			break;
	}
	push_text(p_literal->value);
	// Suffix for string types.
	switch (p_literal->value.get_type()) {
		case Variant::NODE_PATH:
		case Variant::STRING:
		case Variant::STRING_NAME:
			push_text("\"");
			break;
		default:
			break;
	}
}

void GDScriptNewParser::TreePrinter::print_match(MatchNode *p_match) {
	push_text("Match ");
	print_expression(p_match->test);
	push_line(" :");

	increase_indent();
	for (int i = 0; i < p_match->branches.size(); i++) {
		print_match_branch(p_match->branches[i]);
	}
	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_match_branch(MatchBranchNode *p_match_branch) {
	for (int i = 0; i < p_match_branch->patterns.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_match_pattern(p_match_branch->patterns[i]);
	}

	push_line(" :");

	increase_indent();
	print_suite(p_match_branch->block);
	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_match_pattern(PatternNode *p_match_pattern) {
	switch (p_match_pattern->pattern_type) {
		case PatternNode::PT_LITERAL:
			print_literal(p_match_pattern->literal);
			break;
		case PatternNode::PT_WILDCARD:
			push_text("_");
			break;
		case PatternNode::PT_REST:
			push_text("..");
			break;
		case PatternNode::PT_BIND:
			push_text("Var ");
			print_identifier(p_match_pattern->bind);
			break;
		case PatternNode::PT_EXPRESSION:
			print_expression(p_match_pattern->expression);
			break;
		case PatternNode::PT_ARRAY:
			push_text("[ ");
			for (int i = 0; i < p_match_pattern->array.size(); i++) {
				if (i > 0) {
					push_text(" , ");
				}
				print_match_pattern(p_match_pattern->array[i]);
			}
			push_text(" ]");
			break;
		case PatternNode::PT_DICTIONARY:
			push_text("{ ");
			for (int i = 0; i < p_match_pattern->dictionary.size(); i++) {
				if (i > 0) {
					push_text(" , ");
				}
				if (p_match_pattern->dictionary[i].key != nullptr) {
					// Key can be null for rest pattern.
					print_literal(p_match_pattern->dictionary[i].key);
					push_text(" : ");
				}
				print_match_pattern(p_match_pattern->dictionary[i].value_pattern);
			}
			push_text(" }");
			break;
	}
}

void GDScriptNewParser::TreePrinter::print_parameter(ParameterNode *p_parameter) {
	print_identifier(p_parameter->identifier);
	if (p_parameter->datatype_specifier != nullptr) {
		push_text(" : ");
		print_type(p_parameter->datatype_specifier);
	}
	if (p_parameter->default_value != nullptr) {
		push_text(" = ");
		print_expression(p_parameter->default_value);
	}
}

void GDScriptNewParser::TreePrinter::print_return(ReturnNode *p_return) {
	push_text("Return");
	if (p_return->return_value != nullptr) {
		push_text(" ");
		print_expression(p_return->return_value);
	}
	push_line();
}

void GDScriptNewParser::TreePrinter::print_self(SelfNode *p_self) {
	push_text("Self(");
	if (p_self->current_class->identifier != nullptr) {
		print_identifier(p_self->current_class->identifier);
	} else {
		push_text("<main class>");
	}
	push_text(")");
}

void GDScriptNewParser::TreePrinter::print_signal(SignalNode *p_signal) {
	push_text("Signal ");
	print_identifier(p_signal->identifier);
	push_text("( ");
	for (int i = 0; i < p_signal->parameters.size(); i++) {
		print_parameter(p_signal->parameters[i]);
	}
	push_line(" )");
}

void GDScriptNewParser::TreePrinter::print_subscript(SubscriptNode *p_subscript) {
	print_expression(p_subscript->base);
	push_text("[ ");
	print_expression(p_subscript->index);
	push_text(" ]");
}

void GDScriptNewParser::TreePrinter::print_statement(Node *p_statement) {
	switch (p_statement->type) {
		case Node::ASSERT:
			print_assert(static_cast<AssertNode *>(p_statement));
			break;
		case Node::VARIABLE:
			print_variable(static_cast<VariableNode *>(p_statement));
			break;
		case Node::CONSTANT:
			print_constant(static_cast<ConstantNode *>(p_statement));
			break;
		case Node::IF:
			print_if(static_cast<IfNode *>(p_statement));
			break;
		case Node::FOR:
			print_for(static_cast<ForNode *>(p_statement));
			break;
		case Node::WHILE:
			print_while(static_cast<WhileNode *>(p_statement));
			break;
		case Node::MATCH:
			print_match(static_cast<MatchNode *>(p_statement));
			break;
		case Node::RETURN:
			print_return(static_cast<ReturnNode *>(p_statement));
			break;
		case Node::BREAK:
			push_line("Break");
			break;
		case Node::CONTINUE:
			push_line("Continue");
			break;
		case Node::PASS:
			push_line("Pass");
			break;
		case Node::BREAKPOINT:
			push_line("Breakpoint");
			break;
		case Node::ASSIGNMENT:
			print_assignment(static_cast<AssignmentNode *>(p_statement));
			break;
		default:
			if (p_statement->is_expression()) {
				print_expression(static_cast<ExpressionNode *>(p_statement));
				push_line();
			} else {
				push_line(vformat("<unknown statement %d>", p_statement->type));
			}
			break;
	}
}

void GDScriptNewParser::TreePrinter::print_suite(SuiteNode *p_suite) {
	for (int i = 0; i < p_suite->statements.size(); i++) {
		print_statement(p_suite->statements[i]);
	}
}

void GDScriptNewParser::TreePrinter::print_ternary_op(TernaryOpNode *p_ternary_op) {
	print_expression(p_ternary_op->true_expr);
	push_text("IF ");
	print_expression(p_ternary_op->condition);
	push_text(" ELSE ");
	print_expression(p_ternary_op->false_expr);
}

void GDScriptNewParser::TreePrinter::print_type(TypeNode *p_type) {
	if (p_type->type_specifier != nullptr) {
		print_attribute(p_type->type_specifier);
	} else if (p_type->type_base != nullptr) {
		print_identifier(p_type->type_base);
	} else {
		push_text("Void");
	}
}

void GDScriptNewParser::TreePrinter::print_unary_op(UnaryOpNode *p_unary_op) {
	switch (p_unary_op->operation) {
		case UnaryOpNode::OP_POSITIVE:
			push_text("+");
			break;
		case UnaryOpNode::OP_NEGATIVE:
			push_text("-");
			break;
		case UnaryOpNode::OP_LOGIC_NOT:
			push_text("NOT");
			break;
		case UnaryOpNode::OP_COMPLEMENT:
			push_text("~");
			break;
	}
	print_expression(p_unary_op->operand);
}

void GDScriptNewParser::TreePrinter::print_variable(VariableNode *p_variable) {
	for (const List<AnnotationNode *>::Element *E = p_variable->annotations.front(); E != nullptr; E = E->next()) {
		print_annotation(E->get());
	}

	push_text("Variable ");
	print_identifier(p_variable->identifier);

	increase_indent();

	push_line();
	push_text("= ");
	if (p_variable->initializer == nullptr) {
		push_text("<default value>");
	} else {
		print_expression(p_variable->initializer);
	}
	decrease_indent();
	push_line();
}

void GDScriptNewParser::TreePrinter::print_while(WhileNode *p_while) {
	push_text("While ");
	print_expression(p_while->condition);
	push_line(" :");

	increase_indent();
	print_suite(p_while->loop);
	decrease_indent();
}

void GDScriptNewParser::TreePrinter::print_tree(const GDScriptNewParser &p_parser) {
	ERR_FAIL_COND_MSG(p_parser.get_tree() == nullptr, "Parse the code before printing the parse tree.");

	if (p_parser.is_tool()) {
		push_line("@tool");
	}
	if (!p_parser.get_tree()->icon_path.empty()) {
		push_text(R"(@icon (")");
		push_text(p_parser.get_tree()->icon_path);
		push_line("\")");
	}
	print_class(p_parser.get_tree());

	print_line(printed);
}

#endif // DEBUG_ENABLED
