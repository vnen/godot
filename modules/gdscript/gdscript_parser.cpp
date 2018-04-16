/*************************************************************************/
/*  gdscript_parser.cpp                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "gdscript_parser.h"

#include "gdscript.h"
#include "io/resource_loader.h"
#include "os/file_access.h"
#include "print_string.h"
#include "script_language.h"

template <class T>
T *GDScriptParser::alloc_node() {

	T *t = memnew(T);

	t->next = list;
	list = t;

	if (!head)
		head = t;

	t->line = tokenizer->get_token_line();
	t->column = tokenizer->get_token_column();
	return t;
}

bool GDScriptParser::_end_statement() {

	if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON) {
		tokenizer->advance();
		return true; //handle next
	} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE || tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
		return true; //will be handled properly
	}

	return false;
}

bool GDScriptParser::_enter_indent_block(BlockNode *p_block) {

	if (tokenizer->get_token() != GDScriptTokenizer::TK_COLON) {
		// report location at the previous token (on the previous line)
		int error_line = tokenizer->get_token_line(-1);
		int error_column = tokenizer->get_token_column(-1);
		_set_error("':' expected at end of line.", error_line, error_column);
		return false;
	}
	tokenizer->advance();

	if (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE) {

		// be more python-like
		int current = tab_level.back()->get();
		tab_level.push_back(current);
		return true;
		//_set_error("newline expected after ':'.");
		//return false;
	}

	while (true) {

		if (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE) {

			return false; //wtf
		} else if (tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE) {

			int indent = tokenizer->get_token_line_indent();
			int current = tab_level.back()->get();
			if (indent <= current) {
				return false;
			}

			tab_level.push_back(indent);
			tokenizer->advance();
			return true;

		} else if (p_block) {

			NewLineNode *nl = alloc_node<NewLineNode>();
			nl->line = tokenizer->get_token_line();
			p_block->statements.push_back(nl);
		}

		tokenizer->advance(); // go to next newline
	}
}

bool GDScriptParser::_parse_arguments(Node *p_parent, Vector<Node *> &p_args, bool p_static, bool p_can_codecomplete) {

	if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
		tokenizer->advance();
	} else {

		parenthesis++;
		int argidx = 0;

		while (true) {

			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				_make_completable_call(argidx);
				completion_node = p_parent;
			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING && tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR) {
				//completing a string argument..
				completion_cursor = tokenizer->get_token_constant();

				_make_completable_call(argidx);
				completion_node = p_parent;
				tokenizer->advance(1);
				return false;
			}

			Node *arg = _parse_expression(p_parent, p_static);
			if (!arg)
				return false;

			p_args.push_back(arg);

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				tokenizer->advance();
				break;

			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {

				if (tokenizer->get_token(1) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {

					_set_error("Expression expected");
					return false;
				}

				tokenizer->advance();
				argidx++;
			} else {
				// something is broken
				_set_error("Expected ',' or ')'");
				return false;
			}
		}
		parenthesis--;
	}

	return true;
}

void GDScriptParser::_make_completable_call(int p_arg) {

	completion_cursor = StringName();
	completion_type = COMPLETION_CALL_ARGUMENTS;
	completion_class = current_class;
	completion_function = current_function;
	completion_line = tokenizer->get_token_line();
	completion_argument = p_arg;
	completion_block = current_block;
	completion_found = true;
	tokenizer->advance();
}

bool GDScriptParser::_get_completable_identifier(CompletionType p_type, StringName &identifier) {

	identifier = StringName();
	if (tokenizer->is_token_literal()) {
		identifier = tokenizer->get_token_literal();
		tokenizer->advance();
	}
	if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {

		completion_cursor = identifier;
		completion_type = p_type;
		completion_class = current_class;
		completion_function = current_function;
		completion_line = tokenizer->get_token_line();
		completion_block = current_block;
		completion_found = true;
		completion_ident_is_call = false;
		tokenizer->advance();

		if (tokenizer->is_token_literal()) {
			identifier = identifier.operator String() + tokenizer->get_token_literal().operator String();
			tokenizer->advance();
		}

		if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
			completion_ident_is_call = true;
		}
		return true;
	}

	return false;
}

GDScriptParser::Node *GDScriptParser::_parse_expression(Node *p_parent, bool p_static, bool p_allow_assign, bool p_parsing_constant) {

	//Vector<Node*> expressions;
	//Vector<OperatorNode::Operator> operators;

	Vector<Expression> expression;

	Node *expr = NULL;

	int op_line = tokenizer->get_token_line(); // when operators are created at the bottom, the line might have been changed (\n found)

	while (true) {

		/*****************/
		/* Parse Operand */
		/*****************/

		if (parenthesis > 0) {
			//remove empty space (only allowed if inside parenthesis
			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}
		}

		if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
			//subexpression ()
			tokenizer->advance();
			parenthesis++;
			Node *subexpr = _parse_expression(p_parent, p_static, p_allow_assign, p_parsing_constant);
			parenthesis--;
			if (!subexpr)
				return NULL;

			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {

				_set_error("Expected ')' in expression");
				return NULL;
			}

			tokenizer->advance();
			expr = subexpr;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_DOLLAR) {
			tokenizer->advance();

			String path;

			bool need_identifier = true;
			bool done = false;

			while (!done) {

				switch (tokenizer->get_token()) {
					case GDScriptTokenizer::TK_CURSOR: {
						completion_cursor = StringName();
						completion_type = COMPLETION_GET_NODE;
						completion_class = current_class;
						completion_function = current_function;
						completion_line = tokenizer->get_token_line();
						completion_cursor = path;
						completion_argument = 0;
						completion_block = current_block;
						completion_found = true;
						tokenizer->advance();
					} break;
					case GDScriptTokenizer::TK_CONSTANT: {

						if (!need_identifier) {
							done = true;
							break;
						}

						if (tokenizer->get_token_constant().get_type() != Variant::STRING) {
							_set_error("Expected string constant or identifier after '$' or '/'.");
							return NULL;
						}

						path += String(tokenizer->get_token_constant());
						tokenizer->advance();
						need_identifier = false;

					} break;
					case GDScriptTokenizer::TK_OP_DIV: {

						if (need_identifier) {
							done = true;
							break;
						}

						path += "/";
						tokenizer->advance();
						need_identifier = true;

					} break;
					default: {
						// Instead of checking for TK_IDENTIFIER, we check with is_token_literal, as this allows us to use match/sync/etc. as a name
						if (need_identifier && tokenizer->is_token_literal()) {
							path += String(tokenizer->get_token_literal());
							tokenizer->advance();
							need_identifier = false;
						} else {
							done = true;
						}

						break;
					}
				}
			}

			if (path == "") {
				_set_error("Path expected after $.");
				return NULL;
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_CALL;

			// Set expression type
			op->return_type.has_type = true;
			op->return_type.variant_type = Variant::OBJECT;
			op->return_type.class_name = StringName("Node");

			op->arguments.push_back(alloc_node<SelfNode>());

			IdentifierNode *funcname = alloc_node<IdentifierNode>();
			funcname->name = "get_node";

			op->arguments.push_back(funcname);

			ConstantNode *nodepath = alloc_node<ConstantNode>();
			nodepath->value = NodePath(StringName(path));
			op->arguments.push_back(nodepath);

			expr = op;

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
			tokenizer->advance();
			continue; //no point in cursor in the middle of expression

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT) {

			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = tokenizer->get_token_constant();
			constant->constant_type.variant_type = constant->value.get_type();
			constant->constant_type.has_type = true;
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_PI) {

			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_PI;
			constant->constant_type.variant_type = Variant::REAL;
			constant->constant_type.has_type = true;
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_TAU) {

			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_TAU;
			constant->constant_type.variant_type = Variant::REAL;
			constant->constant_type.has_type = true;
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_INF) {

			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_INF;
			constant->constant_type.variant_type = Variant::REAL;
			constant->constant_type.has_type = true;
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_NAN) {

			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_NAN;
			constant->constant_type.variant_type = Variant::REAL;
			constant->constant_type.has_type = true;
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_PRELOAD) {

			//constant defined by tokenizer
			tokenizer->advance();
			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
				_set_error("Expected '(' after 'preload'");
				return NULL;
			}
			tokenizer->advance();

			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				completion_cursor = StringName();
				completion_node = p_parent;
				completion_type = COMPLETION_RESOURCE_PATH;
				completion_class = current_class;
				completion_function = current_function;
				completion_line = tokenizer->get_token_line();
				completion_block = current_block;
				completion_argument = 0;
				completion_found = true;
				tokenizer->advance();
			}

			String path;
			bool found_constant = false;
			bool valid = false;
			ConstantNode *cn;

			Node *subexpr = _parse_and_reduce_expression(p_parent, p_static);
			if (subexpr) {
				if (subexpr->type == Node::TYPE_CONSTANT) {
					cn = static_cast<ConstantNode *>(subexpr);
					found_constant = true;
				}
				if (subexpr->type == Node::TYPE_IDENTIFIER) {
					IdentifierNode *in = static_cast<IdentifierNode *>(subexpr);
					Vector<ClassNode::Constant> ce = current_class->constant_expressions;

					// Try to find the constant expression by the identifier
					for (int i = 0; i < ce.size(); ++i) {
						if (ce[i].identifier == in->name) {
							if (ce[i].expression->type == Node::TYPE_CONSTANT) {
								cn = static_cast<ConstantNode *>(ce[i].expression);
								found_constant = true;
							}
						}
					}
				}

				if (found_constant && cn->value.get_type() == Variant::STRING) {
					valid = true;
					path = (String)cn->value;
				}
			}

			if (!valid) {
				_set_error("expected string constant as 'preload' argument.");
				return NULL;
			}

			if (!path.is_abs_path() && base_path != "")
				path = base_path + "/" + path;
			path = path.replace("///", "//").simplify_path();
			if (path == self_path) {

				_set_error("Can't preload itself (use 'get_script()').");
				return NULL;
			}

			Ref<Resource> res;
			if (!validating) {

				//this can be too slow for just validating code
				if (for_completion && ScriptCodeCompletionCache::get_singleton() && FileAccess::exists(path)) {
					res = ScriptCodeCompletionCache::get_singleton()->get_cached_resource(path);
				} else if (!for_completion || FileAccess::exists(path)) {
					res = ResourceLoader::load(path);
				}
			} else {

				if (!FileAccess::exists(path)) {
					_set_error("Can't preload resource at path: " + path);
					return NULL;
				} else if (ScriptCodeCompletionCache::get_singleton()) {
					res = ScriptCodeCompletionCache::get_singleton()->get_cached_resource(path);
				}
			}

			if (!res.is_valid()) {
				_set_error("Can't preload resource at path: " + path);
				return NULL;
			}

			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				_set_error("Expected ')' after 'preload' path");
				return NULL;
			}
			tokenizer->advance();

			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = res;
			constant->constant_type.has_type = true;
			if (res.is_null()) {
				// No resource loaded, no way to infer the type
				constant->constant_type.variant_type = Variant::NIL;
			} else {
				constant->constant_type.variant_type = Variant::OBJECT;
				if (res->get_class_name() == "GDScript") {
					// If it's a GDScript, use the path as class name
					constant->constant_type.class_name = path;
				} else if (!res->get_script().is_null()) {
					// If the resource contains a script, use that path instead
					Ref<Script> script = res->get_script();
					constant->constant_type.class_name = script->get_path();
				} else {
					// Otherwise use the resource base type
					constant->constant_type.class_name = res->get_class_name();
				}
			}

			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_YIELD) {

			//constant defined by tokenizer

			tokenizer->advance();
			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
				_set_error("Expected '(' after 'yield'");
				return NULL;
			}

			tokenizer->advance();

			OperatorNode *yield = alloc_node<OperatorNode>();
			yield->op = OperatorNode::OP_YIELD;
			// Yield should always return a GDScriptFunctionState
			yield->return_type.has_type = true;
			yield->return_type.variant_type = Variant::OBJECT;
			yield->return_type.class_name = "GDScriptFunctionState";

			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				expr = yield;
				tokenizer->advance();
			} else {

				parenthesis++;

				Node *object = _parse_and_reduce_expression(p_parent, p_static);
				if (!object)
					return NULL;
				yield->arguments.push_back(object);

				if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
					_set_error("Expected ',' after first argument of 'yield'");
					return NULL;
				}

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {

					completion_cursor = StringName();
					completion_node = object;
					completion_type = COMPLETION_YIELD;
					completion_class = current_class;
					completion_function = current_function;
					completion_line = tokenizer->get_token_line();
					completion_argument = 0;
					completion_block = current_block;
					completion_found = true;
					tokenizer->advance();
				}

				Node *signal = _parse_and_reduce_expression(p_parent, p_static);
				if (!signal)
					return NULL;
				yield->arguments.push_back(signal);

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
					_set_error("Expected ')' after second argument of 'yield'");
					return NULL;
				}

				parenthesis--;

				tokenizer->advance();

				expr = yield;
			}

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_SELF) {

			if (p_static) {
				_set_error("'self'' not allowed in static function or constant expression");
				return NULL;
			}
			//constant defined by tokenizer
			SelfNode *self = alloc_node<SelfNode>();
			tokenizer->advance();
			expr = self;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {

			Variant::Type bi_type = tokenizer->get_token_type();
			tokenizer->advance(2);

			StringName identifier;

			if (_get_completable_identifier(COMPLETION_BUILT_IN_TYPE_CONSTANT, identifier)) {

				completion_built_in_constant = bi_type;
			}

			if (identifier == StringName()) {

				_set_error("Built-in type constant or static function expected after '.'");
				return NULL;
			}
			if (!Variant::has_numeric_constant(bi_type, identifier)) {

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN &&
						Variant::is_method_const(bi_type, identifier) &&
						Variant::get_method_return_type(bi_type, identifier) == bi_type) {

					tokenizer->advance();

					OperatorNode *construct = alloc_node<OperatorNode>();
					construct->op = OperatorNode::OP_CALL;

					TypeNode *tn = alloc_node<TypeNode>();
					tn->vtype = bi_type;
					construct->arguments.push_back(tn);

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_CALL;
					op->arguments.push_back(construct);

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = identifier;
					op->arguments.push_back(id);

					if (!_parse_arguments(op, op->arguments, p_static, true))
						return NULL;

					expr = op;
				} else {

					_set_error("Static constant  '" + identifier.operator String() + "' not present in built-in type " + Variant::get_type_name(bi_type) + ".");
					return NULL;
				}
			} else {

				ConstantNode *cn = alloc_node<ConstantNode>();
				cn->value = Variant::get_numeric_constant_value(bi_type, identifier);
				cn->constant_type.has_type = true;
				cn->constant_type.variant_type = Variant::INT;
				expr = cn;
			}

		} else if (tokenizer->get_token(1) == GDScriptTokenizer::TK_PARENTHESIS_OPEN && tokenizer->is_token_literal()) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			//function or constructor

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_CALL;

			//Do a quick Array and Dictionary Check.  Replace if either require no arguments.
			bool replaced = false;

			if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE) {
				Variant::Type ct = tokenizer->get_token_type();
				if (p_parsing_constant == false) {
					if (ct == Variant::ARRAY) {
						if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							ArrayNode *arr = alloc_node<ArrayNode>();
							expr = arr;
							replaced = true;
							tokenizer->advance(3);
						}
					}
					if (ct == Variant::DICTIONARY) {
						if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							DictionaryNode *dict = alloc_node<DictionaryNode>();
							expr = dict;
							replaced = true;
							tokenizer->advance(3);
						}
					}
				}

				if (!replaced) {
					TypeNode *tn = alloc_node<TypeNode>();
					tn->vtype = tokenizer->get_token_type();
					op->arguments.push_back(tn);
					tokenizer->advance(2);
				}
			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_FUNC) {

				BuiltInFunctionNode *bn = alloc_node<BuiltInFunctionNode>();
				bn->function = tokenizer->get_token_built_in_func();
				op->arguments.push_back(bn);
				tokenizer->advance(2);
			} else {

				SelfNode *self = alloc_node<SelfNode>();
				op->arguments.push_back(self);

				StringName identifier;
				if (_get_completable_identifier(COMPLETION_FUNCTION, identifier)) {
				}

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = identifier;
				op->arguments.push_back(id);
				tokenizer->advance(1);
			}

			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				_make_completable_call(0);
				completion_node = op;
			}
			if (!replaced) {
				if (!_parse_arguments(op, op->arguments, p_static, true))
					return NULL;
				expr = op;
			}
		} else if (tokenizer->is_token_literal(0, true)) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			//identifier (reference)

			const ClassNode *cln = current_class;
			bool bfn = false;
			StringName identifier;
			if (_get_completable_identifier(COMPLETION_IDENTIFIER, identifier)) {
			}

			if (p_parsing_constant) {
				for (int i = 0; i < cln->constant_expressions.size(); ++i) {

					if (cln->constant_expressions[i].identifier == identifier) {

						expr = cln->constant_expressions[i].expression;
						bfn = true;
						break;
					}
				}

				if (GDScriptLanguage::get_singleton()->get_global_map().has(identifier)) {
					//check from constants
					ConstantNode *constant = alloc_node<ConstantNode>();
					constant->value = GDScriptLanguage::get_singleton()->get_global_array()[GDScriptLanguage::get_singleton()->get_global_map()[identifier]];
					constant->constant_type.has_type = true;
					constant->constant_type.variant_type = constant->value.get_type();
					if (constant->value.get_type() == Variant::OBJECT) {
						constant->constant_type.class_name = constant->value.operator Object *()->get_class_name();
					}
					expr = constant;
					bfn = true;
				}
			}

			if (!bfn) {
				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = identifier;

				expr = id;
			}

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ADD || tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB || tokenizer->get_token() == GDScriptTokenizer::TK_OP_NOT || tokenizer->get_token() == GDScriptTokenizer::TK_OP_BIT_INVERT) {

			//single prefix operators like !expr +expr -expr ++expr --expr
			alloc_node<OperatorNode>();
			Expression e;
			e.is_op = true;

			switch (tokenizer->get_token()) {
				case GDScriptTokenizer::TK_OP_ADD: e.op = OperatorNode::OP_POS; break;
				case GDScriptTokenizer::TK_OP_SUB: e.op = OperatorNode::OP_NEG; break;
				case GDScriptTokenizer::TK_OP_NOT: e.op = OperatorNode::OP_NOT; break;
				case GDScriptTokenizer::TK_OP_BIT_INVERT: e.op = OperatorNode::OP_BIT_INVERT; break;
				default: {}
			}

			tokenizer->advance();

			if (e.op != OperatorNode::OP_NOT && tokenizer->get_token() == GDScriptTokenizer::TK_OP_NOT) {
				_set_error("Misplaced 'not'.");
				return NULL;
			}

			expression.push_back(e);
			continue; //only exception, must continue...

			/*
			Node *subexpr=_parse_expression(op,p_static);
			if (!subexpr)
				return NULL;
			op->arguments.push_back(subexpr);
			expr=op;*/

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_OPEN) {
			// array
			tokenizer->advance();

			ArrayNode *arr = alloc_node<ArrayNode>();
			bool expecting_comma = false;

			while (true) {

				if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {

					_set_error("Unterminated array");
					return NULL;

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {

					tokenizer->advance(); //ignore newline
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					if (!expecting_comma) {
						_set_error("expression or ']' expected");
						return NULL;
					}

					expecting_comma = false;
					tokenizer->advance(); //ignore newline
				} else {
					//parse expression
					if (expecting_comma) {
						_set_error("',' or ']' expected");
						return NULL;
					}
					Node *n = _parse_expression(arr, p_static, p_allow_assign, p_parsing_constant);
					if (!n)
						return NULL;
					arr->elements.push_back(n);
					expecting_comma = true;
				}
			}

			expr = arr;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_OPEN) {
			// array
			tokenizer->advance();

			DictionaryNode *dict = alloc_node<DictionaryNode>();

			enum DictExpect {

				DICT_EXPECT_KEY,
				DICT_EXPECT_COLON,
				DICT_EXPECT_VALUE,
				DICT_EXPECT_COMMA

			};

			Node *key = NULL;
			Set<Variant> keys;

			DictExpect expecting = DICT_EXPECT_KEY;

			while (true) {

				if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {

					_set_error("Unterminated dictionary");
					return NULL;

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {

					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return NULL;
					}
					tokenizer->advance();
					break;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {

					tokenizer->advance(); //ignore newline
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {

					if (expecting == DICT_EXPECT_KEY) {
						_set_error("key or '}' expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return NULL;
					}

					expecting = DICT_EXPECT_KEY;
					tokenizer->advance(); //ignore newline

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {

					if (expecting == DICT_EXPECT_KEY) {
						_set_error("key or '}' expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_COMMA) {
						_set_error("',' or '}' expected");
						return NULL;
					}

					expecting = DICT_EXPECT_VALUE;
					tokenizer->advance(); //ignore newline
				} else {

					if (expecting == DICT_EXPECT_COMMA) {
						_set_error("',' or '}' expected");
						return NULL;
					}
					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return NULL;
					}

					if (expecting == DICT_EXPECT_KEY) {

						if (tokenizer->is_token_literal() && tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
							// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
							//lua style identifier, easier to write
							ConstantNode *cn = alloc_node<ConstantNode>();
							cn->value = tokenizer->get_token_literal();
							key = cn;
							tokenizer->advance(2);
							expecting = DICT_EXPECT_VALUE;
						} else {
							//python/js style more flexible
							key = _parse_expression(dict, p_static, p_allow_assign, p_parsing_constant);
							if (!key)
								return NULL;
							expecting = DICT_EXPECT_COLON;
						}
					}

					if (expecting == DICT_EXPECT_VALUE) {
						Node *value = _parse_expression(dict, p_static, p_allow_assign, p_parsing_constant);
						if (!value)
							return NULL;
						expecting = DICT_EXPECT_COMMA;

						if (key->type == GDScriptParser::Node::TYPE_CONSTANT) {
							Variant const &keyName = static_cast<const GDScriptParser::ConstantNode *>(key)->value;

							if (keys.has(keyName)) {
								_set_error("Duplicate key found in Dictionary literal");
								return NULL;
							}
							keys.insert(keyName);
						}

						DictionaryNode::Pair pair;
						pair.key = key;
						pair.value = value;
						dict->elements.push_back(pair);
						key = NULL;
					}
				}
			}

			expr = dict;

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && (tokenizer->is_token_literal(1) || tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR) && tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			// parent call

			tokenizer->advance(); //goto identifier
			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_PARENT_CALL;

			/*SelfNode *self = alloc_node<SelfNode>();
			op->arguments.push_back(self);
			forbidden for now */
			StringName identifier;
			if (_get_completable_identifier(COMPLETION_PARENT_FUNCTION, identifier)) {
				//indexing stuff
			}

			IdentifierNode *id = alloc_node<IdentifierNode>();
			id->name = identifier;
			op->arguments.push_back(id);

			tokenizer->advance(1);
			if (!_parse_arguments(op, op->arguments, p_static))
				return NULL;

			expr = op;

		} else {

			//find list [ or find dictionary {

			//print_line("found bug?");

			_set_error("Error parsing expression, misplaced: " + String(tokenizer->get_token_name(tokenizer->get_token())));
			return NULL; //nothing
		}

		if (!expr) {
			ERR_EXPLAIN("GDScriptParser bug, couldn't figure out what expression is..");
			ERR_FAIL_COND_V(!expr, NULL);
		}

		/******************/
		/* Parse Indexing */
		/******************/

		while (true) {

			//expressions can be indexed any number of times

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {

				//indexing using "."

				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_CURSOR && !tokenizer->is_token_literal(1)) {
					// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
					_set_error("Expected identifier as member");
					return NULL;
				} else if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					//call!!
					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_CALL;

					tokenizer->advance();

					IdentifierNode *id = alloc_node<IdentifierNode>();
					if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_FUNC) {
						//small hack so built in funcs don't obfuscate methods

						id->name = GDScriptFunctions::get_func_name(tokenizer->get_token_built_in_func());
						tokenizer->advance();

					} else {
						StringName identifier;
						if (_get_completable_identifier(COMPLETION_METHOD, identifier)) {
							completion_node = op;
							//indexing stuff
						}

						id->name = identifier;
					}

					op->arguments.push_back(expr); // call what
					op->arguments.push_back(id); // call func
					//get arguments
					tokenizer->advance(1);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
						_make_completable_call(0);
						completion_node = op;
					}
					if (!_parse_arguments(op, op->arguments, p_static, true))
						return NULL;
					expr = op;

				} else {
					//simple indexing!

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_INDEX_NAMED;
					tokenizer->advance();

					StringName identifier;
					if (_get_completable_identifier(COMPLETION_INDEX, identifier)) {

						if (identifier == StringName()) {
							identifier = "@temp"; //so it parses allright
						}
						completion_node = op;

						//indexing stuff
					}

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = identifier;

					op->arguments.push_back(expr);
					op->arguments.push_back(id);

					expr = op;
				}

			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_OPEN) {
				//indexing using "[]"
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = OperatorNode::OP_INDEX;

				tokenizer->advance(1);

				Node *subexpr = _parse_expression(op, p_static, p_allow_assign, p_parsing_constant);
				if (!subexpr) {
					return NULL;
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_BRACKET_CLOSE) {
					_set_error("Expected ']'");
					return NULL;
				}

				op->arguments.push_back(expr);
				op->arguments.push_back(subexpr);
				tokenizer->advance(1);
				expr = op;

			} else
				break;
		}

		/******************/
		/* Parse Operator */
		/******************/

		if (parenthesis > 0) {
			//remove empty space (only allowed if inside parenthesis
			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}
		}

		Expression e;
		e.is_op = false;
		e.node = expr;
		expression.push_back(e);

		// determine which operator is next

		OperatorNode::Operator op;
		bool valid = true;

//assign, if allowed is only allowed on the first operator
#define _VALIDATE_ASSIGN                  \
	if (!p_allow_assign) {                \
		_set_error("Unexpected assign."); \
		return NULL;                      \
	}                                     \
	p_allow_assign = false;

		switch (tokenizer->get_token()) { //see operator

			case GDScriptTokenizer::TK_OP_IN: op = OperatorNode::OP_IN; break;
			case GDScriptTokenizer::TK_OP_EQUAL: op = OperatorNode::OP_EQUAL; break;
			case GDScriptTokenizer::TK_OP_NOT_EQUAL: op = OperatorNode::OP_NOT_EQUAL; break;
			case GDScriptTokenizer::TK_OP_LESS: op = OperatorNode::OP_LESS; break;
			case GDScriptTokenizer::TK_OP_LESS_EQUAL: op = OperatorNode::OP_LESS_EQUAL; break;
			case GDScriptTokenizer::TK_OP_GREATER: op = OperatorNode::OP_GREATER; break;
			case GDScriptTokenizer::TK_OP_GREATER_EQUAL: op = OperatorNode::OP_GREATER_EQUAL; break;
			case GDScriptTokenizer::TK_OP_AND: op = OperatorNode::OP_AND; break;
			case GDScriptTokenizer::TK_OP_OR: op = OperatorNode::OP_OR; break;
			case GDScriptTokenizer::TK_OP_ADD: op = OperatorNode::OP_ADD; break;
			case GDScriptTokenizer::TK_OP_SUB: op = OperatorNode::OP_SUB; break;
			case GDScriptTokenizer::TK_OP_MUL: op = OperatorNode::OP_MUL; break;
			case GDScriptTokenizer::TK_OP_DIV: op = OperatorNode::OP_DIV; break;
			case GDScriptTokenizer::TK_OP_MOD:
				op = OperatorNode::OP_MOD;
				break;
			//case GDScriptTokenizer::TK_OP_NEG: op=OperatorNode::OP_NEG ; break;
			case GDScriptTokenizer::TK_OP_SHIFT_LEFT: op = OperatorNode::OP_SHIFT_LEFT; break;
			case GDScriptTokenizer::TK_OP_SHIFT_RIGHT: op = OperatorNode::OP_SHIFT_RIGHT; break;
			case GDScriptTokenizer::TK_OP_ASSIGN: {
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN;

				if (tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR) {
					//code complete assignment
					completion_type = COMPLETION_ASSIGN;
					completion_node = expr;
					completion_class = current_class;
					completion_function = current_function;
					completion_line = tokenizer->get_token_line();
					completion_block = current_block;
					completion_found = true;
					tokenizer->advance();
				}

			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_ADD: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_ADD; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SUB: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SUB; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MUL: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_MUL; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_DIV: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_DIV; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MOD: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_MOD; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_LEFT: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SHIFT_LEFT; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_RIGHT: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SHIFT_RIGHT; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_AND: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_AND; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_OR: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_OR; break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_XOR: _VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_XOR; break;
			case GDScriptTokenizer::TK_OP_BIT_AND: op = OperatorNode::OP_BIT_AND; break;
			case GDScriptTokenizer::TK_OP_BIT_OR: op = OperatorNode::OP_BIT_OR; break;
			case GDScriptTokenizer::TK_OP_BIT_XOR: op = OperatorNode::OP_BIT_XOR; break;
			case GDScriptTokenizer::TK_PR_IS: op = OperatorNode::OP_IS; break;
			case GDScriptTokenizer::TK_CF_IF: op = OperatorNode::OP_TERNARY_IF; break;
			case GDScriptTokenizer::TK_CF_ELSE: op = OperatorNode::OP_TERNARY_ELSE; break;
			default: valid = false; break;
		}

		if (valid) {
			e.is_op = true;
			e.op = op;
			expression.push_back(e);
			tokenizer->advance();
		} else {
			break;
		}
	}

	/* Reduce the set set of expressions and place them in an operator tree, respecting precedence */

	while (expression.size() > 1) {

		int next_op = -1;
		int min_priority = 0xFFFFF;
		bool is_unary = false;
		bool is_ternary = false;

		for (int i = 0; i < expression.size(); i++) {

			if (!expression[i].is_op) {

				continue;
			}

			int priority;

			bool unary = false;
			bool ternary = false;
			bool error = false;
			bool right_to_left = false;

			switch (expression[i].op) {

				case OperatorNode::OP_IS:
					priority = -1;
					break; //before anything

				case OperatorNode::OP_BIT_INVERT:
					priority = 0;
					unary = true;
					break;
				case OperatorNode::OP_NEG:
					priority = 1;
					unary = true;
					break;
				case OperatorNode::OP_POS:
					priority = 1;
					unary = true;
					break;

				case OperatorNode::OP_MUL: priority = 2; break;
				case OperatorNode::OP_DIV: priority = 2; break;
				case OperatorNode::OP_MOD: priority = 2; break;

				case OperatorNode::OP_ADD: priority = 3; break;
				case OperatorNode::OP_SUB: priority = 3; break;

				case OperatorNode::OP_SHIFT_LEFT: priority = 4; break;
				case OperatorNode::OP_SHIFT_RIGHT: priority = 4; break;

				case OperatorNode::OP_BIT_AND: priority = 5; break;
				case OperatorNode::OP_BIT_XOR: priority = 6; break;
				case OperatorNode::OP_BIT_OR: priority = 7; break;

				case OperatorNode::OP_LESS: priority = 8; break;
				case OperatorNode::OP_LESS_EQUAL: priority = 8; break;
				case OperatorNode::OP_GREATER: priority = 8; break;
				case OperatorNode::OP_GREATER_EQUAL: priority = 8; break;

				case OperatorNode::OP_EQUAL: priority = 8; break;
				case OperatorNode::OP_NOT_EQUAL: priority = 8; break;

				case OperatorNode::OP_IN: priority = 10; break;

				case OperatorNode::OP_NOT:
					priority = 11;
					unary = true;
					break;
				case OperatorNode::OP_AND: priority = 12; break;
				case OperatorNode::OP_OR: priority = 13; break;

				case OperatorNode::OP_TERNARY_IF:
					priority = 14;
					ternary = true;
					right_to_left = true;
					break;
				case OperatorNode::OP_TERNARY_ELSE:
					priority = 14;
					error = true;
					// Rigth-to-left should be false in this case, otherwise it would always error.
					break;

				case OperatorNode::OP_ASSIGN: priority = 15; break;
				case OperatorNode::OP_ASSIGN_ADD: priority = 15; break;
				case OperatorNode::OP_ASSIGN_SUB: priority = 15; break;
				case OperatorNode::OP_ASSIGN_MUL: priority = 15; break;
				case OperatorNode::OP_ASSIGN_DIV: priority = 15; break;
				case OperatorNode::OP_ASSIGN_MOD: priority = 15; break;
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT: priority = 15; break;
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT: priority = 15; break;
				case OperatorNode::OP_ASSIGN_BIT_AND: priority = 15; break;
				case OperatorNode::OP_ASSIGN_BIT_OR: priority = 15; break;
				case OperatorNode::OP_ASSIGN_BIT_XOR: priority = 15; break;

				default: {
					_set_error("GDScriptParser bug, invalid operator in expression: " + itos(expression[i].op));
					return NULL;
				}
			}

			if (priority < min_priority || (right_to_left && priority == min_priority)) {
				// < is used for left to right (default)
				// <= is used for right to left
				if (error) {
					_set_error("Unexpected operator");
					return NULL;
				}
				next_op = i;
				min_priority = priority;
				is_unary = unary;
				is_ternary = ternary;
			}
		}

		if (next_op == -1) {

			_set_error("Yet another parser bug....");
			ERR_FAIL_COND_V(next_op == -1, NULL);
		}

		// OK! create operator..
		if (is_unary) {

			int expr_pos = next_op;
			while (expression[expr_pos].is_op) {

				expr_pos++;
				if (expr_pos == expression.size()) {
					//can happen..
					_set_error("Unexpected end of expression..");
					return NULL;
				}
			}

			//consecutively do unary opeators
			for (int i = expr_pos - 1; i >= next_op; i--) {

				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = expression[i].op;
				op->arguments.push_back(expression[i + 1].node);
				op->line = op_line; //line might have been changed from a \n
				expression[i].is_op = false;
				expression[i].node = op;
				expression.remove(i + 1);
			}

		} else if (is_ternary) {
			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_error("Parser bug..");
				ERR_FAIL_V(NULL);
			}

			if (next_op >= (expression.size() - 2) || expression[next_op + 2].op != OperatorNode::OP_TERNARY_ELSE) {
				_set_error("Expected else after ternary if.");
				ERR_FAIL_V(NULL);
			}
			if (next_op >= (expression.size() - 3)) {
				_set_error("Expected value after ternary else.");
				ERR_FAIL_V(NULL);
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;
			op->line = op_line; //line might have been changed from a \n

			if (expression[next_op - 1].is_op) {

				_set_error("Parser bug..");
				ERR_FAIL_V(NULL);
			}

			if (expression[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators after ternary if.");
				return NULL;
			}

			if (expression[next_op + 3].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators after ternary else.");
				return NULL;
			}

			op->arguments.push_back(expression[next_op + 1].node); //next expression goes as first
			op->arguments.push_back(expression[next_op - 1].node); //left expression goes as when-true
			op->arguments.push_back(expression[next_op + 3].node); //expression after next goes as when-false

			//replace all 3 nodes by this operator and make it an expression
			expression[next_op - 1].node = op;
			expression.remove(next_op);
			expression.remove(next_op);
			expression.remove(next_op);
			expression.remove(next_op);
		} else {

			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_error("Parser bug..");
				ERR_FAIL_V(NULL);
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;
			op->line = op_line; //line might have been changed from a \n

			if (expression[next_op - 1].is_op) {

				_set_error("Parser bug..");
				ERR_FAIL_V(NULL);
			}

			if (expression[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators.");
				return NULL;
			}

			op->arguments.push_back(expression[next_op - 1].node); //expression goes as left
			op->arguments.push_back(expression[next_op + 1].node); //next expression goes as right

			//replace all 3 nodes by this operator and make it an expression
			expression[next_op - 1].node = op;
			expression.remove(next_op);
			expression.remove(next_op);
		}
	}

	return expression[0].node;
}

GDScriptParser::Node *GDScriptParser::_reduce_expression(Node *p_node, bool p_to_const) {

	switch (p_node->type) {

		case Node::TYPE_BUILT_IN_FUNCTION: {
			//many may probably be optimizable
			return p_node;
		} break;
		case Node::TYPE_ARRAY: {

			ArrayNode *an = static_cast<ArrayNode *>(p_node);
			bool all_constants = true;

			for (int i = 0; i < an->elements.size(); i++) {

				an->elements[i] = _reduce_expression(an->elements[i], p_to_const);
				if (an->elements[i]->type != Node::TYPE_CONSTANT)
					all_constants = false;
			}

			if (all_constants && p_to_const) {
				//reduce constant array expression

				ConstantNode *cn = alloc_node<ConstantNode>();
				Array arr;
				//print_line("mk array "+itos(!p_to_const));
				arr.resize(an->elements.size());
				for (int i = 0; i < an->elements.size(); i++) {
					ConstantNode *acn = static_cast<ConstantNode *>(an->elements[i]);
					arr[i] = acn->value;
				}
				cn->value = arr;
				return cn;
			}

			return an;

		} break;
		case Node::TYPE_DICTIONARY: {

			DictionaryNode *dn = static_cast<DictionaryNode *>(p_node);
			bool all_constants = true;

			for (int i = 0; i < dn->elements.size(); i++) {

				dn->elements[i].key = _reduce_expression(dn->elements[i].key, p_to_const);
				if (dn->elements[i].key->type != Node::TYPE_CONSTANT)
					all_constants = false;
				dn->elements[i].value = _reduce_expression(dn->elements[i].value, p_to_const);
				if (dn->elements[i].value->type != Node::TYPE_CONSTANT)
					all_constants = false;
			}

			if (all_constants && p_to_const) {
				//reduce constant array expression

				ConstantNode *cn = alloc_node<ConstantNode>();
				Dictionary dict;
				for (int i = 0; i < dn->elements.size(); i++) {
					ConstantNode *key_c = static_cast<ConstantNode *>(dn->elements[i].key);
					ConstantNode *value_c = static_cast<ConstantNode *>(dn->elements[i].value);

					dict[key_c->value] = value_c->value;
				}
				cn->value = dict;
				return cn;
			}

			return dn;

		} break;
		case Node::TYPE_OPERATOR: {

			OperatorNode *op = static_cast<OperatorNode *>(p_node);

			bool all_constants = true;
			bool all_have_type = true;
			int last_not_constant = -1;

			for (int i = 0; i < op->arguments.size(); i++) {

				op->arguments[i] = _reduce_expression(op->arguments[i], p_to_const);
				if (op->arguments[i]->type != Node::TYPE_CONSTANT) {
					all_constants = false;
					last_not_constant = i;
				}
				DataType datatype = op->arguments[i]->get_datatype();
				if (!datatype.has_type) {
					all_have_type = false;
				}
			}

			if (op->op == OperatorNode::OP_IS) {
				op->return_type.has_type = true;
				op->return_type.variant_type = Variant::BOOL;
				//nothing much
				return op;
			}
			if (op->op == OperatorNode::OP_PARENT_CALL) {
				//nothing much
				return op;

			} else if (op->op == OperatorNode::OP_CALL) {
				//can reduce base type constructors
				if ((op->arguments[0]->type == Node::TYPE_TYPE || (op->arguments[0]->type == Node::TYPE_BUILT_IN_FUNCTION && GDScriptFunctions::is_deterministic(static_cast<BuiltInFunctionNode *>(op->arguments[0])->function))) && last_not_constant == 0) {

					//native type constructor or intrinsic function
					const Variant **vptr = NULL;
					Vector<Variant *> ptrs;
					if (op->arguments.size() > 1) {

						ptrs.resize(op->arguments.size() - 1);
						for (int i = 0; i < ptrs.size(); i++) {

							ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[i + 1]);
							ptrs[i] = &cn->value;
						}

						vptr = (const Variant **)&ptrs[0];
					}

					Variant::CallError ce;
					Variant v;
					DataType data_type;

					if (op->arguments[0]->type == Node::TYPE_TYPE) {
						TypeNode *tn = static_cast<TypeNode *>(op->arguments[0]);
						v = Variant::construct(tn->vtype, vptr, ptrs.size(), ce);
						data_type.has_type = true;
						data_type.variant_type = tn->vtype;
					} else {
						GDScriptFunctions::Function func = static_cast<BuiltInFunctionNode *>(op->arguments[0])->function;
						GDScriptFunctions::call(func, vptr, ptrs.size(), v, ce);
						MethodInfo info = GDScriptFunctions::get_info(func);
						data_type.has_type = true;
						data_type.variant_type = info.return_val.type;
						data_type.class_name = info.return_val.class_name;
					}

					if (ce.error != Variant::CallError::CALL_OK) {

						String errwhere;
						if (op->arguments[0]->type == Node::TYPE_TYPE) {
							TypeNode *tn = static_cast<TypeNode *>(op->arguments[0]);
							errwhere = "'" + Variant::get_type_name(tn->vtype) + "' constructor";

						} else {
							GDScriptFunctions::Function func = static_cast<BuiltInFunctionNode *>(op->arguments[0])->function;
							errwhere = String("'") + GDScriptFunctions::get_func_name(func) + "' intrinsic function";
						}

						switch (ce.error) {

							case Variant::CallError::CALL_ERROR_INVALID_ARGUMENT: {

								_set_error("Invalid argument (#" + itos(ce.argument + 1) + ") for " + errwhere + ".");

							} break;
							case Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {

								_set_error("Too many arguments for " + errwhere + ".");
							} break;
							case Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS: {

								_set_error("Too few arguments for " + errwhere + ".");
							} break;
							default: {
								_set_error("Invalid arguments for " + errwhere + ".");

							} break;
						}

						error_line = op->line;

						return p_node;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->constant_type = data_type;
					return cn;

				} else if (op->arguments[0]->type == Node::TYPE_BUILT_IN_FUNCTION) {
					// Can't reduce but can get the return type and validate arguments
					GDScriptFunctions::Function func = static_cast<BuiltInFunctionNode *>(op->arguments[0])->function;
					MethodInfo info = GDScriptFunctions::get_info(func);

					if (false && (info.flags & METHOD_FLAG_VARARG) == 0) {
						// Only check for arguments if not var arg
						String errwhere = String("'") + GDScriptFunctions::get_func_name(func) + "'' intrinsic function";
						if (op->arguments.size() - 1 < (info.arguments.size() - info.default_arguments.size())) {
							_set_error("Too few arguments for " + errwhere + ".", op->line);
							return p_node;
						}
						if (op->arguments.size() - 1 > info.arguments.size()) {
							_set_error("Too many arguments for " + errwhere + ".", op->line);
							return p_node;
						}
					}

					op->return_type.has_type = true;
					op->return_type.variant_type = info.return_val.type;
					op->return_type.class_name = info.return_val.class_name;
				}

				return op; //don't reduce yet

			} else if (op->op == OperatorNode::OP_YIELD) {
				return op;

			} else if (op->op == OperatorNode::OP_INDEX) {
				//can reduce indices into constant arrays or dictionaries

				if (all_constants) {

					ConstantNode *ca = static_cast<ConstantNode *>(op->arguments[0]);
					ConstantNode *cb = static_cast<ConstantNode *>(op->arguments[1]);

					bool valid;

					Variant v = ca->value.get(cb->value, &valid);
					if (!valid) {
						_set_error("invalid index in constant expression");
						error_line = op->line;
						return op;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->constant_type.has_type = true;
					cn->constant_type.variant_type = v.get_type();
					return cn;

				} /*else if (op->arguments[0]->type==Node::TYPE_CONSTANT && op->arguments[1]->type==Node::TYPE_IDENTIFIER) {

					ConstantNode *ca = static_cast<ConstantNode*>(op->arguments[0]);
					IdentifierNode *ib = static_cast<IdentifierNode*>(op->arguments[1]);

					bool valid;
					Variant v = ca->value.get_named(ib->name,&valid);
					if (!valid) {
						_set_error("invalid index '"+String(ib->name)+"' in constant expression");
						return op;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value=v;
					return cn;
				}*/

				return op;

			} else if (op->op == OperatorNode::OP_INDEX_NAMED) {

				if (op->arguments[0]->type == Node::TYPE_CONSTANT && op->arguments[1]->type == Node::TYPE_IDENTIFIER) {

					ConstantNode *ca = static_cast<ConstantNode *>(op->arguments[0]);
					IdentifierNode *ib = static_cast<IdentifierNode *>(op->arguments[1]);

					bool valid;
					Variant v = ca->value.get_named(ib->name, &valid);
					if (!valid) {
						_set_error("invalid index '" + String(ib->name) + "' in constant expression");
						error_line = op->line;
						return op;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->constant_type.has_type = true;
					cn->constant_type.variant_type = v.get_type();
					if (v.get_type() == Variant::OBJECT) {
						cn->constant_type.class_name = v.operator Object *()->get_class_name();
					}
					return cn;
				}

				return op;
			}

			//validate assignment (don't assign to constant expression
			switch (op->op) {

				case OperatorNode::OP_ASSIGN:
				case OperatorNode::OP_ASSIGN_ADD:
				case OperatorNode::OP_ASSIGN_SUB:
				case OperatorNode::OP_ASSIGN_MUL:
				case OperatorNode::OP_ASSIGN_DIV:
				case OperatorNode::OP_ASSIGN_MOD:
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
				case OperatorNode::OP_ASSIGN_BIT_AND:
				case OperatorNode::OP_ASSIGN_BIT_OR:
				case OperatorNode::OP_ASSIGN_BIT_XOR: {

					if (op->arguments[0]->type == Node::TYPE_CONSTANT) {
						_set_error("Can't assign to constant", tokenizer->get_token_line() - 1);
						error_line = op->line;
						return op;
					}

					if (op->arguments[0]->type == Node::TYPE_OPERATOR) {
						OperatorNode *on = static_cast<OperatorNode *>(op->arguments[0]);
						if (on->op != OperatorNode::OP_INDEX && on->op != OperatorNode::OP_INDEX_NAMED) {
							_set_error("Can't assign to an expression", tokenizer->get_token_line() - 1);
							error_line = op->line;
							return op;
						}
					}

				} break;
				default: { break; }
			}
			//now se if all are constants
			if (!all_constants)
				return op; //nothing to reduce from here on
#define _REDUCE_UNARY(m_vop)                                                                               \
	bool valid = false;                                                                                    \
	Variant res;                                                                                           \
	Variant::evaluate(m_vop, static_cast<ConstantNode *>(op->arguments[0])->value, Variant(), res, valid); \
	if (!valid) {                                                                                          \
		_set_error("Invalid operand for unary operator");                                                  \
		error_line = op->line;                                                                             \
		return p_node;                                                                                     \
	}                                                                                                      \
	ConstantNode *cn = alloc_node<ConstantNode>();                                                         \
	cn->value = res;                                                                                       \
	cn->constant_type.has_type = true;                                                                     \
	cn->constant_type.variant_type = res.get_type();                                                       \
	return cn;

#define _REDUCE_BINARY(m_vop)                                                                                                                         \
	bool valid = false;                                                                                                                               \
	Variant res;                                                                                                                                      \
	Variant::evaluate(m_vop, static_cast<ConstantNode *>(op->arguments[0])->value, static_cast<ConstantNode *>(op->arguments[1])->value, res, valid); \
	if (!valid) {                                                                                                                                     \
		_set_error("Invalid operands for operator");                                                                                                  \
		error_line = op->line;                                                                                                                        \
		return p_node;                                                                                                                                \
	}                                                                                                                                                 \
	ConstantNode *cn = alloc_node<ConstantNode>();                                                                                                    \
	cn->value = res;                                                                                                                                  \
	cn->constant_type.has_type = true;                                                                                                                \
	cn->constant_type.variant_type = res.get_type();                                                                                                  \
	return cn;

			switch (op->op) {

				//unary operators
				case OperatorNode::OP_NEG: {
					_REDUCE_UNARY(Variant::OP_NEGATE);
				} break;
				case OperatorNode::OP_POS: {
					_REDUCE_UNARY(Variant::OP_POSITIVE);
				} break;
				case OperatorNode::OP_NOT: {
					_REDUCE_UNARY(Variant::OP_NOT);
				} break;
				case OperatorNode::OP_BIT_INVERT: {
					_REDUCE_UNARY(Variant::OP_BIT_NEGATE);
				} break;
				//binary operators (in precedence order)
				case OperatorNode::OP_IN: {
					_REDUCE_BINARY(Variant::OP_IN);
				} break;
				case OperatorNode::OP_EQUAL: {
					_REDUCE_BINARY(Variant::OP_EQUAL);
				} break;
				case OperatorNode::OP_NOT_EQUAL: {
					_REDUCE_BINARY(Variant::OP_NOT_EQUAL);
				} break;
				case OperatorNode::OP_LESS: {
					_REDUCE_BINARY(Variant::OP_LESS);
				} break;
				case OperatorNode::OP_LESS_EQUAL: {
					_REDUCE_BINARY(Variant::OP_LESS_EQUAL);
				} break;
				case OperatorNode::OP_GREATER: {
					_REDUCE_BINARY(Variant::OP_GREATER);
				} break;
				case OperatorNode::OP_GREATER_EQUAL: {
					_REDUCE_BINARY(Variant::OP_GREATER_EQUAL);
				} break;
				case OperatorNode::OP_AND: {
					_REDUCE_BINARY(Variant::OP_AND);
				} break;
				case OperatorNode::OP_OR: {
					_REDUCE_BINARY(Variant::OP_OR);
				} break;
				case OperatorNode::OP_ADD: {
					_REDUCE_BINARY(Variant::OP_ADD);
				} break;
				case OperatorNode::OP_SUB: {
					_REDUCE_BINARY(Variant::OP_SUBTRACT);
				} break;
				case OperatorNode::OP_MUL: {
					_REDUCE_BINARY(Variant::OP_MULTIPLY);
				} break;
				case OperatorNode::OP_DIV: {
					_REDUCE_BINARY(Variant::OP_DIVIDE);
				} break;
				case OperatorNode::OP_MOD: {
					_REDUCE_BINARY(Variant::OP_MODULE);
				} break;
				case OperatorNode::OP_SHIFT_LEFT: {
					_REDUCE_BINARY(Variant::OP_SHIFT_LEFT);
				} break;
				case OperatorNode::OP_SHIFT_RIGHT: {
					_REDUCE_BINARY(Variant::OP_SHIFT_RIGHT);
				} break;
				case OperatorNode::OP_BIT_AND: {
					_REDUCE_BINARY(Variant::OP_BIT_AND);
				} break;
				case OperatorNode::OP_BIT_OR: {
					_REDUCE_BINARY(Variant::OP_BIT_OR);
				} break;
				case OperatorNode::OP_BIT_XOR: {
					_REDUCE_BINARY(Variant::OP_BIT_XOR);
				} break;
				default: { ERR_FAIL_V(op); }
			}

			ERR_FAIL_V(op);
		} break;
		default: {
			return p_node;
		} break;
	}
}

GDScriptParser::Node *GDScriptParser::_parse_and_reduce_expression(Node *p_parent, bool p_static, bool p_reduce_const, bool p_allow_assign) {

	Node *expr = _parse_expression(p_parent, p_static, p_allow_assign, p_reduce_const);
	if (!expr || error_set)
		return NULL;
	expr = _reduce_expression(expr, p_reduce_const);
	if (!expr || error_set)
		return NULL;
	return expr;
}

bool GDScriptParser::_recover_from_completion() {

	if (!completion_found) {
		return false; //can't recover if no completion
	}
	//skip stuff until newline
	while (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE && tokenizer->get_token() != GDScriptTokenizer::TK_EOF && tokenizer->get_token() != GDScriptTokenizer::TK_ERROR) {
		tokenizer->advance();
	}
	completion_found = false;
	error_set = false;
	if (tokenizer->get_token() == GDScriptTokenizer::TK_ERROR) {
		error_set = true;
	}

	return true;
}

GDScriptParser::PatternNode *GDScriptParser::_parse_pattern(bool p_static) {

	PatternNode *pattern = alloc_node<PatternNode>();

	GDScriptTokenizer::Token token = tokenizer->get_token();
	if (error_set)
		return NULL;

	if (token == GDScriptTokenizer::TK_EOF) {
		return NULL;
	}

	switch (token) {
		// array
		case GDScriptTokenizer::TK_BRACKET_OPEN: {
			tokenizer->advance();
			pattern->pt_type = GDScriptParser::PatternNode::PT_ARRAY;
			while (true) {

				if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {
					// match everything
					tokenizer->advance(2);
					PatternNode *sub_pattern = alloc_node<PatternNode>();
					sub_pattern->pt_type = GDScriptParser::PatternNode::PT_IGNORE_REST;
					pattern->array.push_back(sub_pattern);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA && tokenizer->get_token(1) == GDScriptTokenizer::TK_BRACKET_CLOSE) {
						tokenizer->advance(2);
						break;
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
						tokenizer->advance(1);
						break;
					} else {
						_set_error("'..' pattern only allowed at the end of an array pattern");
						return NULL;
					}
				}

				PatternNode *sub_pattern = _parse_pattern(p_static);
				if (!sub_pattern) {
					return NULL;
				}

				pattern->array.push_back(sub_pattern);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					tokenizer->advance();
					continue;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else {
					_set_error("Not a valid pattern");
					return NULL;
				}
			}
		} break;
		// bind
		case GDScriptTokenizer::TK_PR_VAR: {
			tokenizer->advance();
			pattern->pt_type = GDScriptParser::PatternNode::PT_BIND;
			pattern->bind = tokenizer->get_token_identifier();
			tokenizer->advance();
		} break;
		// dictionary
		case GDScriptTokenizer::TK_CURLY_BRACKET_OPEN: {
			tokenizer->advance();
			pattern->pt_type = GDScriptParser::PatternNode::PT_DICTIONARY;
			while (true) {

				if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {
					// match everything
					tokenizer->advance(2);
					PatternNode *sub_pattern = alloc_node<PatternNode>();
					sub_pattern->pt_type = PatternNode::PT_IGNORE_REST;
					pattern->array.push_back(sub_pattern);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA && tokenizer->get_token(1) == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
						tokenizer->advance(2);
						break;
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
						tokenizer->advance(1);
						break;
					} else {
						_set_error("'..' pattern only allowed at the end of a dictionary pattern");
						return NULL;
					}
				}

				Node *key = _parse_and_reduce_expression(pattern, p_static);
				if (!key) {
					_set_error("Not a valid key in pattern");
					return NULL;
				}

				if (key->type != GDScriptParser::Node::TYPE_CONSTANT) {
					_set_error("Not a constant expression as key");
					return NULL;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					tokenizer->advance();

					PatternNode *value = _parse_pattern(p_static);
					if (!value) {
						_set_error("Expected pattern in dictionary value");
						return NULL;
					}

					pattern->dictionary.insert(static_cast<ConstantNode *>(key), value);
				} else {
					pattern->dictionary.insert(static_cast<ConstantNode *>(key), NULL);
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					tokenizer->advance();
					continue;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else {
					_set_error("Not a valid pattern");
					return NULL;
				}
			}
		} break;
		case GDScriptTokenizer::TK_WILDCARD: {
			tokenizer->advance();
			pattern->pt_type = PatternNode::PT_WILDCARD;
		} break;
		// all the constants like strings and numbers
		default: {
			Node *value = _parse_and_reduce_expression(pattern, p_static);
			if (!value) {
				_set_error("Expect constant expression or variables in a pattern");
				return NULL;
			}

			if (value->type == Node::TYPE_OPERATOR) {
				// Maybe it's SomeEnum.VALUE
				Node *current_value = value;

				while (current_value->type == Node::TYPE_OPERATOR) {
					OperatorNode *op_node = static_cast<OperatorNode *>(current_value);

					if (op_node->op != OperatorNode::OP_INDEX_NAMED) {
						_set_error("Invalid operator in pattern. Only index (`A.B`) is allowed");
						return NULL;
					}
					current_value = op_node->arguments[0];
				}

				if (current_value->type != Node::TYPE_IDENTIFIER) {
					_set_error("Only constant expression or variables allowed in a pattern");
					return NULL;
				}

			} else if (value->type != Node::TYPE_IDENTIFIER && value->type != Node::TYPE_CONSTANT) {
				_set_error("Only constant expressions or variables allowed in a pattern");
				return NULL;
			}

			pattern->pt_type = PatternNode::PT_CONSTANT;
			pattern->constant = value;
		} break;
	}

	return pattern;
}

void GDScriptParser::_parse_pattern_block(BlockNode *p_block, Vector<PatternBranchNode *> &p_branches, bool p_static) {
	int indent_level = tab_level.back()->get();

	while (true) {

		while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE && _parse_newline())
			;

		// GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set)
			return;

		if (indent_level > tab_level.back()->get()) {
			return; // go back a level
		}

		if (pending_newline != -1) {
			pending_newline = -1;
		}

		PatternBranchNode *branch = alloc_node<PatternBranchNode>();

		branch->patterns.push_back(_parse_pattern(p_static));
		if (!branch->patterns[0]) {
			return;
		}

		while (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
			tokenizer->advance();
			branch->patterns.push_back(_parse_pattern(p_static));
			if (!branch->patterns[branch->patterns.size() - 1]) {
				return;
			}
		}

		if (!_enter_indent_block()) {
			_set_error("Expected block in pattern branch");
			return;
		}

		branch->body = alloc_node<BlockNode>();
		branch->body->parent_block = p_block;
		p_block->sub_blocks.push_back(branch->body);
		current_block = branch->body;

		_parse_block(branch->body, p_static);

		current_block = p_block;

		p_branches.push_back(branch);
	}
}

void GDScriptParser::_generate_pattern(PatternNode *p_pattern, Node *p_node_to_match, Node *&p_resulting_node, Map<StringName, Node *> &p_bindings) {
	switch (p_pattern->pt_type) {
		case PatternNode::PT_CONSTANT: {

			// typecheck
			BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
			typeof_node->function = GDScriptFunctions::TYPE_OF;

			OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
			typeof_match_value->op = OperatorNode::OP_CALL;
			typeof_match_value->arguments.push_back(typeof_node);
			typeof_match_value->arguments.push_back(p_node_to_match);

			OperatorNode *typeof_pattern_value = alloc_node<OperatorNode>();
			typeof_pattern_value->op = OperatorNode::OP_CALL;
			typeof_pattern_value->arguments.push_back(typeof_node);
			typeof_pattern_value->arguments.push_back(p_pattern->constant);

			OperatorNode *type_comp = alloc_node<OperatorNode>();
			type_comp->op = OperatorNode::OP_EQUAL;
			type_comp->arguments.push_back(typeof_match_value);
			type_comp->arguments.push_back(typeof_pattern_value);

			// comare the actual values
			OperatorNode *value_comp = alloc_node<OperatorNode>();
			value_comp->op = OperatorNode::OP_EQUAL;
			value_comp->arguments.push_back(p_pattern->constant);
			value_comp->arguments.push_back(p_node_to_match);

			OperatorNode *comparison = alloc_node<OperatorNode>();
			comparison->op = OperatorNode::OP_AND;
			comparison->arguments.push_back(type_comp);
			comparison->arguments.push_back(value_comp);

			p_resulting_node = comparison;

		} break;
		case PatternNode::PT_BIND: {
			p_bindings[p_pattern->bind] = p_node_to_match;

			// a bind always matches
			ConstantNode *true_value = alloc_node<ConstantNode>();
			true_value->value = Variant(true);
			p_resulting_node = true_value;
		} break;
		case PatternNode::PT_ARRAY: {

			bool open_ended = false;

			if (p_pattern->array.size() > 0) {
				if (p_pattern->array[p_pattern->array.size() - 1]->pt_type == PatternNode::PT_IGNORE_REST) {
					open_ended = true;
				}
			}

			// typeof(value_to_match) == TYPE_ARRAY && value_to_match.size() >= length
			// typeof(value_to_match) == TYPE_ARRAY && value_to_match.size() == length

			{
				// typecheck
				BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
				typeof_node->function = GDScriptFunctions::TYPE_OF;

				OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
				typeof_match_value->op = OperatorNode::OP_CALL;
				typeof_match_value->arguments.push_back(typeof_node);
				typeof_match_value->arguments.push_back(p_node_to_match);

				IdentifierNode *typeof_array = alloc_node<IdentifierNode>();
				typeof_array->name = "TYPE_ARRAY";

				OperatorNode *type_comp = alloc_node<OperatorNode>();
				type_comp->op = OperatorNode::OP_EQUAL;
				type_comp->arguments.push_back(typeof_match_value);
				type_comp->arguments.push_back(typeof_array);

				// size
				ConstantNode *length = alloc_node<ConstantNode>();
				length->value = Variant(open_ended ? p_pattern->array.size() - 1 : p_pattern->array.size());

				OperatorNode *call = alloc_node<OperatorNode>();
				call->op = OperatorNode::OP_CALL;
				call->arguments.push_back(p_node_to_match);

				IdentifierNode *size = alloc_node<IdentifierNode>();
				size->name = "size";
				call->arguments.push_back(size);

				OperatorNode *length_comparison = alloc_node<OperatorNode>();
				length_comparison->op = open_ended ? OperatorNode::OP_GREATER_EQUAL : OperatorNode::OP_EQUAL;
				length_comparison->arguments.push_back(call);
				length_comparison->arguments.push_back(length);

				OperatorNode *type_and_length_comparison = alloc_node<OperatorNode>();
				type_and_length_comparison->op = OperatorNode::OP_AND;
				type_and_length_comparison->arguments.push_back(type_comp);
				type_and_length_comparison->arguments.push_back(length_comparison);

				p_resulting_node = type_and_length_comparison;
			}

			for (int i = 0; i < p_pattern->array.size(); i++) {
				PatternNode *pattern = p_pattern->array[i];

				Node *condition = NULL;

				ConstantNode *index = alloc_node<ConstantNode>();
				index->value = Variant(i);

				OperatorNode *indexed_value = alloc_node<OperatorNode>();
				indexed_value->op = OperatorNode::OP_INDEX;
				indexed_value->arguments.push_back(p_node_to_match);
				indexed_value->arguments.push_back(index);

				_generate_pattern(pattern, indexed_value, condition, p_bindings);

				// concatenate all the patterns with &&
				OperatorNode *and_node = alloc_node<OperatorNode>();
				and_node->op = OperatorNode::OP_AND;
				and_node->arguments.push_back(p_resulting_node);
				and_node->arguments.push_back(condition);

				p_resulting_node = and_node;
			}

		} break;
		case PatternNode::PT_DICTIONARY: {

			bool open_ended = false;

			if (p_pattern->array.size() > 0) {
				open_ended = true;
			}

			// typeof(value_to_match) == TYPE_DICTIONARY && value_to_match.size() >= length
			// typeof(value_to_match) == TYPE_DICTIONARY && value_to_match.size() == length

			{
				// typecheck
				BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
				typeof_node->function = GDScriptFunctions::TYPE_OF;

				OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
				typeof_match_value->op = OperatorNode::OP_CALL;
				typeof_match_value->arguments.push_back(typeof_node);
				typeof_match_value->arguments.push_back(p_node_to_match);

				IdentifierNode *typeof_dictionary = alloc_node<IdentifierNode>();
				typeof_dictionary->name = "TYPE_DICTIONARY";

				OperatorNode *type_comp = alloc_node<OperatorNode>();
				type_comp->op = OperatorNode::OP_EQUAL;
				type_comp->arguments.push_back(typeof_match_value);
				type_comp->arguments.push_back(typeof_dictionary);

				// size
				ConstantNode *length = alloc_node<ConstantNode>();
				length->value = Variant(open_ended ? p_pattern->dictionary.size() - 1 : p_pattern->dictionary.size());

				OperatorNode *call = alloc_node<OperatorNode>();
				call->op = OperatorNode::OP_CALL;
				call->arguments.push_back(p_node_to_match);

				IdentifierNode *size = alloc_node<IdentifierNode>();
				size->name = "size";
				call->arguments.push_back(size);

				OperatorNode *length_comparison = alloc_node<OperatorNode>();
				length_comparison->op = open_ended ? OperatorNode::OP_GREATER_EQUAL : OperatorNode::OP_EQUAL;
				length_comparison->arguments.push_back(call);
				length_comparison->arguments.push_back(length);

				OperatorNode *type_and_length_comparison = alloc_node<OperatorNode>();
				type_and_length_comparison->op = OperatorNode::OP_AND;
				type_and_length_comparison->arguments.push_back(type_comp);
				type_and_length_comparison->arguments.push_back(length_comparison);

				p_resulting_node = type_and_length_comparison;
			}

			for (Map<ConstantNode *, PatternNode *>::Element *e = p_pattern->dictionary.front(); e; e = e->next()) {

				Node *condition = NULL;

				// chech for has, then for pattern

				IdentifierNode *has = alloc_node<IdentifierNode>();
				has->name = "has";

				OperatorNode *has_call = alloc_node<OperatorNode>();
				has_call->op = OperatorNode::OP_CALL;
				has_call->arguments.push_back(p_node_to_match);
				has_call->arguments.push_back(has);
				has_call->arguments.push_back(e->key());

				if (e->value()) {

					OperatorNode *indexed_value = alloc_node<OperatorNode>();
					indexed_value->op = OperatorNode::OP_INDEX;
					indexed_value->arguments.push_back(p_node_to_match);
					indexed_value->arguments.push_back(e->key());

					_generate_pattern(e->value(), indexed_value, condition, p_bindings);

					OperatorNode *has_and_pattern = alloc_node<OperatorNode>();
					has_and_pattern->op = OperatorNode::OP_AND;
					has_and_pattern->arguments.push_back(has_call);
					has_and_pattern->arguments.push_back(condition);

					condition = has_and_pattern;

				} else {
					condition = has_call;
				}

				// concatenate all the patterns with &&
				OperatorNode *and_node = alloc_node<OperatorNode>();
				and_node->op = OperatorNode::OP_AND;
				and_node->arguments.push_back(p_resulting_node);
				and_node->arguments.push_back(condition);

				p_resulting_node = and_node;
			}

		} break;
		case PatternNode::PT_IGNORE_REST:
		case PatternNode::PT_WILDCARD: {
			// simply generate a `true`
			ConstantNode *true_value = alloc_node<ConstantNode>();
			true_value->value = Variant(true);
			p_resulting_node = true_value;
		} break;
		default: {

		} break;
	}
}

void GDScriptParser::_transform_match_statment(BlockNode *p_block, MatchNode *p_match_statement) {
	IdentifierNode *id = alloc_node<IdentifierNode>();
	id->name = "#match_value";

	for (int i = 0; i < p_match_statement->branches.size(); i++) {

		PatternBranchNode *branch = p_match_statement->branches[i];

		MatchNode::CompiledPatternBranch compiled_branch;
		compiled_branch.compiled_pattern = NULL;

		Map<StringName, Node *> binding;

		for (int j = 0; j < branch->patterns.size(); j++) {
			PatternNode *pattern = branch->patterns[j];

			Map<StringName, Node *> bindings;
			Node *resulting_node;
			_generate_pattern(pattern, id, resulting_node, bindings);

			if (!binding.empty() && !bindings.empty()) {
				_set_error("Multipatterns can't contain bindings");
				return;
			} else {
				binding = bindings;
			}

			if (compiled_branch.compiled_pattern) {
				OperatorNode *or_node = alloc_node<OperatorNode>();
				or_node->op = OperatorNode::OP_OR;
				or_node->arguments.push_back(compiled_branch.compiled_pattern);
				or_node->arguments.push_back(resulting_node);

				compiled_branch.compiled_pattern = or_node;
			} else {
				// single pattern | first one
				compiled_branch.compiled_pattern = resulting_node;
			}
		}

		// prepare the body ...hehe
		for (Map<StringName, Node *>::Element *e = binding.front(); e; e = e->next()) {
			LocalVarNode *local_var = alloc_node<LocalVarNode>();
			local_var->name = e->key();
			local_var->assign = e->value();

			IdentifierNode *id = alloc_node<IdentifierNode>();
			id->name = local_var->name;

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_ASSIGN;
			op->arguments.push_back(id);
			op->arguments.push_back(local_var->assign);

			branch->body->statements.push_front(op);
			branch->body->statements.push_front(local_var);
		}

		compiled_branch.body = branch->body;

		p_match_statement->compiled_pattern_branches.push_back(compiled_branch);
	}
}

void GDScriptParser::_parse_block(BlockNode *p_block, bool p_static) {

	int indent_level = tab_level.back()->get();

#ifdef DEBUG_ENABLED

	NewLineNode *nl = alloc_node<NewLineNode>();

	nl->line = tokenizer->get_token_line();
	p_block->statements.push_back(nl);
#endif

	bool is_first_line = true;

	while (true) {
		if (!is_first_line && tab_level.back()->prev() && tab_level.back()->prev()->get() == indent_level) {
			// pythonic single-line expression, don't parse future lines
			tab_level.pop_back();
			p_block->end_line = tokenizer->get_token_line();
			return;
		}
		is_first_line = false;

		GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set)
			return;

		if (indent_level > tab_level.back()->get()) {
			p_block->end_line = tokenizer->get_token_line();
			return; //go back a level
		}

		if (pending_newline != -1) {

			NewLineNode *nl = alloc_node<NewLineNode>();
			nl->line = pending_newline;
			p_block->statements.push_back(nl);
			pending_newline = -1;
		}

		switch (token) {

			case GDScriptTokenizer::TK_EOF:
				p_block->end_line = tokenizer->get_token_line();
			case GDScriptTokenizer::TK_ERROR: {
				return; //go back

				//end of file!

			} break;
			case GDScriptTokenizer::TK_NEWLINE: {

				if (!_parse_newline()) {
					if (!error_set) {
						p_block->end_line = tokenizer->get_token_line();
						pending_newline = p_block->end_line;
					}
					return;
				}

				NewLineNode *nl = alloc_node<NewLineNode>();
				nl->line = tokenizer->get_token_line();
				p_block->statements.push_back(nl);

			} break;
			case GDScriptTokenizer::TK_CF_PASS: {
				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_SEMICOLON && tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE && tokenizer->get_token(1) != GDScriptTokenizer::TK_EOF) {

					_set_error("Expected ';' or <NewLine>.");
					return;
				}
				tokenizer->advance();
				if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON) {
					// Ignore semicolon after 'pass'
					tokenizer->advance();
				}
			} break;
			case GDScriptTokenizer::TK_PR_VAR: {
				//variale declaration and (eventual) initialization

				tokenizer->advance();
				if (!tokenizer->is_token_literal(0, true)) {

					_set_error("Expected identifier for local variable name.");
					return;
				}
				StringName n = tokenizer->get_token_literal();
				tokenizer->advance();
				if (current_function) {
					for (int i = 0; i < current_function->arguments.size(); i++) {
						if (n == current_function->arguments[i]) {
							_set_error("Variable '" + String(n) + "' already defined in the scope (at line: " + itos(current_function->line) + ").");
							return;
						}
					}
				}
				BlockNode *check_block = p_block;
				while (check_block) {
					for (int i = 0; i < check_block->variables.size(); i++) {
						if (n == check_block->variables[i]) {
							_set_error("Variable '" + String(n) + "' already defined in the scope (at line: " + itos(check_block->variable_lines[i]) + ").");
							return;
						}
					}
					check_block = check_block->parent_block;
				}

				int var_line = tokenizer->get_token_line();

				LocalVarNode *lv = alloc_node<LocalVarNode>();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					// Has type
					if (!_parse_type(&lv->data_type)) {
						_set_error("Expected type for local variable.");
						return;
					}
					type_check = true;
				}

				//must know when the local variable is declared
				lv->name = n;
				p_block->statements.push_back(lv);

				Node *assigned = NULL;

				if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {

					tokenizer->advance();

					Node *subexpr = _parse_and_reduce_expression(p_block, p_static);
					if (!subexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}

					lv->assign = subexpr;
					assigned = subexpr;
				} else {
					ConstantNode *c = alloc_node<ConstantNode>();
					// Get default value for type
					if (lv->data_type.has_type) {
						Variant::CallError ce;
						c->value = Variant::construct(lv->data_type.variant_type, NULL, 0, ce);
						c->constant_type = lv->data_type;
					} else {
						c->value = Variant();
					}
					assigned = c;
				}
				//must be added later, to avoid self-referencing.
				p_block->variables.push_back(n); //line?
				p_block->variable_lines.push_back(var_line);
				p_block->variable_types.push_back(lv->data_type);

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = n;
				id->data_type = lv->data_type;

				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = OperatorNode::OP_ASSIGN;
				op->arguments.push_back(id);
				op->arguments.push_back(assigned);
				p_block->statements.push_back(op);

				if (!_end_statement()) {
					_set_error("Expected end of statement (var)");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_CF_IF: {

				tokenizer->advance();

				Node *condition = _parse_and_reduce_expression(p_block, p_static);
				if (!condition) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				ControlFlowNode *cf_if = alloc_node<ControlFlowNode>();

				cf_if->cf_type = ControlFlowNode::CF_IF;
				cf_if->arguments.push_back(condition);

				cf_if->body = alloc_node<BlockNode>();
				cf_if->body->parent_block = p_block;
				cf_if->body->if_condition = condition; //helps code completion

				p_block->sub_blocks.push_back(cf_if->body);

				if (!_enter_indent_block(cf_if->body)) {
					_set_error("Expected indented block after 'if'");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_if->body;
				_parse_block(cf_if->body, p_static);
				current_block = p_block;

				if (error_set)
					return;
				p_block->statements.push_back(cf_if);

				while (true) {

					while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE && _parse_newline())
						;

					if (tab_level.back()->get() < indent_level) { //not at current indent level
						p_block->end_line = tokenizer->get_token_line();
						return;
					}

					if (tokenizer->get_token() == GDScriptTokenizer::TK_CF_ELIF) {

						if (tab_level.back()->get() > indent_level) {

							_set_error("Invalid indent");
							return;
						}

						tokenizer->advance();

						cf_if->body_else = alloc_node<BlockNode>();
						cf_if->body_else->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body_else);

						ControlFlowNode *cf_else = alloc_node<ControlFlowNode>();
						cf_else->cf_type = ControlFlowNode::CF_IF;

						//condition
						Node *condition = _parse_and_reduce_expression(p_block, p_static);
						if (!condition) {
							if (_recover_from_completion()) {
								break;
							}
							return;
						}
						cf_else->arguments.push_back(condition);
						cf_else->cf_type = ControlFlowNode::CF_IF;

						cf_if->body_else->statements.push_back(cf_else);
						cf_if = cf_else;
						cf_if->body = alloc_node<BlockNode>();
						cf_if->body->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body);

						if (!_enter_indent_block(cf_if->body)) {
							_set_error("Expected indented block after 'elif'");
							p_block->end_line = tokenizer->get_token_line();
							return;
						}

						current_block = cf_else->body;
						_parse_block(cf_else->body, p_static);
						current_block = p_block;
						if (error_set)
							return;

					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CF_ELSE) {

						if (tab_level.back()->get() > indent_level) {
							_set_error("Invalid indent");
							return;
						}

						tokenizer->advance();
						cf_if->body_else = alloc_node<BlockNode>();
						cf_if->body_else->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body_else);

						if (!_enter_indent_block(cf_if->body_else)) {
							_set_error("Expected indented block after 'else'");
							p_block->end_line = tokenizer->get_token_line();
							return;
						}
						current_block = cf_if->body_else;
						_parse_block(cf_if->body_else, p_static);
						current_block = p_block;
						if (error_set)
							return;

						break; //after else, exit

					} else
						break;
				}

			} break;
			case GDScriptTokenizer::TK_CF_WHILE: {

				tokenizer->advance();
				Node *condition = _parse_and_reduce_expression(p_block, p_static);
				if (!condition) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				ControlFlowNode *cf_while = alloc_node<ControlFlowNode>();

				cf_while->cf_type = ControlFlowNode::CF_WHILE;
				cf_while->arguments.push_back(condition);

				cf_while->body = alloc_node<BlockNode>();
				cf_while->body->parent_block = p_block;
				p_block->sub_blocks.push_back(cf_while->body);

				if (!_enter_indent_block(cf_while->body)) {
					_set_error("Expected indented block after 'while'");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_while->body;
				_parse_block(cf_while->body, p_static);
				current_block = p_block;
				if (error_set)
					return;
				p_block->statements.push_back(cf_while);
			} break;
			case GDScriptTokenizer::TK_CF_FOR: {

				tokenizer->advance();

				if (!tokenizer->is_token_literal(0, true)) {

					_set_error("identifier expected after 'for'");
				}

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = tokenizer->get_token_identifier();

				tokenizer->advance();

				if (tokenizer->get_token() != GDScriptTokenizer::TK_OP_IN) {
					_set_error("'in' expected after identifier");
					return;
				}

				tokenizer->advance();

				Node *container = _parse_and_reduce_expression(p_block, p_static);
				if (!container) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				if (container->type == Node::TYPE_OPERATOR) {

					OperatorNode *op = static_cast<OperatorNode *>(container);
					if (op->op == OperatorNode::OP_CALL && op->arguments[0]->type == Node::TYPE_BUILT_IN_FUNCTION && static_cast<BuiltInFunctionNode *>(op->arguments[0])->function == GDScriptFunctions::GEN_RANGE) {
						//iterating a range, so see if range() can be optimized without allocating memory, by replacing it by vectors (which can work as iterable too!)

						Vector<Node *> args;
						Vector<double> constants;

						bool constant = false;

						for (int i = 1; i < op->arguments.size(); i++) {
							args.push_back(op->arguments[i]);
							if (constant && op->arguments[i]->type == Node::TYPE_CONSTANT) {
								ConstantNode *c = static_cast<ConstantNode *>(op->arguments[i]);
								if (c->value.get_type() == Variant::REAL || c->value.get_type() == Variant::INT) {
									constants.push_back(c->value);
									constant = true;
								}
							} else {
								constant = false;
							}
						}

						if (args.size() > 0 && args.size() < 4) {

							if (constant) {

								ConstantNode *cn = alloc_node<ConstantNode>();
								switch (args.size()) {
									case 1: cn->value = (int)constants[0]; break;
									case 2: cn->value = Vector2(constants[0], constants[1]); break;
									case 3: cn->value = Vector3(constants[0], constants[1], constants[2]); break;
								}
								container = cn;
							} else {
								OperatorNode *on = alloc_node<OperatorNode>();
								on->op = OperatorNode::OP_CALL;

								TypeNode *tn = alloc_node<TypeNode>();
								on->arguments.push_back(tn);

								switch (args.size()) {
									case 1: tn->vtype = Variant::INT; break;
									case 2: tn->vtype = Variant::VECTOR2; break;
									case 3: tn->vtype = Variant::VECTOR3; break;
								}

								for (int i = 0; i < args.size(); i++) {
									on->arguments.push_back(args[i]);
								}

								container = on;
							}
						}
					}
				}

				ControlFlowNode *cf_for = alloc_node<ControlFlowNode>();

				cf_for->cf_type = ControlFlowNode::CF_FOR;
				cf_for->arguments.push_back(id);
				cf_for->arguments.push_back(container);

				cf_for->body = alloc_node<BlockNode>();
				cf_for->body->parent_block = p_block;
				p_block->sub_blocks.push_back(cf_for->body);

				if (!_enter_indent_block(cf_for->body)) {
					_set_error("Expected indented block after 'for'");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_for->body;

				// this is for checking variable for redefining
				// inside this _parse_block
				cf_for->body->variables.push_back(id->name);
				cf_for->body->variable_lines.push_back(id->line);
				cf_for->body->variable_types.push_back(id->data_type);
				_parse_block(cf_for->body, p_static);
				cf_for->body->variables.remove(0);
				cf_for->body->variable_lines.remove(0);
				cf_for->body->variable_types.remove(0);
				current_block = p_block;

				if (error_set)
					return;
				p_block->statements.push_back(cf_for);
			} break;
			case GDScriptTokenizer::TK_CF_CONTINUE: {

				tokenizer->advance();
				ControlFlowNode *cf_continue = alloc_node<ControlFlowNode>();
				cf_continue->cf_type = ControlFlowNode::CF_CONTINUE;
				p_block->statements.push_back(cf_continue);
				if (!_end_statement()) {
					_set_error("Expected end of statement (continue)");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_CF_BREAK: {

				tokenizer->advance();
				ControlFlowNode *cf_break = alloc_node<ControlFlowNode>();
				cf_break->cf_type = ControlFlowNode::CF_BREAK;
				p_block->statements.push_back(cf_break);
				if (!_end_statement()) {
					_set_error("Expected end of statement (break)");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_CF_RETURN: {

				tokenizer->advance();
				ControlFlowNode *cf_return = alloc_node<ControlFlowNode>();
				cf_return->cf_type = ControlFlowNode::CF_RETURN;

				if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON || tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE || tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
					//expect end of statement
					p_block->statements.push_back(cf_return);
					if (!_end_statement()) {
						return;
					}
				} else {
					//expect expression
					Node *retexpr = _parse_and_reduce_expression(p_block, p_static);
					if (!retexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}

					cf_return->arguments.push_back(retexpr);
					p_block->statements.push_back(cf_return);
					if (!_end_statement()) {
						_set_error("Expected end of statement after return expression.");
						return;
					}
				}

			} break;
			case GDScriptTokenizer::TK_CF_MATCH: {

				tokenizer->advance();

				MatchNode *match_node = alloc_node<MatchNode>();

				Node *val_to_match = _parse_and_reduce_expression(p_block, p_static);

				if (!val_to_match) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				match_node->val_to_match = val_to_match;

				if (!_enter_indent_block()) {
					_set_error("Expected indented pattern matching block after 'match'");
					return;
				}

				BlockNode *compiled_branches = alloc_node<BlockNode>();
				compiled_branches->parent_block = p_block;
				compiled_branches->parent_class = p_block->parent_class;

				p_block->sub_blocks.push_back(compiled_branches);

				_parse_pattern_block(compiled_branches, match_node->branches, p_static);

				_transform_match_statment(compiled_branches, match_node);

				ControlFlowNode *match_cf_node = alloc_node<ControlFlowNode>();
				match_cf_node->cf_type = ControlFlowNode::CF_MATCH;
				match_cf_node->match = match_node;

				p_block->statements.push_back(match_cf_node);

				_end_statement();
			} break;
			case GDScriptTokenizer::TK_PR_ASSERT: {

				tokenizer->advance();
				Node *condition = _parse_and_reduce_expression(p_block, p_static);
				if (!condition) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}
				AssertNode *an = alloc_node<AssertNode>();
				an->condition = condition;
				p_block->statements.push_back(an);

				if (!_end_statement()) {
					_set_error("Expected end of statement after assert.");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_BREAKPOINT: {

				tokenizer->advance();
				BreakpointNode *bn = alloc_node<BreakpointNode>();
				p_block->statements.push_back(bn);

				if (!_end_statement()) {
					_set_error("Expected end of statement after breakpoint.");
					return;
				}
			} break;
			default: {

				Node *expression = _parse_and_reduce_expression(p_block, p_static, false, true);
				if (!expression) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}
				p_block->statements.push_back(expression);
				if (!_end_statement()) {
					_set_error("Expected end of statement after expression.");
					return;
				}

			} break;
				/*
			case GDScriptTokenizer::TK_CF_LOCAL: {

				if (tokenizer->get_token(1)!=GDScriptTokenizer::TK_SEMICOLON && tokenizer->get_token(1)!=GDScriptTokenizer::TK_NEWLINE ) {

					_set_error("Expected ';' or <NewLine>.");
				}
				tokenizer->advance();
			} break;
			*/
		}
	}
}

bool GDScriptParser::_parse_newline() {

	if (tokenizer->get_token(1) != GDScriptTokenizer::TK_EOF && tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE) {

		int indent = tokenizer->get_token_line_indent();
		int current_indent = tab_level.back()->get();

		if (indent > current_indent) {
			_set_error("Unexpected indent.");
			return false;
		}

		if (indent < current_indent) {

			while (indent < current_indent) {

				//exit block
				if (tab_level.size() == 1) {
					_set_error("Invalid indent. BUG?");
					return false;
				}

				tab_level.pop_back();

				if (tab_level.back()->get() < indent) {

					_set_error("Unindent does not match any outer indentation level.");
					return false;
				}
				current_indent = tab_level.back()->get();
			}

			tokenizer->advance();
			return false;
		}
	}

	tokenizer->advance();
	return true;
}

void GDScriptParser::_parse_extends(ClassNode *p_class) {

	if (p_class->extends_used) {

		_set_error("'extends' already used for this class.");
		return;
	}

	if (!p_class->constant_expressions.empty() || !p_class->subclasses.empty() || !p_class->functions.empty() || !p_class->variables.empty()) {

		_set_error("'extends' must be used before anything else.");
		return;
	}

	p_class->extends_used = true;

	tokenizer->advance();

	if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE && tokenizer->get_token_type() == Variant::OBJECT) {
		p_class->extends_class.push_back(Variant::get_type_name(Variant::OBJECT));
		tokenizer->advance();
		return;
	}

	// see if inheritance happens from a file
	if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT) {

		Variant constant = tokenizer->get_token_constant();
		if (constant.get_type() != Variant::STRING) {

			_set_error("'extends' constant must be a string.");
			return;
		}

		p_class->extends_file = constant;
		tokenizer->advance();

		if (tokenizer->get_token() != GDScriptTokenizer::TK_PERIOD) {
			return;
		} else
			tokenizer->advance();
	}

	while (true) {

		switch (tokenizer->get_token()) {

			case GDScriptTokenizer::TK_IDENTIFIER: {

				StringName identifier = tokenizer->get_token_identifier();
				p_class->extends_class.push_back(identifier);
			} break;

			case GDScriptTokenizer::TK_PERIOD:
				break;

			default: {

				_set_error("Invalid 'extends' syntax, expected string constant (path) and/or identifier (parent class).");
				return;
			}
		}

		tokenizer->advance(1);

		switch (tokenizer->get_token()) {

			case GDScriptTokenizer::TK_IDENTIFIER:
			case GDScriptTokenizer::TK_PERIOD:
				continue;

			default:
				return;
		}
	}
}

void GDScriptParser::_parse_class(ClassNode *p_class) {

	int indent_level = tab_level.back()->get();

	while (true) {

		GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set)
			return;

		if (indent_level > tab_level.back()->get()) {
			p_class->end_line = tokenizer->get_token_line();
			return; //go back a level
		}

		switch (token) {

			case GDScriptTokenizer::TK_EOF:
				p_class->end_line = tokenizer->get_token_line();
			case GDScriptTokenizer::TK_ERROR: {
				return; //go back
				//end of file!
			} break;
			case GDScriptTokenizer::TK_NEWLINE: {
				if (!_parse_newline()) {
					if (!error_set) {
						p_class->end_line = tokenizer->get_token_line();
					}
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_EXTENDS: {

				_parse_extends(p_class);
				if (error_set)
					return;
				if (!_end_statement()) {
					_set_error("Expected end of statement after extends");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_PR_TOOL: {

				if (p_class->tool) {

					_set_error("tool used more than once");
					return;
				}

				p_class->tool = true;
				tokenizer->advance();

			} break;
			case GDScriptTokenizer::TK_PR_CLASS: {
				//class inside class :D

				StringName name;
				StringName extends;

				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_IDENTIFIER) {

					_set_error("'class' syntax: 'class <Name>:' or 'class <Name> extends <BaseClass>:'");
					return;
				}
				name = tokenizer->get_token_identifier(1);
				tokenizer->advance(2);

				ClassNode *newclass = alloc_node<ClassNode>();
				newclass->initializer = alloc_node<BlockNode>();
				newclass->initializer->parent_class = newclass;
				newclass->ready = alloc_node<BlockNode>();
				newclass->ready->parent_class = newclass;
				newclass->name = name;
				newclass->owner = p_class;

				p_class->subclasses.push_back(newclass);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_EXTENDS) {

					_parse_extends(newclass);
					if (error_set)
						return;
				}

				if (!_enter_indent_block()) {

					_set_error("Indented block expected.");
					return;
				}
				current_class = newclass;
				_parse_class(newclass);
				current_class = p_class;

			} break;
			/* this is for functions....
			case GDScriptTokenizer::TK_CF_PASS: {

				tokenizer->advance(1);
			} break;
			*/
			case GDScriptTokenizer::TK_PR_STATIC: {
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {

					_set_error("Expected 'func'.");
					return;
				}

			}; //fallthrough to function
			case GDScriptTokenizer::TK_PR_FUNCTION: {

				bool _static = false;
				pending_newline = -1;

				if (tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_STATIC) {

					_static = true;
				}

				tokenizer->advance();
				StringName name;

				if (_get_completable_identifier(COMPLETION_VIRTUAL_FUNC, name)) {
				}

				if (name == StringName()) {

					_set_error("Expected identifier after 'func' (syntax: 'func <identifier>([arguments]):' ).");
					return;
				}

				for (int i = 0; i < p_class->functions.size(); i++) {
					if (p_class->functions[i]->name == name) {
						_set_error("Function '" + String(name) + "' already exists in this class (at line: " + itos(p_class->functions[i]->line) + ").");
					}
				}
				for (int i = 0; i < p_class->static_functions.size(); i++) {
					if (p_class->static_functions[i]->name == name) {
						_set_error("Function '" + String(name) + "' already exists in this class (at line: " + itos(p_class->static_functions[i]->line) + ").");
					}
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {

					_set_error("Expected '(' after identifier (syntax: 'func <identifier>([arguments]):' ).");
					return;
				}

				tokenizer->advance();

				Vector<StringName> arguments;
				Vector<Node *> default_values;
				Vector<DataType> argument_types;
				DataType return_type;

				int fnline = tokenizer->get_token_line();

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
					//has arguments
					bool defaulting = false;
					while (true) {

						if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
							continue;
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_VAR) {

							tokenizer->advance(); //var before the identifier is allowed
						}

						if (!tokenizer->is_token_literal(0, true)) {

							_set_error("Expected identifier for argument.");
							return;
						}

						StringName argname = tokenizer->get_token_identifier();
						arguments.push_back(argname);

						tokenizer->advance();

						DataType datatype;

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
							// Has type
							if (!_parse_type(&datatype)) {
								_set_error("Expected type for function argument");
								return;
							}
							type_check = true;
						}

						argument_types.push_back(datatype);

						if (defaulting && tokenizer->get_token() != GDScriptTokenizer::TK_OP_ASSIGN) {

							_set_error("Default parameter expected.");
							return;
						}

						//tokenizer->advance();

						if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
							defaulting = true;
							tokenizer->advance(1);
							Node *defval = _parse_and_reduce_expression(p_class, _static);
							if (!defval || error_set)
								return;

							OperatorNode *on = alloc_node<OperatorNode>();
							on->op = OperatorNode::OP_ASSIGN;

							IdentifierNode *in = alloc_node<IdentifierNode>();
							in->name = argname;

							on->arguments.push_back(in);
							on->arguments.push_back(defval);
							/* no ..
							if (defval->type!=Node::TYPE_CONSTANT) {

								_set_error("default argument must be constant");
							}
							*/
							default_values.push_back(on);
						}

						while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
							continue;
						} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {

							_set_error("Expected ',' or ')'.");
							return;
						}

						break;
					}
				}

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_FORWARD_ARROW) {
					// Has type
					if (!_parse_type(&return_type, true)) {
						_set_error("Expected type for function return.");
						return;
					}
					type_check = true;
				}

				BlockNode *block = alloc_node<BlockNode>();
				block->parent_class = p_class;

				if (name == "_init") {

					if (p_class->extends_used) {

						OperatorNode *cparent = alloc_node<OperatorNode>();
						cparent->op = OperatorNode::OP_PARENT_CALL;
						block->statements.push_back(cparent);

						IdentifierNode *id = alloc_node<IdentifierNode>();
						id->name = "_init";
						cparent->arguments.push_back(id);

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {
							tokenizer->advance();
							if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
								_set_error("expected '(' for parent constructor arguments.");
							}
							tokenizer->advance();

							if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
								//has arguments
								parenthesis++;
								while (true) {

									Node *arg = _parse_and_reduce_expression(p_class, _static);
									cparent->arguments.push_back(arg);

									if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
										tokenizer->advance();
										continue;
									} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {

										_set_error("Expected ',' or ')'.");
										return;
									}

									break;
								}
								parenthesis--;
							}

							tokenizer->advance();
						}
					} else {

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {

							_set_error("Parent constructor call found for a class without inheritance.");
							return;
						}
					}
				}

				if (!_enter_indent_block(block)) {

					_set_error("Indented block expected.");
					return;
				}

				FunctionNode *function = alloc_node<FunctionNode>();
				function->name = name;
				function->arguments = arguments;
				function->default_values = default_values;
				function->argument_types = argument_types;
				function->return_type = return_type;
				function->_static = _static;
				function->line = fnline;

				function->rpc_mode = rpc_mode;
				rpc_mode = ScriptInstance::RPC_MODE_DISABLED;

				if (_static)
					p_class->static_functions.push_back(function);
				else
					p_class->functions.push_back(function);

				current_function = function;
				function->body = block;
				current_block = block;
				_parse_block(block, _static);
				current_block = NULL;
				current_function = NULL;

				//arguments
			} break;
			case GDScriptTokenizer::TK_PR_SIGNAL: {
				tokenizer->advance();

				if (!tokenizer->is_token_literal()) {
					_set_error("Expected identifier after 'signal'.");
					return;
				}

				ClassNode::Signal sig;
				sig.name = tokenizer->get_token_identifier();
				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					tokenizer->advance();
					while (true) {
						if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
							continue;
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							tokenizer->advance();
							break;
						}

						if (!tokenizer->is_token_literal(0, true)) {
							_set_error("Expected identifier in signal argument.");
							return;
						}

						sig.arguments.push_back(tokenizer->get_token_identifier());
						tokenizer->advance();

						while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
						} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							_set_error("Expected ',' or ')' after signal parameter identifier.");
							return;
						}
					}
				}

				p_class->_signals.push_back(sig);

				if (!_end_statement()) {
					_set_error("Expected end of statement (signal)");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_EXPORT: {

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {

					tokenizer->advance();
					if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE) {

						Variant::Type type = tokenizer->get_token_type();
						if (type == Variant::NIL) {
							_set_error("Can't export null type.");
							return;
						}
						if (type == Variant::OBJECT) {
							_set_error("Can't export raw object type.");
							return;
						}
						current_export.type = type;
						current_export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
						tokenizer->advance();

						String hint_prefix = "";

						if (type == Variant::ARRAY && tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();

							while (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE) {
								type = tokenizer->get_token_type();

								tokenizer->advance();

								if (type == Variant::ARRAY) {
									hint_prefix += itos(Variant::ARRAY) + ":";
									if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
										tokenizer->advance();
									}
								} else {
									hint_prefix += itos(type);
									break;
								}
							}
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							// hint expected next!
							tokenizer->advance();

							switch (type) {

								case Variant::INT: {

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FLAGS") {

										//current_export.hint=PROPERTY_HINT_ALL_FLAGS;
										tokenizer->advance();

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											break;
										}
										if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
											_set_error("Expected ')' or ',' in bit flags hint.");
											return;
										}

										current_export.hint = PROPERTY_HINT_FLAGS;
										tokenizer->advance();

										bool first = true;
										while (true) {

											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {
												current_export = PropertyInfo();
												_set_error("Expected a string constant in named bit flags hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first)
												current_export.hint_string += ",";
											else
												first = false;

											current_export.hint_string += c.xml_escape();

											tokenizer->advance();
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
												break;

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected ')' or ',' in named bit flags hint.");
												return;
											}
											tokenizer->advance();
										}

										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING) {
										//enumeration
										current_export.hint = PROPERTY_HINT_ENUM;
										bool first = true;
										while (true) {

											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {

												current_export = PropertyInfo();
												_set_error("Expected a string constant in enumeration hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first)
												current_export.hint_string += ",";
											else
												first = false;

											current_export.hint_string += c.xml_escape();

											tokenizer->advance();
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
												break;

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected ')' or ',' in enumeration hint.");
												return;
											}

											tokenizer->advance();
										}

										break;
									}

								}; //fallthrough to use the same
								case Variant::REAL: {

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "EASE") {
										current_export.hint = PROPERTY_HINT_EXP_EASING;
										tokenizer->advance();
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected ')' in hint.");
											return;
										}
										break;
									}

									// range
									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "EXP") {

										current_export.hint = PROPERTY_HINT_EXP_RANGE;
										tokenizer->advance();

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
											break;
										else if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
											_set_error("Expected ')' or ',' in exponential range hint.");
											return;
										}
										tokenizer->advance();
									} else
										current_export.hint = PROPERTY_HINT_RANGE;

									float sign = 1.0;

									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										tokenizer->advance();
									}
									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {

										current_export = PropertyInfo();
										_set_error("Expected a range in numeric hint.");
										return;
									}

									current_export.hint_string = rtos(sign * double(tokenizer->get_token_constant()));
									tokenizer->advance();

									if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
										current_export.hint_string = "0," + current_export.hint_string;
										break;
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {

										current_export = PropertyInfo();
										_set_error("Expected ',' or ')' in numeric range hint.");
										return;
									}

									tokenizer->advance();

									sign = 1.0;
									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										tokenizer->advance();
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {

										current_export = PropertyInfo();
										_set_error("Expected a number as upper bound in numeric range hint.");
										return;
									}

									current_export.hint_string += "," + rtos(sign * double(tokenizer->get_token_constant()));
									tokenizer->advance();

									if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
										break;

									if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {

										current_export = PropertyInfo();
										_set_error("Expected ',' or ')' in numeric range hint.");
										return;
									}

									tokenizer->advance();
									sign = 1.0;
									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										tokenizer->advance();
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {

										current_export = PropertyInfo();
										_set_error("Expected a number as step in numeric range hint.");
										return;
									}

									current_export.hint_string += "," + rtos(sign * double(tokenizer->get_token_constant()));
									tokenizer->advance();

								} break;
								case Variant::STRING: {

									if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING) {
										//enumeration
										current_export.hint = PROPERTY_HINT_ENUM;
										bool first = true;
										while (true) {

											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {

												current_export = PropertyInfo();
												_set_error("Expected a string constant in enumeration hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first)
												current_export.hint_string += ",";
											else
												first = false;

											current_export.hint_string += c.xml_escape();
											tokenizer->advance();
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
												break;

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected ')' or ',' in enumeration hint.");
												return;
											}
											tokenizer->advance();
										}

										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "DIR") {

										tokenizer->advance();

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
											current_export.hint = PROPERTY_HINT_DIR;
										else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {

											tokenizer->advance();

											if (tokenizer->get_token() != GDScriptTokenizer::TK_IDENTIFIER || !(tokenizer->get_token_identifier() == "GLOBAL")) {
												_set_error("Expected 'GLOBAL' after comma in directory hint.");
												return;
											}
											if (!p_class->tool) {
												_set_error("Global filesystem hints may only be used in tool scripts.");
												return;
											}
											current_export.hint = PROPERTY_HINT_GLOBAL_DIR;
											tokenizer->advance();

											if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
												_set_error("Expected ')' in hint.");
												return;
											}
										} else {
											_set_error("Expected ')' or ',' in hint.");
											return;
										}
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FILE") {

										current_export.hint = PROPERTY_HINT_FILE;
										tokenizer->advance();

										if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {

											tokenizer->advance();

											if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "GLOBAL") {

												if (!p_class->tool) {
													_set_error("Global filesystem hints may only be used in tool scripts.");
													return;
												}
												current_export.hint = PROPERTY_HINT_GLOBAL_FILE;
												tokenizer->advance();

												if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE)
													break;
												else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA)
													tokenizer->advance();
												else {
													_set_error("Expected ')' or ',' in hint.");
													return;
												}
											}

											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {

												if (current_export.hint == PROPERTY_HINT_GLOBAL_FILE)
													_set_error("Expected string constant with filter");
												else
													_set_error("Expected 'GLOBAL' or string constant with filter");
												return;
											}
											current_export.hint_string = tokenizer->get_token_constant();
											tokenizer->advance();
										}

										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected ')' in hint.");
											return;
										}
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "MULTILINE") {

										current_export.hint = PROPERTY_HINT_MULTILINE_TEXT;
										tokenizer->advance();
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected ')' in hint.");
											return;
										}
										break;
									}
								} break;
								case Variant::COLOR: {

									if (tokenizer->get_token() != GDScriptTokenizer::TK_IDENTIFIER) {

										current_export = PropertyInfo();
										_set_error("Color type hint expects RGB or RGBA as hints");
										return;
									}

									String identifier = tokenizer->get_token_identifier();
									if (identifier == "RGB") {
										current_export.hint = PROPERTY_HINT_COLOR_NO_ALPHA;
									} else if (identifier == "RGBA") {
										//none
									} else {
										current_export = PropertyInfo();
										_set_error("Color type hint expects RGB or RGBA as hints");
										return;
									}
									tokenizer->advance();

								} break;
								default: {

									current_export = PropertyInfo();
									_set_error("Type '" + Variant::get_type_name(type) + "' can't take hints.");
									return;
								} break;
							}
						}
						if (current_export.type == Variant::ARRAY && !hint_prefix.empty()) {
							if (current_export.hint) {
								hint_prefix += "/" + itos(current_export.hint);
							}
							current_export.hint_string = hint_prefix + ":" + current_export.hint_string;
							current_export.hint = PROPERTY_HINT_NONE;
						}

					} else {

						parenthesis++;
						Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
						if (!subexpr) {
							if (_recover_from_completion()) {
								break;
							}
							return;
						}
						parenthesis--;

						if (subexpr->type != Node::TYPE_CONSTANT) {
							current_export = PropertyInfo();
							_set_error("Expected a constant expression.");
						}

						Variant constant = static_cast<ConstantNode *>(subexpr)->value;

						if (constant.get_type() == Variant::OBJECT) {
							GDScriptNativeClass *native_class = Object::cast_to<GDScriptNativeClass>(constant);

							if (native_class && ClassDB::is_parent_class(native_class->get_name(), "Resource")) {
								current_export.type = Variant::OBJECT;
								current_export.hint = PROPERTY_HINT_RESOURCE_TYPE;
								current_export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;

								current_export.hint_string = native_class->get_name();

							} else {
								current_export = PropertyInfo();
								_set_error("Export hint not a resource type.");
							}
						} else if (constant.get_type() == Variant::DICTIONARY) {
							// Enumeration
							bool is_flags = false;

							if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
								tokenizer->advance();

								if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FLAGS") {
									is_flags = true;
									tokenizer->advance();
								} else {
									current_export = PropertyInfo();
									_set_error("Expected 'FLAGS' after comma.");
								}
							}

							current_export.type = Variant::INT;
							current_export.hint = is_flags ? PROPERTY_HINT_FLAGS : PROPERTY_HINT_ENUM;
							Dictionary enum_values = constant;

							List<Variant> keys;
							enum_values.get_key_list(&keys);

							bool first = true;
							for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
								if (enum_values[E->get()].get_type() == Variant::INT) {
									if (!first)
										current_export.hint_string += ",";
									else
										first = false;

									current_export.hint_string += E->get().operator String().camelcase_to_underscore(true).capitalize().xml_escape();
									if (!is_flags) {
										current_export.hint_string += ":";
										current_export.hint_string += enum_values[E->get()].operator String().xml_escape();
									}
								}
							}
						} else {
							current_export = PropertyInfo();
							_set_error("Expected type for export.");
							return;
						}
					}

					if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {

						current_export = PropertyInfo();
						_set_error("Expected ')' or ',' after export hint.");
						return;
					}

					tokenizer->advance();
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_ONREADY && tokenizer->get_token() != GDScriptTokenizer::TK_PR_REMOTE && tokenizer->get_token() != GDScriptTokenizer::TK_PR_MASTER && tokenizer->get_token() != GDScriptTokenizer::TK_PR_SLAVE && tokenizer->get_token() != GDScriptTokenizer::TK_PR_SYNC) {

					current_export = PropertyInfo();
					_set_error("Expected 'var', 'onready', 'remote', 'master', 'slave' or 'sync'.");
					return;
				}

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_ONREADY: {

				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
					_set_error("Expected 'var'.");
					return;
				}

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_REMOTE: {

				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected 'var'.");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected 'var' or 'func'.");
						return;
					}
				}
				rpc_mode = ScriptInstance::RPC_MODE_REMOTE;

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_MASTER: {

				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected 'var'.");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected 'var' or 'func'.");
						return;
					}
				}

				rpc_mode = ScriptInstance::RPC_MODE_MASTER;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_SLAVE: {

				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected 'var'.");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected 'var' or 'func'.");
						return;
					}
				}

				rpc_mode = ScriptInstance::RPC_MODE_SLAVE;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_SYNC: {

				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
					if (current_export.type)
						_set_error("Expected 'var'.");
					else
						_set_error("Expected 'var' or 'func'.");
					return;
				}

				rpc_mode = ScriptInstance::RPC_MODE_SYNC;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_VAR: {
				//variale declaration and (eventual) initialization

				ClassNode::Member member;
				bool autoexport = tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_EXPORT;
				if (current_export.type != Variant::NIL) {
					member._export = current_export;
					current_export = PropertyInfo();
				}

				bool onready = tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_ONREADY;

				tokenizer->advance();
				if (!tokenizer->is_token_literal(0, true)) {

					_set_error("Expected identifier for member variable name.");
					return;
				}

				member.identifier = tokenizer->get_token_literal();
				member.expression = NULL;
				member._export.name = member.identifier;
				member.line = tokenizer->get_token_line();
				member.rpc_mode = rpc_mode;

				tokenizer->advance();

				rpc_mode = ScriptInstance::RPC_MODE_DISABLED;

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					// Has type
					if (!_parse_type(&member.data_type)) {
						_set_error("Expected member variable type.");
						return;
					}
					type_check = true;
				}

				// Export uses member explicit type by default
				if (autoexport && member.data_type.has_type) {
					member._export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
					member._export.type = member.data_type.variant_type;

					// If the type is an object, look at the class db to see if it's a resource
					if (member.data_type.variant_type == Variant::OBJECT) {
						if (!ClassDB::is_parent_class(member.data_type.class_name, "Resource")) {
							_set_error("Exported type is not a built-in type or resource.");
							return;
						}
						member._export.hint = PROPERTY_HINT_RESOURCE_TYPE;
						member._export.hint_string = member.data_type.class_name;
					}
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {

#ifdef DEBUG_ENABLED
					int line = tokenizer->get_token_line();
#else
					int line = 0;
#endif
					tokenizer->advance();

					Node *subexpr = _parse_and_reduce_expression(p_class, false, autoexport);
					if (!subexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}

					//discourage common error
					if (!onready && subexpr->type == Node::TYPE_OPERATOR) {

						OperatorNode *op = static_cast<OperatorNode *>(subexpr);
						if (op->op == OperatorNode::OP_CALL && op->arguments[0]->type == Node::TYPE_SELF && op->arguments[1]->type == Node::TYPE_IDENTIFIER) {
							IdentifierNode *id = static_cast<IdentifierNode *>(op->arguments[1]);
							if (id->name == "get_node") {

								_set_error("Use 'onready var " + String(member.identifier) + " = get_node(..)' instead");
								return;
							}
						}
					}

					member.expression = subexpr;

					if (autoexport && !member.data_type.has_type) {
						if (subexpr->type != Node::TYPE_CONSTANT) {

							_set_error("Type-less export needs a constant expression assigned to infer type.");
							return;
						}

						ConstantNode *cn = static_cast<ConstantNode *>(subexpr);
						if (cn->value.get_type() == Variant::NIL) {

							_set_error("Can't accept a null constant expression for inferring export type.");
							return;
						}
						member._export.type = cn->value.get_type();
						member._export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
						if (cn->value.get_type() == Variant::OBJECT) {
							Object *obj = cn->value;
							Resource *res = Object::cast_to<Resource>(obj);
							if (res == NULL) {
								_set_error("Exported constant not a type or resource.");
								return;
							}
							member._export.hint = PROPERTY_HINT_RESOURCE_TYPE;
							member._export.hint_string = res->get_class();
						}
					}
#ifdef TOOLS_ENABLED
					if (member.data_type.has_type) {
						// Construct default value if there's a type but nothing was assigned
						Variant::CallError err;
						member.default_value = Variant::construct(member.data_type.variant_type, NULL, 0, err);
					}
					if (subexpr->type == Node::TYPE_CONSTANT && member._export.type != Variant::NIL) {

						ConstantNode *cn = static_cast<ConstantNode *>(subexpr);
						if (cn->value.get_type() != Variant::NIL) {
							member.default_value = cn->value;
						}
						cn->constant_type.has_type = true;
						cn->constant_type.variant_type = cn->value.get_type();
					}
#endif

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = member.identifier;

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_INIT_ASSIGN;
					op->arguments.push_back(id);
					op->arguments.push_back(subexpr);

#ifdef DEBUG_ENABLED
					NewLineNode *nl = alloc_node<NewLineNode>();
					nl->line = line;
					if (onready)
						p_class->ready->statements.push_back(nl);
					else
						p_class->initializer->statements.push_back(nl);
#endif
					if (onready)
						p_class->ready->statements.push_back(op);
					else
						p_class->initializer->statements.push_back(op);

				} else {

					if (autoexport && !member.data_type.has_type) {

						_set_error("Type-less export needs a constant expression assigned to infer type.");
						return;
					}
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_SETGET) {

					tokenizer->advance();

					if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
						//just comma means using only getter
						if (!tokenizer->is_token_literal()) {
							_set_error("Expected identifier for setter function after 'setget'.");
						}

						member.setter = tokenizer->get_token_literal();

						tokenizer->advance();
					}

					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
						//there is a getter
						tokenizer->advance();

						if (!tokenizer->is_token_literal()) {
							_set_error("Expected identifier for getter function after ','.");
						}

						member.getter = tokenizer->get_token_literal();
						tokenizer->advance();
					}
				}

				p_class->variables.push_back(member);

				if (!_end_statement()) {
					_set_error("Expected end of statement (continue)");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_CONST: {
				//variale declaration and (eventual) initialization

				ClassNode::Constant constant;

				tokenizer->advance();
				if (!tokenizer->is_token_literal(0, true)) {

					_set_error("Expected name (identifier) for constant.");
					return;
				}

				constant.identifier = tokenizer->get_token_literal();
				constant.line = tokenizer->get_token_line();
				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					// Has type
					if (!_parse_type(&constant.data_type)) {
						_set_error("Expected type for constant.");
						return;
					}
					type_check = true;
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_OP_ASSIGN) {
					_set_error("Constant expects assignment.");
					return;
				}

				tokenizer->advance();

				Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
				if (!subexpr) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				if (subexpr->type != Node::TYPE_CONSTANT) {
					_set_error("Expected constant expression.");
				}

				constant.expression = subexpr;

				p_class->constant_expressions.push_back(constant);

				if (!_end_statement()) {
					_set_error("Expected end of statement (constant).");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_PR_ENUM: {
				//multiple constant declarations..

				int last_assign = -1; // Incremented by 1 right before the assingment.
				String enum_name;
				Dictionary enum_dict;

				tokenizer->advance();
				if (tokenizer->is_token_literal(0, true)) {
					enum_name = tokenizer->get_token_literal();
					tokenizer->advance();
				}
				if (tokenizer->get_token() != GDScriptTokenizer::TK_CURLY_BRACKET_OPEN) {
					_set_error("Expected '{' in enum declaration");
					return;
				}
				tokenizer->advance();

				while (true) {
					if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {

						tokenizer->advance(); // Ignore newlines
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {

						tokenizer->advance();
						break; // End of enum
					} else if (!tokenizer->is_token_literal(0, true)) {

						if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
							_set_error("Unexpected end of file.");
						} else {
							_set_error(String("Unexpected ") + GDScriptTokenizer::get_token_name(tokenizer->get_token()) + ", expected identifier");
						}

						return;
					} else { // tokenizer->is_token_literal(0, true)
						ClassNode::Constant constant;

						constant.identifier = tokenizer->get_token_literal();

						tokenizer->advance();

						if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
							tokenizer->advance();

							Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
							if (!subexpr) {
								if (_recover_from_completion()) {
									break;
								}
								return;
							}

							if (subexpr->type != Node::TYPE_CONSTANT) {
								_set_error("Expected constant expression");
							}

							const ConstantNode *subexpr_const = static_cast<const ConstantNode *>(subexpr);

							if (subexpr_const->value.get_type() != Variant::INT) {
								_set_error("Expected an int value for enum");
							}

							last_assign = subexpr_const->value;

							constant.expression = subexpr;

						} else {
							last_assign = last_assign + 1;
							ConstantNode *cn = alloc_node<ConstantNode>();
							cn->value = last_assign;
							cn->constant_type.has_type = true;
							cn->constant_type.variant_type = Variant::INT;
							constant.expression = cn;
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
						}

						// Enum constants are always integers
						constant.data_type.has_type = true;
						constant.data_type.variant_type = Variant::INT;

						if (enum_name != "") {
							const ConstantNode *cn = static_cast<const ConstantNode *>(constant.expression);
							enum_dict[constant.identifier] = cn->value;
						}

						p_class->constant_expressions.push_back(constant);
					}
				}

				if (enum_name != "") {
					ClassNode::Constant enum_constant;
					enum_constant.identifier = enum_name;
					enum_constant.data_type.has_type = true;
					enum_constant.data_type.variant_type = Variant::DICTIONARY;
					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = enum_dict;
					cn->constant_type.has_type = true;
					cn->constant_type.variant_type = Variant::DICTIONARY;
					enum_constant.expression = cn;
					p_class->constant_expressions.push_back(enum_constant);
				}

				if (!_end_statement()) {
					_set_error("Expected end of statement (enum)");
					return;
				}

			} break;

			case GDScriptTokenizer::TK_CONSTANT: {
				if (tokenizer->get_token_constant().get_type() == Variant::STRING) {
					tokenizer->advance();
					// Ignore
				} else {
					_set_error(String() + "Unexpected constant of type: " + Variant::get_type_name(tokenizer->get_token_constant().get_type()));
					return;
				}
			} break;

			default: {

				_set_error(String() + "Unexpected token: " + tokenizer->get_token_name(tokenizer->get_token()) + ":" + tokenizer->get_token_identifier());
				return;

			} break;
		}
	}
}

bool GDScriptParser::_parse_type(DataType *r_datatype, bool p_can_be_void) {
	tokenizer->advance();

	r_datatype->has_type = true;

	switch (tokenizer->get_token()) {
		// No string typing for now
		// Would be same logic as `extends` but doesn't look good
#if 0
		case GDScriptTokenizer::TK_CONSTANT: {
			Variant constant = tokenizer->get_token_constant();
			if (constant.get_type() != Variant::STRING) {

				_set_error("Type constrain must be a string.");
				return;
			}

			r_datatype->base_type = StringName(constant);
			r_datatype->variant_type = Variant::OBJECT;
		} break;
#endif
		case GDScriptTokenizer::TK_IDENTIFIER: {
			r_datatype->class_name = tokenizer->get_token_identifier();
			if (r_datatype->class_name == "void") {
				if (!p_can_be_void) {
					return false;
				} else {
					r_datatype->variant_type = Variant::NIL;
				}
			} else {
				r_datatype->variant_type = Variant::OBJECT;
			}
		} break;
		case GDScriptTokenizer::TK_BUILT_IN_TYPE: {
			r_datatype->variant_type = tokenizer->get_token_type();
		} break;
		default: {
			return false;
		}
	}

	tokenizer->advance();
	return true;
}

void GDScriptParser::_check_class_types(ClassNode *p_class) {

	// Constants
	for (int i = 0; i < p_class->constant_expressions.size(); i++) {
		ClassNode::Constant &c = p_class->constant_expressions[i];

		if (!_is_type_compatible(c.data_type, c.expression->get_datatype())) {
			_set_error("Assigned value type (" + _get_type_string(c.expression->get_datatype()) +
							   ") doesn't match the constant's type (" + _get_type_string(c.data_type) + ").",
					c.line);
			return;
		}
	}

	// Members
	for (int i = 0; i < p_class->variables.size(); i++) {
		ClassNode::Member &v = p_class->variables[i];
		if (!v.data_type.has_type) continue; // Don't bother with non-typed ones

		_check_variable_assign_type(v, v.expression);
		if (error_set) return;

		// Check the export hint
		if (v._export.type != Variant::NIL) {
			DataType export_type;
			export_type.has_type = true;
			export_type.variant_type = v._export.type;
			export_type.class_name = v._export.class_name;

			if (!_is_type_compatible(v.data_type, export_type)) {
				_set_error("Export hint type (" + _get_type_string(export_type) +
								   ") does not match the variable type (" + _get_type_string(v.data_type) + ").",
						v.line);
				return;
			}
		}

		// Check the setter and getter
		if (v.setter == StringName() && v.getter == StringName()) continue;

		bool found_getter = false;
		bool found_setter = false;
		for (int j = 0; j < p_class->functions.size(); j++) {
			if (v.setter == p_class->functions[j]->name) {
				found_setter = true;
				FunctionNode *setter = p_class->functions[j];

				if (setter->arguments.size() != 1) {
					_set_error("Setter function needs to receive exactly 1 argument. See " + setter->name + " definition at line " + itos(setter->line) + ".", v.line);
					return;
				}
				if (!_is_type_compatible(v.data_type, setter->argument_types[0])) {
					_set_error("Setter argument type (" + _get_type_string(setter->argument_types[0]) + ") doesn't match the variable's type (" + _get_type_string(v.data_type) + "). See " + setter->name + " definition at line " + itos(setter->line) + ".", v.line);
					return;
				}
				continue;
			}
			if (v.getter == p_class->functions[j]->name) {
				found_getter = true;
				FunctionNode *getter = p_class->functions[j];

				if (getter->arguments.size() != 0) {
					_set_error("Getter function can't receive arguments. See " + getter->name + " definition at line " + itos(getter->line) + ".", v.line);
					return;
				}
				if (!_is_type_compatible(v.data_type, getter->get_datatype())) {
					_set_error("Getter return type (" + _get_type_string(getter->get_datatype()) + ") doesn't match the variable's type (" + _get_type_string(v.data_type) + "). See " + getter->name + " definition at line " + itos(getter->line) + ".", v.line);
					return;
				}
			}
			if (found_getter && found_setter) break;
		}

		if ((found_getter || v.getter == StringName()) && (found_setter || v.setter == StringName())) continue;

		// Check for static functions
		for (int j = 0; j < p_class->static_functions.size(); j++) {
			if (v.setter == p_class->static_functions[j]->name) {
				FunctionNode *setter = p_class->static_functions[j];
				_set_error("Setter can't be a static function. See " + setter->name + " definition at line " + itos(setter->line) + ".", v.line);
				return;
			}
			if (v.getter == p_class->static_functions[j]->name) {
				FunctionNode *getter = p_class->static_functions[j];
				_set_error("Getter can't be a static function. See " + getter->name + " definition at line " + itos(getter->line) + ".", v.line);
				return;
			}
		}

		if (!found_setter && v.setter != StringName()) {
			_set_error("Setter function is not defined.", v.line);
			return;
		}

		if (!found_getter && v.getter != StringName()) {
			_set_error("Getter function is not defined.", v.line);
			return;
		}
	}

	// Inner classes
	for (int i = 0; i < p_class->subclasses.size(); i++) {
		current_class = p_class->subclasses[i];
		_check_class_types(current_class);
		if (error_set) return;
		current_class = p_class;
	}

	// Function blocks
	for (int i = 0; i < p_class->static_functions.size(); i++) {
		_check_function_types(p_class->static_functions[i]);
		if (error_set) return;
	}
	for (int i = 0; i < p_class->functions.size(); i++) {
		_check_function_types(p_class->functions[i]);
		if (error_set) return;
	}
}

void GDScriptParser::_check_function_types(FunctionNode *p_function) {

	// Arguments
	int defaults_ofs = p_function->arguments.size() - p_function->default_values.size();
	for (int i = defaults_ofs; i < p_function->arguments.size(); i++) {
		ClassNode::Member fake_member;
		fake_member.data_type = p_function->argument_types[i];
		fake_member.identifier = p_function->arguments[i];
		fake_member.line = p_function->line;

		if (p_function->default_values[i - defaults_ofs]->type != Node::TYPE_OPERATOR) {
			_set_error("Parser bug: invalid argument default value.", p_function->line, p_function->column);
			return;
		}

		OperatorNode *op = static_cast<OperatorNode *>(p_function->default_values[i - defaults_ofs]);

		if (op->op != OperatorNode::OP_ASSIGN || op->arguments.size() != 2) {
			_set_error("Parser bug: invalid argument default value operation.", p_function->line, p_function->column);
			return;
		}

		_check_variable_assign_type(fake_member, op->arguments[1]);
		if (error_set) return;
	}

	// Body
	current_function = p_function;
	current_block = current_function->body;
	_check_block_types(p_function->body);
	if (error_set) return;
	current_block = NULL;
	current_function = NULL;
}

void GDScriptParser::_check_block_types(BlockNode *p_block) {

	// Check each statement
	for (List<Node *>::Element *E = p_block->statements.front(); E; E = E->next()) {
		Node *statement = E->get();

		switch (statement->type) {
			case GDScriptParser::Node::TYPE_LOCAL_VAR: {
				LocalVarNode *lv = static_cast<LocalVarNode *>(statement);

				// Fake member to simpler function re-use
				ClassNode::Member fake_member;
				fake_member.data_type = lv->get_datatype();
				fake_member.identifier = lv->name;
				fake_member.line = lv->line;

				_check_variable_assign_type(fake_member, lv->assign);

				if (error_set) return;
			} break;
			case GDScriptParser::Node::TYPE_OPERATOR: {
				OperatorNode *op = static_cast<OperatorNode *>(statement);

				switch (op->op) {
					case OperatorNode::OP_ASSIGN:
					case OperatorNode::OP_ASSIGN_ADD:
					case OperatorNode::OP_ASSIGN_SUB:
					case OperatorNode::OP_ASSIGN_MUL:
					case OperatorNode::OP_ASSIGN_DIV:
					case OperatorNode::OP_ASSIGN_MOD:
					case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
					case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
					case OperatorNode::OP_ASSIGN_BIT_AND:
					case OperatorNode::OP_ASSIGN_BIT_OR:
					case OperatorNode::OP_ASSIGN_BIT_XOR: {
						if (op->arguments.size() < 2) {
							_set_error("Parser bug: operation without enough arguments.", op->line, op->column);
							return;
						}
						if (op->arguments[0]->type != GDScriptParser::Node::TYPE_IDENTIFIER) break;

						IdentifierNode *id = static_cast<IdentifierNode *>(op->arguments[0]);
						bool is_constant = false;
						DataType id_type = _lookup_identifier_type(id->name, op->line, is_constant);

						if (is_constant) {
							_set_error("Cannot assign value to constant.", op->line, op->column);
							return;
						}

						if (op->op != OperatorNode::OP_ASSIGN) {
							// Check if operation is valid
							DataType argument_type = _lookup_node_type(op->arguments[1], op->line);
							if (!argument_type.has_type) {
								return;
							}
							Variant::Operator var_op = _get_variant_operation(op->op);
							bool valid = false;
							Variant::Type ret_type = _get_operation_type(var_op, id_type.variant_type, argument_type.variant_type, valid);

							if (!valid) {
								_set_error("Invalid operand types ('" + _get_type_string(id_type) + "' and '" +
												   _get_type_string(argument_type) + "') to assignment operator '" + Variant::get_operator_name(var_op) +
												   "'.",
										op->line, op->column);
								return;
							}

							DataType exp_type;
							exp_type.has_type = true;
							exp_type.variant_type = ret_type;

							if (!_is_type_compatible(id_type, exp_type)) {
								_set_error("Assigned expression type (" + _get_type_string(exp_type) + ") doesn't match variable's type (" +
												   _get_type_string(id_type) + ").",
										op->line);
								return;
							}

						} else {
							ClassNode::Member fake_member;
							fake_member.data_type = id_type;
							fake_member.identifier = id->name;
							fake_member.line = id->line;
							_check_variable_assign_type(fake_member, op->arguments[1]);
						}

						if (error_set) return;

					} break;
					case OperatorNode::OP_CALL: {
						_check_call_args_types(op);
						if (error_set) return;
					}
				}
			} break;
			case GDScriptParser::Node::TYPE_CONTROL_FLOW: {
				ControlFlowNode *cf = static_cast<ControlFlowNode *>(statement);

				switch (cf->cf_type) {
					case ControlFlowNode::CF_RETURN: {
						DataType function_type = current_function->get_datatype();

						if (!function_type.has_type) break;

						if (function_type.variant_type == Variant::NIL) {
							// Return void, should not have arguments
							if (cf->arguments.size() > 0) {
								_set_error("Void function cannot return a value.", cf->line, cf->column);
								return;
							}
						} else {
							// Return something, cannot be empty
							if (cf->arguments.size() == 0) {
								_set_error("Function must return a value.", cf->line, cf->column);
								return;
							}

							DataType ret_type = _lookup_node_type(cf->arguments[0], cf->line);
							if (!_is_type_compatible(function_type, ret_type)) {
								_set_error("Returned value type (" + _get_type_string(ret_type) + ") doesn't match the function return type (" +
												   _get_type_string(function_type) + ").",
										cf->line, cf->column);
								return;
							}
						}
					} break;
				}
			} break;
		}
	}

	// Check sub-blocks
	for (List<BlockNode *>::Element *E = p_block->sub_blocks.front(); E; E = E->next()) {
		BlockNode *sub_block = E->get();
		current_block = sub_block;
		_check_block_types(sub_block);
		if (error_set) return;
		current_block = p_block;
	}
}

void GDScriptParser::_check_variable_assign_type(const ClassNode::Member &p_var, Node *p_assign) {
	if (!p_assign) return;

	DataType rh_type = _lookup_node_type(p_assign, p_var.line);

	if (!_is_type_compatible(p_var.data_type, rh_type)) {
		_set_error("Assigned expression type (" + _get_type_string(rh_type) + ") doesn't match variable's type (" +
						   _get_type_string(p_var.data_type) + ").",
				p_var.line);
		return;
	}
}

void GDScriptParser::_check_call_args_types(OperatorNode *p_call) {

	// Should never be called without at least one argument
	switch (p_call->arguments[0]->type) {
		case Node::TYPE_BUILT_IN_FUNCTION: {
			BuiltInFunctionNode *func = static_cast<BuiltInFunctionNode *>(p_call->arguments[0]);
			MethodInfo mi = GDScriptFunctions::get_info(func->function);

			// No checking for varargs (for now)
			if (mi.flags & METHOD_FLAG_VARARG) break;

			if (p_call->arguments.size() - 1 < mi.arguments.size() - mi.default_arguments.size()) {
				_set_error("Too few arguments for " + mi.name + "() call. Expected at least " + itos(mi.arguments.size() - mi.default_arguments.size()) + ".", p_call->line);
				return;
			}
			if (p_call->arguments.size() - 1 > mi.arguments.size()) {
				_set_error("Too many arguments for " + mi.name + "() call. Expected at most " + itos(mi.arguments.size()) + ".", p_call->line);
				return;
			}

			for (int i = 1; i < p_call->arguments.size(); i++) {
				DataType arg_type;
				arg_type.has_type = true;
				arg_type.variant_type = mi.arguments[i - 1].type;
				arg_type.class_name = mi.arguments[i - 1].class_name;

				DataType par_type = _lookup_node_type(p_call->arguments[i], p_call->line);

				if (!_is_type_compatible(arg_type, par_type)) {
					_set_error("At " + mi.name + "() call, argument " + itos(i) + ". Assigned type (" +
									   _get_type_string(par_type) + ") doesn't match the function argument's type (" + _get_type_string(arg_type) + ").",
							p_call->line);
					return;
				}
			}
		} break;
		case Node::TYPE_TYPE: {
			// Built-in constructor
			TypeNode *tn = static_cast<TypeNode *>(p_call->arguments[0]);

			Vector<DataType> par_types;
			par_types.resize(p_call->arguments.size() - 1);
			for (int i = 1; i < p_call->arguments.size(); i++) {
				par_types[i - 1] = _lookup_node_type(p_call->arguments[i], p_call->line);
			}

			if (error_set) return;

			bool match = false;
			List<MethodInfo> constructors;
			Variant::get_constructor_list(tn->vtype, &constructors);

			for (List<MethodInfo>::Element *E = constructors.front(); E; E = E->next()) {
				MethodInfo &mi = E->get();

				if (p_call->arguments.size() - 1 < mi.arguments.size() - mi.default_arguments.size()) {
					continue;
				}
				if (p_call->arguments.size() - 1 > mi.arguments.size()) {
					continue;
				}

				bool types_match = true;
				for (int i = 0; i < par_types.size(); i++) {
					DataType arg_type;
					arg_type.has_type = true;
					arg_type.variant_type = mi.arguments[i].type;
					arg_type.class_name = mi.arguments[i].class_name;

					if (!_is_type_compatible(arg_type, par_types[i])) {
						types_match = false;
						break;
					}
				}

				if (types_match) {
					match = true;
					break;
				}
			}

			if (!match) {
				String err = "No constructor of ";
				err += Variant::get_type_name(tn->vtype);
				err += " matches the signature ";
				err += Variant::get_type_name(tn->vtype) + "(";
				for (int i = 0; i < par_types.size(); i++) {
					if (i > 0) err += ", ";
					err += _get_type_string(par_types[i]);
				}
				err += ").";
				_set_error(err, p_call->line, p_call->column);
				return;
			}
		} break;
		case Node::TYPE_SELF: {
			switch (p_call->arguments[1]->type) {
				case Node::TYPE_IDENTIFIER: {
					IdentifierNode *id = static_cast<IdentifierNode *>(p_call->arguments[1]);

					bool found = false;

					for (int i = 0; i < current_class->static_functions.size(); i++) {
						FunctionNode *func = current_class->static_functions[i];
						if (id->name == func->name) {
							found = true;

							this->_check_func_node_args_types(p_call, func);
							if (error_set) return;
							break;
						}
					}

					if (found) break;

					for (int i = 0; i < current_class->functions.size(); i++) {
						FunctionNode *func = current_class->functions[i];
						if (id->name == func->name) {
							found = true;

							if (current_function && current_function->_static) {
								_set_error("Can't call non-static function " + func->name + " from the body of a static function", p_call->line, p_call->column);
								return;
							}

							this->_check_func_node_args_types(p_call, func);
							if (error_set) return;

							break;
						}
					}

				} break;
			}
		} break;
	}
}

void GDScriptParser::_check_func_node_args_types(OperatorNode *p_call, FunctionNode *p_func) {
	if (p_call->arguments.size() - 2 < p_func->arguments.size() - p_func->default_values.size()) {
		_set_error("Too few arguments for " + p_func->name + "() call. Expected at least " + itos(p_func->arguments.size() - p_func->default_values.size()) + ".", p_call->line);
		return;
	}
	if (p_call->arguments.size() - 2 > p_func->arguments.size()) {
		_set_error("Too many arguments for " + p_func->name + "() call. Expected at most " + itos(p_func->arguments.size()) + ".", p_call->line);
		return;
	}
	for (int i = 2; i < p_call->arguments.size(); i++) {
		DataType arg_type = p_func->argument_types[i - 2];
		DataType par_type = _lookup_node_type(p_call->arguments[i], p_call->line);

		if (!_is_type_compatible(arg_type, par_type)) {
			_set_error("At " + p_func->name + "() call, argument \"" + String(p_func->arguments[i - 2]) + "\". Assigned type (" +
							   _get_type_string(par_type) + ") doesn't match the function argument's type (" + _get_type_string(arg_type) + ").",
					p_call->line);
			return;
		}
	}
}

GDScriptParser::DataType GDScriptParser::_lookup_node_type(Node *p_node, int p_line) {

	// Early out if type is already defined
	if (p_node->get_datatype().has_type) {
		return p_node->get_datatype();
	}

	switch (p_node->type) {
		case GDScriptParser::Node::TYPE_CONSTANT:
		case GDScriptParser::Node::TYPE_ARRAY:
		case GDScriptParser::Node::TYPE_DICTIONARY: {
			return p_node->get_datatype();
		} break;
		case GDScriptParser::Node::TYPE_IDENTIFIER: {
			IdentifierNode *id = static_cast<IdentifierNode *>(p_node);
			return _lookup_identifier_type(id->name, p_line);
		} break;
		case GDScriptParser::Node::TYPE_OPERATOR: {
			OperatorNode *op = static_cast<OperatorNode *>(p_node);

			switch (op->op) {
			}

			switch (op->op) {
				case OperatorNode::OP_CALL: {
					if (op->arguments.size() < 1) {
						_set_error("Parser bug: function call without enough arguments.", p_line);
						ERR_FAIL_V(DataType());
					}
					// While this is a "lookup", this operation won't be tested again
					// So it's the only place to check the call arguments
					// It's also recursive, which can be good or bad
					// TODO: Find a better time to check this
					_check_call_args_types(op);
					if (error_set) return DataType();

					switch (op->arguments[0]->type) {
						case GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION: {
							BuiltInFunctionNode *func = static_cast<BuiltInFunctionNode *>(op->arguments[0]);
							MethodInfo mi = GDScriptFunctions::get_info(func->function);

							DataType func_type;
							func_type.has_type = true;
							func_type.variant_type = mi.return_val.type;
							func_type.class_name = mi.return_val.name;

							return func_type;
						} break;
						case GDScriptParser::Node::TYPE_TYPE: {
							// Built-in constructor
							return op->arguments[0]->get_datatype();
						} break;
						case GDScriptParser::Node::TYPE_SELF: {
							// Look at current class functions
							// TODO: Look at inherited functions

							if (op->arguments.size() < 2) {
								_set_error("Parser bug: self method call without enough arguments.", p_line);
								ERR_FAIL_V(DataType());
							}

							IdentifierNode *func_id = static_cast<IdentifierNode *>(op->arguments[1]);

							for (int i = 0; i < current_class->static_functions.size(); i++) {
								FunctionNode *func = current_class->static_functions[i];
								if (func_id->name == func->name) {
									return func->get_datatype();
								}
							}
							for (int i = 0; i < current_class->functions.size(); i++) {
								FunctionNode *func = current_class->functions[i];
								if (func_id->name == func->name) {
									return func->get_datatype();
								}
							}
						} break;
					}

				} break;
				// Unary operators
				case OperatorNode::OP_NEG:
				case OperatorNode::OP_POS:
				case OperatorNode::OP_NOT:
				case OperatorNode::OP_BIT_INVERT: {

					DataType argument_type = _lookup_node_type(op->arguments[0], p_line);
					if (!argument_type.has_type) {
						break;
					}

					Variant::Operator var_op = _get_variant_operation(op->op);
					bool valid = false;
					Variant::Type ret_type = _get_operation_type(var_op, argument_type.variant_type, argument_type.variant_type, valid);

					if (!valid) {
						_set_error("Invalid operand type ('" + _get_type_string(argument_type) +
										   "') to operator '" + Variant::get_operator_name(var_op) + "'.",
								op->line, op->column);
						return DataType();
					}

					DataType return_type;
					return_type.has_type = true;
					return_type.variant_type = ret_type;

					return return_type;

				} break;
				// Binary operators
				case OperatorNode::OP_IN:
				case OperatorNode::OP_EQUAL:
				case OperatorNode::OP_NOT_EQUAL:
				case OperatorNode::OP_LESS:
				case OperatorNode::OP_LESS_EQUAL:
				case OperatorNode::OP_GREATER:
				case OperatorNode::OP_GREATER_EQUAL:
				case OperatorNode::OP_AND:
				case OperatorNode::OP_OR:
				case OperatorNode::OP_ADD:
				case OperatorNode::OP_SUB:
				case OperatorNode::OP_MUL:
				case OperatorNode::OP_DIV:
				case OperatorNode::OP_MOD:
				case OperatorNode::OP_SHIFT_LEFT:
				case OperatorNode::OP_SHIFT_RIGHT:
				case OperatorNode::OP_BIT_AND:
				case OperatorNode::OP_BIT_OR:
				case OperatorNode::OP_BIT_XOR: {

					DataType argument_a_type = _lookup_node_type(op->arguments[0], p_line);
					DataType argument_b_type = _lookup_node_type(op->arguments[1], p_line);
					if (!argument_a_type.has_type || !argument_b_type.has_type) {
						break;
					}

					Variant::Operator var_op = _get_variant_operation(op->op);
					bool valid = false;
					Variant::Type ret_type = _get_operation_type(var_op, argument_a_type.variant_type, argument_b_type.variant_type, valid);

					if (!valid) {
						_set_error("Invalid operand types ('" + _get_type_string(argument_a_type) + "' and '" +
										   _get_type_string(argument_b_type) + "') to operator '" + Variant::get_operator_name(var_op) +
										   "'.",
								op->line, op->column);
						return DataType();
					}

					DataType return_type;
					return_type.has_type = true;
					return_type.variant_type = ret_type;

					return return_type;

				} break;
				// Assignment should never happen within an expression
				case OperatorNode::OP_ASSIGN:
				case OperatorNode::OP_ASSIGN_ADD:
				case OperatorNode::OP_ASSIGN_SUB:
				case OperatorNode::OP_ASSIGN_MUL:
				case OperatorNode::OP_ASSIGN_DIV:
				case OperatorNode::OP_ASSIGN_MOD:
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
				case OperatorNode::OP_ASSIGN_BIT_AND:
				case OperatorNode::OP_ASSIGN_BIT_OR:
				case OperatorNode::OP_ASSIGN_BIT_XOR: {

					_set_error("Assignment inside expression is not allowed.", op->line);
					return DataType();

				} break;
			}
		} break;
	}
	return DataType();
}

GDScriptParser::DataType GDScriptParser::_lookup_identifier_type(const StringName &p_identifier, int p_line) const {
	bool tmp;
	return _lookup_identifier_type(p_identifier, p_line, tmp);
}

GDScriptParser::DataType GDScriptParser::_lookup_identifier_type(const StringName &p_identifier, int p_line, bool &r_is_constant) const {
	bool in_function = !!current_function;

	r_is_constant = false;

	// Look for current context first, as they might shadow global things
	if (in_function) {
		// Check for function arguments
		for (int i = 0; i < current_function->arguments.size(); i++) {
			if (p_identifier == current_function->arguments[i]) {
				return current_function->argument_types[i];
			}
		}

		// Check for variables in current context
		BlockNode *bln = current_block;
		while (bln) {
			for (int i = 0; i < bln->variables.size(); i++) {
				if (bln->variable_lines[i] > p_line) {
					// Ignore local variable defined after the assignment
					// TODO: Check if the name is defined at all, otherwise error out(?)
					break;
				}
				if (p_identifier == bln->variables[i]) {
					return bln->variable_types[i];
				}
			}
			bln = bln->parent_block;
		}
	}

	// Check for constants
	for (int i = 0; i < current_class->constant_expressions.size(); i++) {
		ClassNode::Constant &c = current_class->constant_expressions[i];
		if (p_identifier == c.identifier) {
			r_is_constant = true;
			return c.data_type;
		}
	}

	// Check for class variables
	for (int i = 0; i < current_class->variables.size(); i++) {
		ClassNode::Member &v = current_class->variables[i];
		if (p_identifier == v.identifier) {
			return v.data_type;
		}
	}

	return DataType();
}

bool GDScriptParser::_is_type_compatible(const DataType &p_container_type, const DataType &p_expression_type) {
	// Defaults to true since if the check isn't possible should be
	// assumed to be until it fails
	bool is_compatible = true;

	if (!p_container_type.has_type || !p_expression_type.has_type) {
		// Datatypes aren't detected by the parser
		return is_compatible;
	}

	is_compatible = p_container_type.variant_type == p_expression_type.variant_type;

	if (p_container_type.variant_type == Variant::OBJECT && p_expression_type.variant_type == Variant::NIL) {
		// Object variable can have Nil, but not the other way around
		return true;
	}

	if (is_compatible && p_container_type.variant_type == Variant::OBJECT) {
		// Check ClassDB for compatibility
		if (ClassDB::class_exists(p_container_type.class_name) && ClassDB::class_exists(p_expression_type.class_name)) {
			is_compatible = ClassDB::is_parent_class(p_expression_type.class_name, p_container_type.class_name);
		}
		// TODO: Check types from scripts
	}

	return is_compatible;
}

String GDScriptParser::_get_type_string(const DataType &p_type) const {
	if (!p_type.has_type) {
		return "Variant";
	}
	if (p_type.variant_type == Variant::OBJECT) {
		return String(p_type.class_name);
	}
	return Variant::get_type_name(p_type.variant_type);
}

Variant::Type GDScriptParser::_get_operation_type(const Variant::Operator p_op, const Variant::Type p_a, const Variant::Type p_b, bool &r_valid) {
	Variant::CallError err;
	Variant a = Variant::construct(p_a, NULL, 0, err);
	if (err.error != Variant::CallError::CALL_OK) {
		r_valid = false;
		return Variant::NIL;
	}
	Variant b = Variant::construct(p_b, NULL, 0, err);
	if (err.error != Variant::CallError::CALL_OK) {
		r_valid = false;
		return Variant::NIL;
	}

	Variant ret;
	Variant::evaluate(p_op, a, b, ret, r_valid);

	if (r_valid) {
		return ret.get_type();
	} else {
		return Variant::NIL;
	}
}

Variant::Operator GDScriptParser::_get_variant_operation(const OperatorNode::Operator &p_op) {
	switch (p_op) {
		case OperatorNode::OP_NEG: {
			return Variant::OP_NEGATE;
		} break;
		case OperatorNode::OP_POS: {
			return Variant::OP_POSITIVE;
		} break;
		case OperatorNode::OP_NOT: {
			return Variant::OP_NOT;
		} break;
		case OperatorNode::OP_BIT_INVERT: {
			return Variant::OP_BIT_NEGATE;
		} break;
		case OperatorNode::OP_IN: {
			return Variant::OP_IN;
		} break;
		case OperatorNode::OP_EQUAL: {
			return Variant::OP_EQUAL;
		} break;
		case OperatorNode::OP_NOT_EQUAL: {
			return Variant::OP_NOT_EQUAL;
		} break;
		case OperatorNode::OP_LESS: {
			return Variant::OP_LESS;
		} break;
		case OperatorNode::OP_LESS_EQUAL: {
			return Variant::OP_LESS_EQUAL;
		} break;
		case OperatorNode::OP_GREATER: {
			return Variant::OP_GREATER;
		} break;
		case OperatorNode::OP_GREATER_EQUAL: {
			return Variant::OP_GREATER_EQUAL;
		} break;
		case OperatorNode::OP_AND: {
			return Variant::OP_AND;
		} break;
		case OperatorNode::OP_OR: {
			return Variant::OP_OR;
		} break;
		case OperatorNode::OP_ASSIGN_ADD:
		case OperatorNode::OP_ADD: {
			return Variant::OP_ADD;
		} break;
		case OperatorNode::OP_ASSIGN_SUB:
		case OperatorNode::OP_SUB: {
			return Variant::OP_SUBTRACT;
		} break;
		case OperatorNode::OP_ASSIGN_MUL:
		case OperatorNode::OP_MUL: {
			return Variant::OP_MULTIPLY;
		} break;
		case OperatorNode::OP_ASSIGN_DIV:
		case OperatorNode::OP_DIV: {
			return Variant::OP_DIVIDE;
		} break;
		case OperatorNode::OP_ASSIGN_MOD:
		case OperatorNode::OP_MOD: {
			return Variant::OP_MODULE;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_AND:
		case OperatorNode::OP_BIT_AND: {
			return Variant::OP_BIT_AND;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_OR:
		case OperatorNode::OP_BIT_OR: {
			return Variant::OP_BIT_OR;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_XOR:
		case OperatorNode::OP_BIT_XOR: {
			return Variant::OP_BIT_XOR;
		} break;
		case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
		case OperatorNode::OP_SHIFT_LEFT: {
			return Variant::OP_SHIFT_LEFT;
		}
		case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
		case OperatorNode::OP_SHIFT_RIGHT: {
			return Variant::OP_SHIFT_RIGHT;
		}
		default: {
			return Variant::OP_MAX;
		} break;
	}
}

void GDScriptParser::_set_error(const String &p_error, int p_line, int p_column) {

	if (error_set)
		return; //allow no further errors

	error = p_error;
	error_line = p_line < 0 ? tokenizer->get_token_line() : p_line;
	error_column = p_column < 0 ? tokenizer->get_token_column() : p_column;
	error_set = true;
}

String GDScriptParser::get_error() const {

	return error;
}

int GDScriptParser::get_error_line() const {

	return error_line;
}
int GDScriptParser::get_error_column() const {

	return error_column;
}

Error GDScriptParser::_parse(const String &p_base_path) {

	base_path = p_base_path;

	//assume class
	ClassNode *main_class = alloc_node<ClassNode>();
	main_class->initializer = alloc_node<BlockNode>();
	main_class->initializer->parent_class = main_class;
	main_class->ready = alloc_node<BlockNode>();
	main_class->ready->parent_class = main_class;
	current_class = main_class;

	_parse_class(main_class);

	if (tokenizer->get_token() == GDScriptTokenizer::TK_ERROR) {
		error_set = false;
		_set_error("Parse Error: " + tokenizer->get_token_error());
	}

	if (error_set) {

		return ERR_PARSE_ERROR;
	}

	if (type_check) {
		current_class = main_class;
		current_block = NULL;
		current_function = NULL;
		_check_class_types(main_class);
	}

	if (error_set) {

		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error GDScriptParser::parse_bytecode(const Vector<uint8_t> &p_bytecode, const String &p_base_path, const String &p_self_path) {

	clear();

	self_path = p_self_path;
	GDScriptTokenizerBuffer *tb = memnew(GDScriptTokenizerBuffer);
	tb->set_code_buffer(p_bytecode);
	tokenizer = tb;
	Error ret = _parse(p_base_path);
	memdelete(tb);
	tokenizer = NULL;
	return ret;
}

Error GDScriptParser::parse(const String &p_code, const String &p_base_path, bool p_just_validate, const String &p_self_path, bool p_for_completion) {

	clear();

	self_path = p_self_path;
	GDScriptTokenizerText *tt = memnew(GDScriptTokenizerText);
	tt->set_code(p_code);

	validating = p_just_validate;
	for_completion = p_for_completion;
	tokenizer = tt;
	Error ret = _parse(p_base_path);
	memdelete(tt);
	tokenizer = NULL;
	return ret;
}

bool GDScriptParser::is_tool_script() const {

	return (head && head->type == Node::TYPE_CLASS && static_cast<const ClassNode *>(head)->tool);
}

const GDScriptParser::Node *GDScriptParser::get_parse_tree() const {

	return head;
}

void GDScriptParser::clear() {

	while (list) {

		Node *l = list;
		list = list->next;
		memdelete(l);
	}

	head = NULL;
	list = NULL;

	completion_type = COMPLETION_NONE;
	completion_node = NULL;
	completion_class = NULL;
	completion_function = NULL;
	completion_block = NULL;
	current_block = NULL;
	current_class = NULL;

	completion_found = false;
	rpc_mode = ScriptInstance::RPC_MODE_DISABLED;

	current_function = NULL;

	validating = false;
	for_completion = false;
	error_set = false;
	type_check = false;
	tab_level.clear();
	tab_level.push_back(0);
	error_line = 0;
	error_column = 0;
	pending_newline = -1;
	parenthesis = 0;
	current_export.type = Variant::NIL;
	error = "";
}

GDScriptParser::CompletionType GDScriptParser::get_completion_type() {

	return completion_type;
}

StringName GDScriptParser::get_completion_cursor() {

	return completion_cursor;
}

int GDScriptParser::get_completion_line() {

	return completion_line;
}

Variant::Type GDScriptParser::get_completion_built_in_constant() {

	return completion_built_in_constant;
}

GDScriptParser::Node *GDScriptParser::get_completion_node() {

	return completion_node;
}

GDScriptParser::BlockNode *GDScriptParser::get_completion_block() {

	return completion_block;
}

GDScriptParser::ClassNode *GDScriptParser::get_completion_class() {

	return completion_class;
}

GDScriptParser::FunctionNode *GDScriptParser::get_completion_function() {

	return completion_function;
}

int GDScriptParser::get_completion_argument_index() {

	return completion_argument;
}

int GDScriptParser::get_completion_identifier_is_function() {

	return completion_ident_is_call;
}

GDScriptParser::GDScriptParser() {

	head = NULL;
	list = NULL;
	tokenizer = NULL;
	pending_newline = -1;
	type_check = true;
	clear();
}

GDScriptParser::~GDScriptParser() {

	clear();
}
