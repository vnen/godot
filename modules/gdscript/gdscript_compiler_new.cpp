/*************************************************************************/
/*  gdscript_compiler.cpp                                                */
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

#include "gdscript_compiler_new.h"

#include "gdscript.h"

bool GDScriptNewCompiler::_is_class_member_property(CodeGen &codegen, const StringName &p_name) {
	if (codegen.function_node && codegen.function_node->is_static) {
		return false;
	}

	if (codegen.stack_identifiers.has(p_name)) {
		return false; //shadowed
	}

	return _is_class_member_property(codegen.script, p_name);
}

bool GDScriptNewCompiler::_is_class_member_property(GDScript *owner, const StringName &p_name) {
	GDScript *scr = owner;
	GDScriptNativeClass *nc = nullptr;
	while (scr) {
		if (scr->native.is_valid()) {
			nc = scr->native.ptr();
		}
		scr = scr->_base;
	}

	ERR_FAIL_COND_V(!nc, false);

	return ClassDB::has_property(nc->get_name(), p_name);
}

void GDScriptNewCompiler::_set_error(const String &p_error, const GDScriptNewParser::Node *p_node) {
	if (error != "") {
		return;
	}

	error = p_error;
	if (p_node) {
		err_line = p_node->start_line;
		err_column = p_node->leftmost_column;
	} else {
		err_line = 0;
		err_column = 0;
	}
}

bool GDScriptNewCompiler::_create_unary_operator(CodeGen &codegen, const GDScriptNewParser::UnaryOpNode *on, Variant::Operator op, int p_stack_level) {
	int src_address_a = _parse_expression(codegen, on->operand, p_stack_level);
	if (src_address_a < 0) {
		return false;
	}

	codegen.opcodes.push_back(GDScriptFunction::OPCODE_OPERATOR); // perform operator
	codegen.opcodes.push_back(op); //which operator
	codegen.opcodes.push_back(src_address_a); // argument 1
	codegen.opcodes.push_back(src_address_a); // argument 2 (repeated)
	//codegen.opcodes.push_back(GDScriptFunction::ADDR_TYPE_NIL); // argument 2 (unary only takes one parameter)
	return true;
}

bool GDScriptNewCompiler::_create_binary_operator(CodeGen &codegen, const GDScriptNewParser::ExpressionNode *p_left_operand, const GDScriptNewParser::ExpressionNode *p_right_operand, Variant::Operator op, int p_stack_level, bool p_initializer, int p_index_addr) {
	int src_address_a = _parse_expression(codegen, p_left_operand, p_stack_level, false, p_initializer, p_index_addr);
	if (src_address_a < 0) {
		return false;
	}
	if (src_address_a & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
		p_stack_level++; //uses stack for return, increase stack
	}

	int src_address_b = _parse_expression(codegen, p_right_operand, p_stack_level, false, p_initializer);
	if (src_address_b < 0) {
		return false;
	}

	codegen.opcodes.push_back(GDScriptFunction::OPCODE_OPERATOR); // perform operator
	codegen.opcodes.push_back(op); //which operator
	codegen.opcodes.push_back(src_address_a); // argument 1
	codegen.opcodes.push_back(src_address_b); // argument 2 (unary only takes one parameter)
	return true;
}

bool GDScriptNewCompiler::_create_binary_operator(CodeGen &codegen, const GDScriptNewParser::BinaryOpNode *on, Variant::Operator op, int p_stack_level, bool p_initializer, int p_index_addr) {
	return _create_binary_operator(codegen, on->left_operand, on->right_operand, op, p_stack_level, p_initializer, p_index_addr);
}

GDScriptDataType GDScriptNewCompiler::_gdtype_from_datatype(const GDScriptNewParser::DataType &p_datatype) const {
	if (!p_datatype.is_set()) {
		return GDScriptDataType();
	}

	GDScriptDataType result;
	result.has_type = true;

	switch (p_datatype.kind) {
		case GDScriptNewParser::DataType::BUILTIN: {
			result.kind = GDScriptDataType::BUILTIN;
			result.builtin_type = p_datatype.builtin_type;
		} break;
		case GDScriptNewParser::DataType::NATIVE: {
			result.kind = GDScriptDataType::NATIVE;
			result.native_type = p_datatype.native_type;
		} break;
		case GDScriptNewParser::DataType::SCRIPT: {
			result.kind = GDScriptDataType::SCRIPT;
			result.script_type = p_datatype.script_type;
			result.native_type = result.script_type->get_instance_base_type();
		} break;
		case GDScriptNewParser::DataType::GDSCRIPT: {
			// Locate class by constructing the path to it and following that path
			GDScriptNewParser::ClassNode *class_type = p_datatype.gdscript_type;
			List<StringName> names;
			while (class_type->outer) {
				names.push_back(class_type->identifier->name);
				class_type = class_type->outer;
			}

			Ref<GDScript> script = Ref<GDScript>(main_script);
			while (names.back()) {
				if (!script->subclasses.has(names.back()->get())) {
					ERR_PRINT("Parser bug: Cannot locate datatype class.");
					result.has_type = false;
					return GDScriptDataType();
				}
				script = script->subclasses[names.back()->get()];
				names.pop_back();
			}

			result.kind = GDScriptDataType::GDSCRIPT;
			result.script_type = script;
			result.native_type = script->get_instance_base_type();
		} break;
		default: {
			ERR_PRINT("Parser bug: converting unresolved type.");
			return GDScriptDataType();
		}
	}

	return result;
}

int GDScriptNewCompiler::_parse_assign_right_expression(CodeGen &codegen, const GDScriptNewParser::AssignmentNode *p_assignment, int p_stack_level, int p_index_addr) {
	Variant::Operator var_op = Variant::OP_MAX;

	switch (p_assignment->operation) {
		case GDScriptNewParser::AssignmentNode::OP_ADDITION:
			var_op = Variant::OP_ADD;
			break;
		case GDScriptNewParser::AssignmentNode::OP_SUBTRACTION:
			var_op = Variant::OP_SUBTRACT;
			break;
		case GDScriptNewParser::AssignmentNode::OP_MULTIPLICATION:
			var_op = Variant::OP_MULTIPLY;
			break;
		case GDScriptNewParser::AssignmentNode::OP_DIVISION:
			var_op = Variant::OP_DIVIDE;
			break;
		case GDScriptNewParser::AssignmentNode::OP_MODULO:
			var_op = Variant::OP_MODULE;
			break;
		case GDScriptNewParser::AssignmentNode::OP_BIT_SHIFT_LEFT:
			var_op = Variant::OP_SHIFT_LEFT;
			break;
		case GDScriptNewParser::AssignmentNode::OP_BIT_SHIFT_RIGHT:
			var_op = Variant::OP_SHIFT_RIGHT;
			break;
		case GDScriptNewParser::AssignmentNode::OP_BIT_AND:
			var_op = Variant::OP_BIT_AND;
			break;
		case GDScriptNewParser::AssignmentNode::OP_BIT_OR:
			var_op = Variant::OP_BIT_OR;
			break;
		case GDScriptNewParser::AssignmentNode::OP_BIT_XOR:
			var_op = Variant::OP_BIT_XOR;
			break;
		case GDScriptNewParser::AssignmentNode::OP_NONE: {
			//none
		} break;
		default: {
			ERR_FAIL_V(-1);
		}
	}

	// bool initializer = p_expression->op == GDScriptNewParser::OperatorNode::OP_INIT_ASSIGN;

	if (var_op == Variant::OP_MAX) {
		return _parse_expression(codegen, p_assignment->assigned_value, p_stack_level, false, false);
	}

	if (!_create_binary_operator(codegen, p_assignment->assignee, p_assignment->assigned_value, var_op, p_stack_level, false, p_index_addr)) {
		return -1;
	}

	int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
	codegen.opcodes.push_back(dst_addr); // append the stack level as destination address of the opcode
	codegen.alloc_stack(p_stack_level);
	return dst_addr;
}

int GDScriptNewCompiler::_parse_expression(CodeGen &codegen, const GDScriptNewParser::ExpressionNode *p_expression, int p_stack_level, bool p_root, bool p_initializer, int p_index_addr) {
	switch (p_expression->type) {
		//should parse variable declaration and adjust stack accordingly...
		case GDScriptNewParser::Node::IDENTIFIER: {
			//return identifier
			//wait, identifier could be a local variable or something else... careful here, must reference properly
			//as stack may be more interesting to work with

			//This could be made much simpler by just indexing "self", but done this way (with custom self-addressing modes) increases performance a lot.

			const GDScriptNewParser::IdentifierNode *in = static_cast<const GDScriptNewParser::IdentifierNode *>(p_expression);

			StringName identifier = in->name;

			// TRY STACK!
			if (!p_initializer && codegen.stack_identifiers.has(identifier)) {
				int pos = codegen.stack_identifiers[identifier];
				return pos | (GDScriptFunction::ADDR_TYPE_STACK_VARIABLE << GDScriptFunction::ADDR_BITS);
			}

			// TRY CLASS MEMBER
			if (_is_class_member_property(codegen, identifier)) {
				//get property
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_GET_MEMBER); // perform operator
				codegen.opcodes.push_back(codegen.get_name_map_pos(identifier)); // argument 2 (unary only takes one parameter)
				int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
				codegen.opcodes.push_back(dst_addr); // append the stack level as destination address of the opcode
				codegen.alloc_stack(p_stack_level);
				return dst_addr;
			}

			//TRY MEMBERS!
			if (!codegen.function_node || !codegen.function_node->is_static) {
				// TRY MEMBER VARIABLES!
				//static function
				if (codegen.script->member_indices.has(identifier)) {
					int idx = codegen.script->member_indices[identifier].index;
					return idx | (GDScriptFunction::ADDR_TYPE_MEMBER << GDScriptFunction::ADDR_BITS); //argument (stack root)
				}
			}

			//TRY CLASS CONSTANTS

			GDScript *owner = codegen.script;
			while (owner) {
				GDScript *scr = owner;
				GDScriptNativeClass *nc = nullptr;
				while (scr) {
					if (scr->constants.has(identifier)) {
						//int idx=scr->constants[identifier];
						int idx = codegen.get_name_map_pos(identifier);
						return idx | (GDScriptFunction::ADDR_TYPE_CLASS_CONSTANT << GDScriptFunction::ADDR_BITS); //argument (stack root)
					}
					if (scr->native.is_valid()) {
						nc = scr->native.ptr();
					}
					scr = scr->_base;
				}

				// CLASS C++ Integer Constant

				if (nc) {
					bool success = false;
					int constant = ClassDB::get_integer_constant(nc->get_name(), identifier, &success);
					if (success) {
						Variant key = constant;
						int idx;

						if (!codegen.constant_map.has(key)) {
							idx = codegen.constant_map.size();
							codegen.constant_map[key] = idx;

						} else {
							idx = codegen.constant_map[key];
						}

						return idx | (GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT << GDScriptFunction::ADDR_BITS); //make it a local constant (faster access)
					}
				}

				owner = owner->_owner;
			}

			if (GDScriptLanguage::get_singleton()->get_global_map().has(identifier)) {
				int idx = GDScriptLanguage::get_singleton()->get_global_map()[identifier];
				return idx | (GDScriptFunction::ADDR_TYPE_GLOBAL << GDScriptFunction::ADDR_BITS); //argument (stack root)
			}

			/* TRY GLOBAL CLASSES */

			if (ScriptServer::is_global_class(identifier)) {
				const GDScriptNewParser::ClassNode *class_node = codegen.class_node;
				while (class_node->outer) {
					class_node = class_node->outer;
				}

				if (class_node->identifier && class_node->identifier->name == identifier) {
					_set_error("Using own name in class file is not allowed (creates a cyclic reference)", p_expression);
					return -1;
				}

				RES res = ResourceLoader::load(ScriptServer::get_global_class_path(identifier));
				if (res.is_null()) {
					_set_error("Can't load global class " + String(identifier) + ", cyclic reference?", p_expression);
					return -1;
				}

				Variant key = res;
				int idx;

				if (!codegen.constant_map.has(key)) {
					idx = codegen.constant_map.size();
					codegen.constant_map[key] = idx;

				} else {
					idx = codegen.constant_map[key];
				}

				return idx | (GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT << GDScriptFunction::ADDR_BITS); //make it a local constant (faster access)
			}

#ifdef TOOLS_ENABLED
			if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(identifier)) {
				int idx = codegen.named_globals.find(identifier);
				if (idx == -1) {
					idx = codegen.named_globals.size();
					codegen.named_globals.push_back(identifier);
				}
				return idx | (GDScriptFunction::ADDR_TYPE_NAMED_GLOBAL << GDScriptFunction::ADDR_BITS);
			}
#endif

			//not found, error

			_set_error("Identifier not found: " + String(identifier), p_expression);

			return -1;

		} break;
		case GDScriptNewParser::Node::LITERAL: {
			//return constant
			const GDScriptNewParser::LiteralNode *cn = static_cast<const GDScriptNewParser::LiteralNode *>(p_expression);

			int idx;

			if (!codegen.constant_map.has(cn->value)) {
				idx = codegen.constant_map.size();
				codegen.constant_map[cn->value] = idx;

			} else {
				idx = codegen.constant_map[cn->value];
			}

			return idx | (GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT << GDScriptFunction::ADDR_BITS); //argument (stack root)

		} break;
		case GDScriptNewParser::Node::SELF: {
			//return constant
			if (codegen.function_node && codegen.function_node->is_static) {
				_set_error("'self' not present in static function!", p_expression);
				return -1;
			}
			return (GDScriptFunction::ADDR_TYPE_SELF << GDScriptFunction::ADDR_BITS);
		} break;
		case GDScriptNewParser::Node::ARRAY: {
			const GDScriptNewParser::ArrayNode *an = static_cast<const GDScriptNewParser::ArrayNode *>(p_expression);
			Vector<int> values;

			int slevel = p_stack_level;

			for (int i = 0; i < an->elements.size(); i++) {
				int ret = _parse_expression(codegen, an->elements[i], slevel);
				if (ret < 0) {
					return ret;
				}
				if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				values.push_back(ret);
			}

			codegen.opcodes.push_back(GDScriptFunction::OPCODE_CONSTRUCT_ARRAY);
			codegen.opcodes.push_back(values.size());
			for (int i = 0; i < values.size(); i++) {
				codegen.opcodes.push_back(values[i]);
			}

			int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			codegen.opcodes.push_back(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		case GDScriptNewParser::Node::DICTIONARY: {
			const GDScriptNewParser::DictionaryNode *dn = static_cast<const GDScriptNewParser::DictionaryNode *>(p_expression);
			Vector<int> values;

			int slevel = p_stack_level;

			for (int i = 0; i < dn->elements.size(); i++) {
				int ret = _parse_expression(codegen, dn->elements[i].key, slevel);
				if (ret < 0) {
					return ret;
				}
				if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				values.push_back(ret);

				ret = _parse_expression(codegen, dn->elements[i].value, slevel);
				if (ret < 0) {
					return ret;
				}
				if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				values.push_back(ret);
			}

			codegen.opcodes.push_back(GDScriptFunction::OPCODE_CONSTRUCT_DICTIONARY);
			codegen.opcodes.push_back(dn->elements.size());
			for (int i = 0; i < values.size(); i++) {
				codegen.opcodes.push_back(values[i]);
			}

			int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			codegen.opcodes.push_back(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		case GDScriptNewParser::Node::CAST: {
			const GDScriptNewParser::CastNode *cn = static_cast<const GDScriptNewParser::CastNode *>(p_expression);

			int slevel = p_stack_level;
			int src_addr = _parse_expression(codegen, cn->operand, slevel);
			if (src_addr < 0) {
				return src_addr;
			}
			if (src_addr & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
				slevel++;
				codegen.alloc_stack(slevel);
			}

			// FIXME: Allow actual cast.
			GDScriptDataType cast_type; // = _gdtype_from_datatype(cn->cast_type);

			switch (cast_type.kind) {
				case GDScriptDataType::BUILTIN: {
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_CAST_TO_BUILTIN);
					codegen.opcodes.push_back(cast_type.builtin_type);
				} break;
				case GDScriptDataType::NATIVE: {
					int class_idx;
					if (GDScriptLanguage::get_singleton()->get_global_map().has(cast_type.native_type)) {
						class_idx = GDScriptLanguage::get_singleton()->get_global_map()[cast_type.native_type];
						class_idx |= (GDScriptFunction::ADDR_TYPE_GLOBAL << GDScriptFunction::ADDR_BITS); //argument (stack root)
					} else {
						_set_error("Invalid native class type '" + String(cast_type.native_type) + "'.", cn);
						return -1;
					}
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_CAST_TO_NATIVE); // perform operator
					codegen.opcodes.push_back(class_idx); // variable type
				} break;
				case GDScriptDataType::SCRIPT:
				case GDScriptDataType::GDSCRIPT: {
					Variant script = cast_type.script_type;
					int idx = codegen.get_constant_pos(script);
					idx |= GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT << GDScriptFunction::ADDR_BITS; //make it a local constant (faster access)

					codegen.opcodes.push_back(GDScriptFunction::OPCODE_CAST_TO_SCRIPT); // perform operator
					codegen.opcodes.push_back(idx); // variable type
				} break;
				default: {
					_set_error("Parser bug: unresolved data type.", cn);
					return -1;
				}
			}

			codegen.opcodes.push_back(src_addr); // source address
			int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			codegen.opcodes.push_back(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		//hell breaks loose

		//call/constructor operator
		// TODO: Support for super call.
		//				case GDScriptNewParser::Node::SUPER_CALL: {
		//					ERR_FAIL_COND_V(on->arguments.size() < 1, -1);

		//					const GDScriptNewParser::IdentifierNode *in = (const GDScriptNewParser::IdentifierNode *)on->arguments[0];

		//					Vector<int> arguments;
		//					int slevel = p_stack_level;
		//					for (int i = 1; i < on->arguments.size(); i++) {
		//						int ret = _parse_expression(codegen, on->arguments[i], slevel);
		//						if (ret < 0) {
		//							return ret;
		//						}
		//						if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
		//							slevel++;
		//							codegen.alloc_stack(slevel);
		//						}
		//						arguments.push_back(ret);
		//					}

		//					//push call bytecode
		//					codegen.opcodes.push_back(GDScriptFunction::OPCODE_CALL_SELF_BASE); // basic type constructor

		//					codegen.opcodes.push_back(codegen.get_name_map_pos(in->name)); //instance
		//					codegen.opcodes.push_back(arguments.size()); //argument count
		//					codegen.alloc_call(arguments.size());
		//					for (int i = 0; i < arguments.size(); i++) {
		//						codegen.opcodes.push_back(arguments[i]); //arguments
		//					}

		//				} break;

#define OPERATOR_RETURN                                                                                  \
	int dst_addr = (p_stack_level) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS); \
	codegen.opcodes.push_back(dst_addr);                                                                 \
	codegen.alloc_stack(p_stack_level);                                                                  \
	return dst_addr

		case GDScriptNewParser::Node::CALL: {
			const GDScriptNewParser::CallNode *call = static_cast<const GDScriptNewParser::CallNode *>(p_expression);
			if (call->callee->type == GDScriptNewParser::Node::IDENTIFIER && GDScriptNewParser::get_builtin_type(static_cast<GDScriptNewParser::IdentifierNode *>(call->callee)->name) != Variant::VARIANT_MAX) {
				//construct a basic type

				Variant::Type vtype = GDScriptNewParser::get_builtin_type(static_cast<GDScriptNewParser::IdentifierNode *>(call->callee)->name);

				Vector<int> arguments;
				int slevel = p_stack_level;
				for (int i = 0; i < call->arguments.size(); i++) {
					int ret = _parse_expression(codegen, call->arguments[i], slevel);
					if (ret < 0) {
						return ret;
					}
					if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
						slevel++;
						codegen.alloc_stack(slevel);
					}
					arguments.push_back(ret);
				}

				//push call bytecode
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_CONSTRUCT); // basic type constructor
				codegen.opcodes.push_back(vtype); //instance
				codegen.opcodes.push_back(arguments.size()); //argument count
				codegen.alloc_call(arguments.size());
				for (int i = 0; i < arguments.size(); i++) {
					codegen.opcodes.push_back(arguments[i]); //arguments
				}

			} else if (call->callee->type == GDScriptNewParser::Node::IDENTIFIER && GDScriptNewParser::get_builtin_function(static_cast<GDScriptNewParser::IdentifierNode *>(call->callee)->name) != GDScriptFunctions::FUNC_MAX) {
				//built in function

				Vector<int> arguments;
				int slevel = p_stack_level;
				for (int i = 0; i < call->arguments.size(); i++) {
					int ret = _parse_expression(codegen, call->arguments[i], slevel);
					if (ret < 0) {
						return ret;
					}

					if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
						slevel++;
						codegen.alloc_stack(slevel);
					}

					arguments.push_back(ret);
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_CALL_BUILT_IN);
				codegen.opcodes.push_back(GDScriptNewParser::get_builtin_function(static_cast<GDScriptNewParser::IdentifierNode *>(call->callee)->name));
				codegen.opcodes.push_back(arguments.size());
				codegen.alloc_call(arguments.size());
				for (int i = 0; i < arguments.size(); i++) {
					codegen.opcodes.push_back(arguments[i]);
				}

			} else {
				//regular function

				const GDScriptNewParser::ExpressionNode *instance = call->callee;

				if (instance->type == GDScriptNewParser::Node::SELF) {
					//room for optimization
				}

				Vector<int> arguments;
				int slevel = p_stack_level;

				int ret = -1;
				if (call->callee->type == GDScriptNewParser::Node::SELF) {
					ret = (GDScriptFunction::ADDR_TYPE_CLASS << GDScriptFunction::ADDR_BITS);
				} else {
					ret = _parse_expression(codegen, call->callee, slevel);
					if (ret < 0) {
						return ret;
					}
					if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
						slevel++;
						codegen.alloc_stack(slevel);
					}
				}
				for (int i = 0; i < call->arguments.size(); i++) {
					ret = _parse_expression(codegen, call->arguments[i], slevel);
					if (ret < 0) {
						return ret;
					}
					if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
						slevel++;
						codegen.alloc_stack(slevel);
					}
					arguments.push_back(ret);
				}

				codegen.opcodes.push_back(p_root ? GDScriptFunction::OPCODE_CALL : GDScriptFunction::OPCODE_CALL_RETURN); // perform operator
				codegen.opcodes.push_back(arguments.size());
				codegen.alloc_call(arguments.size());
				for (int i = 0; i < arguments.size(); i++) {
					codegen.opcodes.push_back(arguments[i]);
				}
			}
			OPERATOR_RETURN;
		} break;
		// TODO: Need to fix VM to support a signal type.
		// case GDScriptNewParser::Node::AWAIT: {
		// 	Vector<int> arguments;
		// 	int slevel = p_stack_level;
		// 	for (int i = 0; i < on->arguments.size(); i++) {
		// 		int ret = _parse_expression(codegen, on->arguments[i], slevel);
		// 		if (ret < 0) {
		// 			return ret;
		// 		}
		// 		if ((ret >> GDScriptFunction::ADDR_BITS & GDScriptFunction::ADDR_TYPE_STACK) == GDScriptFunction::ADDR_TYPE_STACK) {
		// 			slevel++;
		// 			codegen.alloc_stack(slevel);
		// 		}
		// 		arguments.push_back(ret);
		// 	}

		// 	//push call bytecode
		// 	codegen.opcodes.push_back(arguments.size() == 0 ? GDScriptFunction::OPCODE_YIELD : GDScriptFunction::OPCODE_YIELD_SIGNAL); // basic type constructor
		// 	for (int i = 0; i < arguments.size(); i++) {
		// 		codegen.opcodes.push_back(arguments[i]); //arguments
		// 	}
		// 	codegen.opcodes.push_back(GDScriptFunction::OPCODE_YIELD_RESUME);
		// 	//next will be where to place the result :)

		// } break;

		//indexing operator
		case GDScriptNewParser::Node::SUBSCRIPT: {
			int slevel = p_stack_level;

			const GDScriptNewParser::SubscriptNode *subscript = static_cast<const GDScriptNewParser::SubscriptNode *>(p_expression);

			int from = _parse_expression(codegen, subscript->base, slevel);
			if (from < 0) {
				return from;
			}

			int index;
			if (p_index_addr != 0) {
				index = p_index_addr;
			} else if (subscript->is_attribute) {
				if (subscript->base->type == GDScriptNewParser::Node::SELF && codegen.script && codegen.function_node && !codegen.function_node->is_static) {
					GDScriptNewParser::IdentifierNode *identifier = subscript->attribute;
					const Map<StringName, GDScript::MemberInfo>::Element *MI = codegen.script->member_indices.find(identifier->name);

#ifdef DEBUG_ENABLED
					if (MI && MI->get().getter == codegen.function_node->identifier->name) {
						String n = identifier->name;
						_set_error("Must use '" + n + "' instead of 'self." + n + "' in getter.", identifier);
						return -1;
					}
#endif

					if (MI && MI->get().getter == "") {
						// Faster than indexing self (as if no self. had been used)
						return (MI->get().index) | (GDScriptFunction::ADDR_TYPE_MEMBER << GDScriptFunction::ADDR_BITS);
					}
				}

				index = codegen.get_name_map_pos(subscript->attribute->name);

			} else {
				if (subscript->index->type == GDScriptNewParser::Node::LITERAL && static_cast<const GDScriptNewParser::LiteralNode *>(subscript->index)->value.get_type() == Variant::STRING) {
					//also, somehow, named (speed up anyway)
					StringName name = static_cast<const GDScriptNewParser::LiteralNode *>(subscript->index)->value;
					index = codegen.get_name_map_pos(name);

				} else {
					//regular indexing
					if (from & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
						slevel++;
						codegen.alloc_stack(slevel);
					}

					index = _parse_expression(codegen, subscript->index, slevel);
					if (index < 0) {
						return index;
					}
				}
			}

			codegen.opcodes.push_back(subscript->is_attribute ? GDScriptFunction::OPCODE_GET_NAMED : GDScriptFunction::OPCODE_GET); // perform operator
			codegen.opcodes.push_back(from); // argument 1
			codegen.opcodes.push_back(index); // argument 2 (unary only takes one parameter)
			OPERATOR_RETURN;
		} break;
		case GDScriptNewParser::Node::UNARY_OPERATOR: {
			//unary operators
			const GDScriptNewParser::UnaryOpNode *unary = static_cast<const GDScriptNewParser::UnaryOpNode *>(p_expression);
			switch (unary->operation) {
				case GDScriptNewParser::UnaryOpNode::OP_NEGATIVE: {
					if (!_create_unary_operator(codegen, unary, Variant::OP_NEGATE, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::UnaryOpNode::OP_POSITIVE: {
					if (!_create_unary_operator(codegen, unary, Variant::OP_POSITIVE, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::UnaryOpNode::OP_LOGIC_NOT: {
					if (!_create_unary_operator(codegen, unary, Variant::OP_NOT, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::UnaryOpNode::OP_COMPLEMENT: {
					if (!_create_unary_operator(codegen, unary, Variant::OP_BIT_NEGATE, p_stack_level)) {
						return -1;
					}
				} break;
			}
			OPERATOR_RETURN;
		}
		case GDScriptNewParser::Node::BINARY_OPERATOR: {
			//binary operators (in precedence order)
			const GDScriptNewParser::BinaryOpNode *binary = static_cast<const GDScriptNewParser::BinaryOpNode *>(p_expression);

			switch (binary->operation) {
				case GDScriptNewParser::BinaryOpNode::OP_LOGIC_AND: {
					// AND operator with early out on failure

					int res = _parse_expression(codegen, binary->left_operand, p_stack_level);
					if (res < 0) {
						return res;
					}
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF_NOT);
					codegen.opcodes.push_back(res);
					int jump_fail_pos = codegen.opcodes.size();
					codegen.opcodes.push_back(0);

					res = _parse_expression(codegen, binary->right_operand, p_stack_level);
					if (res < 0) {
						return res;
					}

					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF_NOT);
					codegen.opcodes.push_back(res);
					int jump_fail_pos2 = codegen.opcodes.size();
					codegen.opcodes.push_back(0);

					codegen.alloc_stack(p_stack_level); //it will be used..
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_TRUE);
					codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
					codegen.opcodes.push_back(codegen.opcodes.size() + 3);
					codegen.opcodes.write[jump_fail_pos] = codegen.opcodes.size();
					codegen.opcodes.write[jump_fail_pos2] = codegen.opcodes.size();
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_FALSE);
					codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
					return p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS;

				} break;
				case GDScriptNewParser::BinaryOpNode::OP_LOGIC_OR: {
					// OR operator with early out on success

					int res = _parse_expression(codegen, binary->left_operand, p_stack_level);
					if (res < 0) {
						return res;
					}
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF);
					codegen.opcodes.push_back(res);
					int jump_success_pos = codegen.opcodes.size();
					codegen.opcodes.push_back(0);

					res = _parse_expression(codegen, binary->right_operand, p_stack_level);
					if (res < 0) {
						return res;
					}

					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF);
					codegen.opcodes.push_back(res);
					int jump_success_pos2 = codegen.opcodes.size();
					codegen.opcodes.push_back(0);

					codegen.alloc_stack(p_stack_level); //it will be used..
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_FALSE);
					codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
					codegen.opcodes.push_back(codegen.opcodes.size() + 3);
					codegen.opcodes.write[jump_success_pos] = codegen.opcodes.size();
					codegen.opcodes.write[jump_success_pos2] = codegen.opcodes.size();
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_TRUE);
					codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
					return p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS;

				} break;
				case GDScriptNewParser::BinaryOpNode::OP_TYPE_TEST: {
					int slevel = p_stack_level;

					int src_address_a = _parse_expression(codegen, binary->left_operand, slevel);
					if (src_address_a < 0) {
						return -1;
					}

					if (src_address_a & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
						slevel++; //uses stack for return, increase stack
					}

					int src_address_b = -1;
					bool builtin = false;
					if (binary->right_operand->type == GDScriptNewParser::Node::IDENTIFIER && GDScriptNewParser::get_builtin_type(static_cast<const GDScriptNewParser::IdentifierNode *>(binary->right_operand)->name) != Variant::VARIANT_MAX) {
						// `is` with builtin type
						builtin = true;
						src_address_b = (int)GDScriptNewParser::get_builtin_type(static_cast<const GDScriptNewParser::IdentifierNode *>(binary->right_operand)->name);
					} else {
						src_address_b = _parse_expression(codegen, binary->right_operand, slevel);
						if (src_address_b < 0) {
							return -1;
						}
					}

					codegen.opcodes.push_back(builtin ? GDScriptFunction::OPCODE_IS_BUILTIN : GDScriptFunction::OPCODE_EXTENDS_TEST); // perform operator
					codegen.opcodes.push_back(src_address_a); // argument 1
					codegen.opcodes.push_back(src_address_b); // argument 2 (unary only takes one parameter)

				} break;
				case GDScriptNewParser::BinaryOpNode::OP_CONTENT_TEST: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_IN, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_EQUAL: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_EQUAL, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_NOT_EQUAL: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_NOT_EQUAL, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_LESS: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_LESS, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_LESS_EQUAL: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_LESS_EQUAL, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_GREATER: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_GREATER, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_COMP_GREATER_EQUAL: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_GREATER_EQUAL, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_ADDITION: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_ADD, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_SUBTRACTION: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_SUBTRACT, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_MULTIPLICATION: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_MULTIPLY, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_DIVISION: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_DIVIDE, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_MODULO: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_MODULE, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_BIT_AND: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_BIT_AND, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_BIT_OR: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_BIT_OR, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_BIT_XOR: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_BIT_XOR, p_stack_level)) {
						return -1;
					}
				} break;
				//shift
				case GDScriptNewParser::BinaryOpNode::OP_BIT_LEFT_SHIFT: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_SHIFT_LEFT, p_stack_level)) {
						return -1;
					}
				} break;
				case GDScriptNewParser::BinaryOpNode::OP_BIT_RIGHT_SHIFT: {
					if (!_create_binary_operator(codegen, binary, Variant::OP_SHIFT_RIGHT, p_stack_level)) {
						return -1;
					}
				} break;
			}
			OPERATOR_RETURN;
		} break;
		// ternary operators
		case GDScriptNewParser::Node::TERNARY_OPERATOR: {
			// x IF a ELSE y operator with early out on failure

			const GDScriptNewParser::TernaryOpNode *ternary = static_cast<const GDScriptNewParser::TernaryOpNode *>(p_expression);
			int res = _parse_expression(codegen, ternary->condition, p_stack_level);
			if (res < 0) {
				return res;
			}
			codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF_NOT);
			codegen.opcodes.push_back(res);
			int jump_fail_pos = codegen.opcodes.size();
			codegen.opcodes.push_back(0);

			res = _parse_expression(codegen, ternary->true_expr, p_stack_level);
			if (res < 0) {
				return res;
			}

			codegen.alloc_stack(p_stack_level); //it will be used..
			codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN);
			codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			codegen.opcodes.push_back(res);
			codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
			int jump_past_pos = codegen.opcodes.size();
			codegen.opcodes.push_back(0);

			codegen.opcodes.write[jump_fail_pos] = codegen.opcodes.size();
			res = _parse_expression(codegen, ternary->false_expr, p_stack_level);
			if (res < 0) {
				return res;
			}

			codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN);
			codegen.opcodes.push_back(p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			codegen.opcodes.push_back(res);

			codegen.opcodes.write[jump_past_pos] = codegen.opcodes.size();

			return p_stack_level | GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS;

		} break;
		//assignment operators
		case GDScriptNewParser::Node::ASSIGNMENT: {
			const GDScriptNewParser::AssignmentNode *assignment = static_cast<const GDScriptNewParser::AssignmentNode *>(p_expression);

			if (assignment->assignee->type == GDScriptNewParser::Node::SUBSCRIPT) {
				// SET (chained) MODE!
				const GDScriptNewParser::SubscriptNode *subscript = static_cast<GDScriptNewParser::SubscriptNode *>(assignment->assignee);
#ifdef DEBUG_ENABLED
				if (subscript->is_attribute) {
					if (subscript->base->type == GDScriptNewParser::Node::SELF && codegen.script && codegen.function_node && !codegen.function_node->is_static) {
						const Map<StringName, GDScript::MemberInfo>::Element *MI = codegen.script->member_indices.find(subscript->attribute->name);
						if (MI && MI->get().setter == codegen.function_node->identifier->name) {
							String n = subscript->attribute->name;
							_set_error("Must use '" + n + "' instead of 'self." + n + "' in setter.", subscript);
							return -1;
						}
					}
				}
#endif

				int slevel = p_stack_level;

				/* Find chain of sets */

				StringName assign_property;

				List<const GDScriptNewParser::SubscriptNode *> chain;

				{
					//create get/set chain
					const GDScriptNewParser::SubscriptNode *n = subscript;
					while (true) {
						chain.push_back(n);

						if (n->base->type != GDScriptNewParser::Node::SUBSCRIPT) {
							//check for a built-in property
							if (n->base->type == GDScriptNewParser::Node::IDENTIFIER) {
								GDScriptNewParser::IdentifierNode *identifier = static_cast<GDScriptNewParser::IdentifierNode *>(n->base);
								if (_is_class_member_property(codegen, identifier->name)) {
									assign_property = identifier->name;
								}
							}
							break;
						}
						n = static_cast<const GDScriptNewParser::SubscriptNode *>(n->base);
					}
				}

				/* Chain of gets */

				//get at (potential) root stack pos, so it can be returned
				int prev_pos = _parse_expression(codegen, chain.back()->get()->base, slevel);
				if (prev_pos < 0) {
					return prev_pos;
				}
				int retval = prev_pos;

				if (retval & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				Vector<int> setchain;

				if (assign_property != StringName()) {
					// recover and assign at the end, this allows stuff like
					// position.x+=2.0
					// in Node2D
					setchain.push_back(prev_pos);
					setchain.push_back(codegen.get_name_map_pos(assign_property));
					setchain.push_back(GDScriptFunction::OPCODE_SET_MEMBER);
				}

				for (List<const GDScriptNewParser::SubscriptNode *>::Element *E = chain.back(); E; E = E->prev()) {
					if (E == chain.front()) { //ignore first
						break;
					}

					const GDScriptNewParser::SubscriptNode *subscript = E->get();
					int key_idx;

					if (subscript->is_attribute) {
						key_idx = codegen.get_name_map_pos(subscript->attribute->name);
						//printf("named key %x\n",key_idx);

					} else {
						if (prev_pos & (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS)) {
							slevel++;
							codegen.alloc_stack(slevel);
						}

						GDScriptNewParser::ExpressionNode *key = subscript->index;
						key_idx = _parse_expression(codegen, key, slevel);
						//printf("expr key %x\n",key_idx);

						//stack was raised here if retval was stack but..
					}

					if (key_idx < 0) { //error
						return key_idx;
					}

					codegen.opcodes.push_back(subscript->is_attribute ? GDScriptFunction::OPCODE_GET_NAMED : GDScriptFunction::OPCODE_GET);
					codegen.opcodes.push_back(prev_pos);
					codegen.opcodes.push_back(key_idx);
					slevel++;
					codegen.alloc_stack(slevel);
					int dst_pos = (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) | slevel;

					codegen.opcodes.push_back(dst_pos);

					//add in reverse order, since it will be reverted

					setchain.push_back(dst_pos);
					setchain.push_back(key_idx);
					setchain.push_back(prev_pos);
					setchain.push_back(subscript->is_attribute ? GDScriptFunction::OPCODE_SET_NAMED : GDScriptFunction::OPCODE_SET);

					prev_pos = dst_pos;
				}

				setchain.invert();

				int set_index;

				if (subscript->is_attribute) {
					set_index = codegen.get_name_map_pos(subscript->attribute->name);
				} else {
					set_index = _parse_expression(codegen, subscript->index, slevel + 1);
				}

				if (set_index < 0) { //error
					return set_index;
				}

				if (set_index & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				int set_value = _parse_assign_right_expression(codegen, assignment, slevel + 1, subscript->is_attribute ? 0 : set_index);
				if (set_value < 0) { //error
					return set_value;
				}

				codegen.opcodes.push_back(subscript->is_attribute ? GDScriptFunction::OPCODE_SET_NAMED : GDScriptFunction::OPCODE_SET);
				codegen.opcodes.push_back(prev_pos);
				codegen.opcodes.push_back(set_index);
				codegen.opcodes.push_back(set_value);

				for (int i = 0; i < setchain.size(); i++) {
					codegen.opcodes.push_back(setchain[i]);
				}

				return retval;

			} else if (assignment->assignee->type == GDScriptNewParser::Node::IDENTIFIER && _is_class_member_property(codegen, static_cast<GDScriptNewParser::IdentifierNode *>(assignment->assignee)->name)) {
				//assignment to member property

				int slevel = p_stack_level;

				int src_address = _parse_assign_right_expression(codegen, assignment, slevel);
				if (src_address < 0) {
					return -1;
				}

				StringName name = static_cast<GDScriptNewParser::IdentifierNode *>(assignment->assignee)->name;

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_SET_MEMBER);
				codegen.opcodes.push_back(codegen.get_name_map_pos(name));
				codegen.opcodes.push_back(src_address);

				return GDScriptFunction::ADDR_TYPE_NIL << GDScriptFunction::ADDR_BITS;
			} else {
				//REGULAR ASSIGNMENT MODE!!

				int slevel = p_stack_level;

				// TODO: Add initialization assignment back somewhere;
				int dst_address_a = _parse_expression(codegen, assignment->assignee, slevel);
				if (dst_address_a < 0) {
					return -1;
				}

				if (dst_address_a & GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) {
					slevel++;
					codegen.alloc_stack(slevel);
				}

				int src_address_b = _parse_assign_right_expression(codegen, assignment, slevel);
				if (src_address_b < 0) {
					return -1;
				}

				// FIXME: Actually check type.
				GDScriptDataType assign_type; // = _gdtype_from_datatype(on->arguments[0]->get_datatype());

				if (assign_type.has_type && !assignment->get_datatype().is_set()) {
					// Typed assignment
					switch (assign_type.kind) {
						case GDScriptDataType::BUILTIN: {
							codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_TYPED_BUILTIN); // perform operator
							codegen.opcodes.push_back(assign_type.builtin_type); // variable type
							codegen.opcodes.push_back(dst_address_a); // argument 1
							codegen.opcodes.push_back(src_address_b); // argument 2
						} break;
						case GDScriptDataType::NATIVE: {
							int class_idx;
							if (GDScriptLanguage::get_singleton()->get_global_map().has(assign_type.native_type)) {
								class_idx = GDScriptLanguage::get_singleton()->get_global_map()[assign_type.native_type];
								class_idx |= (GDScriptFunction::ADDR_TYPE_GLOBAL << GDScriptFunction::ADDR_BITS); //argument (stack root)
							} else {
								// _set_error("Invalid native class type '" + String(assign_type.native_type) + "'.", on->arguments[0]);
								return -1;
							}
							codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_TYPED_NATIVE); // perform operator
							codegen.opcodes.push_back(class_idx); // variable type
							codegen.opcodes.push_back(dst_address_a); // argument 1
							codegen.opcodes.push_back(src_address_b); // argument 2
						} break;
						case GDScriptDataType::SCRIPT:
						case GDScriptDataType::GDSCRIPT: {
							Variant script = assign_type.script_type;
							int idx = codegen.get_constant_pos(script);
							idx |= GDScriptFunction::ADDR_TYPE_LOCAL_CONSTANT << GDScriptFunction::ADDR_BITS; //make it a local constant (faster access)

							codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN_TYPED_SCRIPT); // perform operator
							codegen.opcodes.push_back(idx); // variable type
							codegen.opcodes.push_back(dst_address_a); // argument 1
							codegen.opcodes.push_back(src_address_b); // argument 2
						} break;
						default: {
							ERR_PRINT("Compiler bug: unresolved assign.");

							// Shouldn't get here, but fail-safe to a regular assignment
							codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN); // perform operator
							codegen.opcodes.push_back(dst_address_a); // argument 1
							codegen.opcodes.push_back(src_address_b); // argument 2 (unary only takes one parameter)
						}
					}
				} else {
					// Either untyped assignment or already type-checked by the parser
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN); // perform operator
					codegen.opcodes.push_back(dst_address_a); // argument 1
					codegen.opcodes.push_back(src_address_b); // argument 2 (unary only takes one parameter)
				}
				return dst_address_a; //if anything, returns wathever was assigned or correct stack position
			}
		} break;
#undef OPERATOR_RETURN
		//TYPE_TYPE,
		default: {
			ERR_FAIL_V_MSG(-1, "Bug in bytecode compiler, unexpected node in parse tree while parsing expression."); //unreachable code
		} break;
	}
}

Error GDScriptNewCompiler::_parse_block(CodeGen &codegen, const GDScriptNewParser::SuiteNode *p_block, int p_stack_level, int p_break_addr, int p_continue_addr) {
	codegen.push_stack_identifiers();
	int new_identifiers = 0;
	codegen.current_line = p_block->start_line;

	for (int i = 0; i < p_block->statements.size(); i++) {
		const GDScriptNewParser::Node *s = p_block->statements[i];

		switch (s->type) {
				// TODO: Fix newlines.
				// 			case GDScriptNewParser::Node::TYPE_NEWLINE: {
				// #ifdef DEBUG_ENABLED
				// 				const GDScriptNewParser::NewLineNode *nl = static_cast<const GDScriptNewParser::NewLineNode *>(s);
				// 				codegen.opcodes.push_back(GDScriptFunction::OPCODE_LINE);
				// 				codegen.opcodes.push_back(nl->line);
				// 				codegen.current_line = nl->line;
				// #endif
				// 			} break;
				// FIXME: Implement match compiler.
				// case GDScriptNewParser::Node::MATCH: {
				// 	GDScriptNewParser::MatchNode *match = cf->match;

				// 	GDScriptNewParser::IdentifierNode *id = memnew(GDScriptNewParser::IdentifierNode);
				// 	id->name = "#match_value";

				// 	// var #match_value
				// 	// copied because there is no _parse_statement :(
				// 	codegen.add_stack_identifier(id->name, p_stack_level++);
				// 	codegen.alloc_stack(p_stack_level);
				// 	new_identifiers++;

				// 	GDScriptNewParser::OperatorNode *op = memnew(GDScriptNewParser::OperatorNode);
				// 	op->op = GDScriptNewParser::OperatorNode::OP_ASSIGN;
				// 	op->arguments.push_back(id);
				// 	op->arguments.push_back(match->val_to_match);

				// 	int ret = _parse_expression(codegen, op, p_stack_level);
				// 	if (ret < 0) {
				// 		memdelete(id);
				// 		memdelete(op);
				// 		return ERR_PARSE_ERROR;
				// 	}

				// 	// break address
				// 	codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				// 	codegen.opcodes.push_back(codegen.opcodes.size() + 3);
				// 	int break_addr = codegen.opcodes.size();
				// 	codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				// 	codegen.opcodes.push_back(0); // break addr

				// 	for (int j = 0; j < match->compiled_pattern_branches.size(); j++) {
				// 		GDScriptNewParser::MatchNode::CompiledPatternBranch branch = match->compiled_pattern_branches[j];

				// 		// jump over continue
				// 		// jump unconditionally
				// 		// continue address
				// 		// compile the condition
				// 		int ret2 = _parse_expression(codegen, branch.compiled_pattern, p_stack_level);
				// 		if (ret2 < 0) {
				// 			memdelete(id);
				// 			memdelete(op);
				// 			return ERR_PARSE_ERROR;
				// 		}

				// 		codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF);
				// 		codegen.opcodes.push_back(ret2);
				// 		codegen.opcodes.push_back(codegen.opcodes.size() + 3);
				// 		int continue_addr = codegen.opcodes.size();
				// 		codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				// 		codegen.opcodes.push_back(0);

				// 		Error err = _parse_block(codegen, branch.body, p_stack_level, p_break_addr, continue_addr);
				// 		if (err) {
				// 			memdelete(id);
				// 			memdelete(op);
				// 			return ERR_PARSE_ERROR;
				// 		}

				// 		codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				// 		codegen.opcodes.push_back(break_addr);

				// 		codegen.opcodes.write[continue_addr + 1] = codegen.opcodes.size();
				// 	}

				// 	codegen.opcodes.write[break_addr + 1] = codegen.opcodes.size();

				// 	memdelete(id);
				// 	memdelete(op);

				// } break;

			case GDScriptNewParser::Node::IF: {
				const GDScriptNewParser::IfNode *if_n = static_cast<const GDScriptNewParser::IfNode *>(s);
				int ret2 = _parse_expression(codegen, if_n->condition, p_stack_level, false);
				if (ret2 < 0) {
					return ERR_PARSE_ERROR;
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF_NOT);
				codegen.opcodes.push_back(ret2);
				int else_addr = codegen.opcodes.size();
				codegen.opcodes.push_back(0); //temporary

				Error err = _parse_block(codegen, if_n->true_block, p_stack_level, p_break_addr, p_continue_addr);
				if (err) {
					return err;
				}

				if (if_n->false_block) {
					codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
					int end_addr = codegen.opcodes.size();
					codegen.opcodes.push_back(0);
					codegen.opcodes.write[else_addr] = codegen.opcodes.size();

					Error err2 = _parse_block(codegen, if_n->false_block, p_stack_level, p_break_addr, p_continue_addr);
					if (err2) {
						return err2;
					}

					codegen.opcodes.write[end_addr] = codegen.opcodes.size();
				} else {
					//end without else
					codegen.opcodes.write[else_addr] = codegen.opcodes.size();
				}

			} break;
			case GDScriptNewParser::Node::FOR: {
				const GDScriptNewParser::ForNode *for_n = static_cast<const GDScriptNewParser::ForNode *>(s);
				int slevel = p_stack_level;
				int iter_stack_pos = slevel;
				int iterator_pos = (slevel++) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
				int counter_pos = (slevel++) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
				int container_pos = (slevel++) | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
				codegen.alloc_stack(slevel);

				codegen.push_stack_identifiers();
				codegen.add_stack_identifier(for_n->variable->name, iter_stack_pos);

				int ret2 = _parse_expression(codegen, for_n->list, slevel, false);
				if (ret2 < 0) {
					return ERR_COMPILATION_FAILED;
				}

				//assign container
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSIGN);
				codegen.opcodes.push_back(container_pos);
				codegen.opcodes.push_back(ret2);

				//begin loop
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_ITERATE_BEGIN);
				codegen.opcodes.push_back(counter_pos);
				codegen.opcodes.push_back(container_pos);
				codegen.opcodes.push_back(codegen.opcodes.size() + 4);
				codegen.opcodes.push_back(iterator_pos);
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP); //skip code for next
				codegen.opcodes.push_back(codegen.opcodes.size() + 8);
				//break loop
				int break_pos = codegen.opcodes.size();
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP); //skip code for next
				codegen.opcodes.push_back(0); //skip code for next
				//next loop
				int continue_pos = codegen.opcodes.size();
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_ITERATE);
				codegen.opcodes.push_back(counter_pos);
				codegen.opcodes.push_back(container_pos);
				codegen.opcodes.push_back(break_pos);
				codegen.opcodes.push_back(iterator_pos);

				Error err = _parse_block(codegen, for_n->loop, slevel, break_pos, continue_pos);
				if (err) {
					return err;
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(continue_pos);
				codegen.opcodes.write[break_pos + 1] = codegen.opcodes.size();

				codegen.pop_stack_identifiers();

			} break;
			case GDScriptNewParser::Node::WHILE: {
				const GDScriptNewParser::WhileNode *while_n = static_cast<const GDScriptNewParser::WhileNode *>(s);
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(codegen.opcodes.size() + 3);
				int break_addr = codegen.opcodes.size();
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(0);
				int continue_addr = codegen.opcodes.size();

				int ret2 = _parse_expression(codegen, while_n->condition, p_stack_level, false);
				if (ret2 < 0) {
					return ERR_PARSE_ERROR;
				}
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_IF_NOT);
				codegen.opcodes.push_back(ret2);
				codegen.opcodes.push_back(break_addr);
				Error err = _parse_block(codegen, while_n->loop, p_stack_level, break_addr, continue_addr);
				if (err) {
					return err;
				}
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(continue_addr);

				codegen.opcodes.write[break_addr + 1] = codegen.opcodes.size();

			} break;
			case GDScriptNewParser::Node::BREAK: {
				if (p_break_addr < 0) {
					_set_error("'break'' not within loop", s);
					return ERR_COMPILATION_FAILED;
				}
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(p_break_addr);

			} break;
			case GDScriptNewParser::Node::CONTINUE: {
				if (p_continue_addr < 0) {
					_set_error("'continue' not within loop", s);
					return ERR_COMPILATION_FAILED;
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP);
				codegen.opcodes.push_back(p_continue_addr);

			} break;
			case GDScriptNewParser::Node::RETURN: {
				const GDScriptNewParser::ReturnNode *return_n = static_cast<const GDScriptNewParser::ReturnNode *>(s);
				int ret2;

				if (return_n->return_value != nullptr) {
					ret2 = _parse_expression(codegen, return_n->return_value, p_stack_level, false);
					if (ret2 < 0) {
						return ERR_PARSE_ERROR;
					}

				} else {
					ret2 = GDScriptFunction::ADDR_TYPE_NIL << GDScriptFunction::ADDR_BITS;
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_RETURN);
				codegen.opcodes.push_back(ret2);

			} break;
			case GDScriptNewParser::Node::ASSERT: {
#ifdef DEBUG_ENABLED
				// try subblocks

				const GDScriptNewParser::AssertNode *as = static_cast<const GDScriptNewParser::AssertNode *>(s);

				int ret2 = _parse_expression(codegen, as->condition, p_stack_level, false);
				if (ret2 < 0) {
					return ERR_PARSE_ERROR;
				}

				int message_ret = 0;
				if (as->message) {
					message_ret = _parse_expression(codegen, as->message, p_stack_level + 1, false);
					if (message_ret < 0) {
						return ERR_PARSE_ERROR;
					}
				}

				codegen.opcodes.push_back(GDScriptFunction::OPCODE_ASSERT);
				codegen.opcodes.push_back(ret2);
				codegen.opcodes.push_back(message_ret);
#endif
			} break;
			case GDScriptNewParser::Node::BREAKPOINT: {
#ifdef DEBUG_ENABLED
				// try subblocks
				codegen.opcodes.push_back(GDScriptFunction::OPCODE_BREAKPOINT);
#endif
			} break;
			case GDScriptNewParser::Node::VARIABLE: {
				const GDScriptNewParser::VariableNode *lv = static_cast<const GDScriptNewParser::VariableNode *>(s);

				// since we are using properties now for most class access, allow shadowing of class members to make user's life easier.
				//
				//if (_is_class_member_property(codegen, lv->name)) {
				//	_set_error("Name for local variable '" + String(lv->name) + "' can't shadow class property of the same name.", lv);
				//	return ERR_ALREADY_EXISTS;
				//}

				codegen.add_stack_identifier(lv->identifier->name, p_stack_level++);
				codegen.alloc_stack(p_stack_level);
				new_identifiers++;
				// TODO: Initialization.

			} break;
			case GDScriptNewParser::Node::PASS:
				// Nothing to do.
				break;
			default: {
				//expression
				if (s->is_expression()) {
					int ret2 = _parse_expression(codegen, static_cast<const GDScriptNewParser::ExpressionNode *>(s), p_stack_level, true);
					if (ret2 < 0) {
						return ERR_PARSE_ERROR;
					}
				} else {
					ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Bug in bytecode compiler, unexpected node in parse tree while parsing statement."); //unreachable code
				}
			} break;
		}
	}
	codegen.pop_stack_identifiers();
	return OK;
}

Error GDScriptNewCompiler::_parse_function(GDScript *p_script, const GDScriptNewParser::ClassNode *p_class, const GDScriptNewParser::FunctionNode *p_func, bool p_for_ready) {
	Vector<int> bytecode;
	CodeGen codegen;

	codegen.class_node = p_class;
	codegen.script = p_script;
	codegen.function_node = p_func;
	codegen.stack_max = 0;
	codegen.current_line = 0;
	codegen.call_max = 0;
	codegen.debug_stack = EngineDebugger::is_active();
	Vector<StringName> argnames;

	int stack_level = 0;
	int optional_parameters = 0;

	if (p_func) {
		for (int i = 0; i < p_func->parameters.size(); i++) {
			// since we are using properties now for most class access, allow shadowing of class members to make user's life easier.
			//
			//if (_is_class_member_property(p_script, p_func->arguments[i])) {
			//	_set_error("Name for argument '" + String(p_func->arguments[i]) + "' can't shadow class property of the same name.", p_func);
			//	return ERR_ALREADY_EXISTS;
			//}

			codegen.add_stack_identifier(p_func->parameters[i]->identifier->name, i);
#ifdef TOOLS_ENABLED
			argnames.push_back(p_func->parameters[i]->identifier->name);
#endif
			if (p_func->parameters[i]->default_value != nullptr) {
				optional_parameters++;
			}
		}
		stack_level = p_func->parameters.size();
	}

	codegen.alloc_stack(stack_level);

	/* Parse initializer -if applies- */

	bool is_initializer = !p_for_ready && !p_func;

	if (is_initializer || (p_func && String(p_func->identifier->name) == "_init")) {
		//parse initializer for class members
		if (!p_func && p_class->extends_used && p_script->native.is_null()) {
			//call implicit parent constructor
			codegen.opcodes.push_back(GDScriptFunction::OPCODE_CALL_SELF_BASE);
			codegen.opcodes.push_back(codegen.get_name_map_pos("_init"));
			codegen.opcodes.push_back(0);
			codegen.opcodes.push_back((GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS) | 0);
		}
		// FIXME: Implicit initializer.
		if (p_func) {
			Error err = _parse_block(codegen, p_func->body, stack_level);
			if (err) {
				return err;
			}
		}
		is_initializer = true;
	}

	if (p_for_ready || (p_func && String(p_func->identifier->name) == "_ready")) {
		//parse initializer for class members
		// FIXME: @onready stuff.
		// if (p_class->ready->statements.size()) {
		// 	Error err = _parse_block(codegen, p_class->ready, stack_level);
		// 	if (err) {
		// 		return err;
		// 	}
		// }
	}

	/* Parse default argument code -if applies- */

	Vector<int> defarg_addr;
	StringName func_name;

	if (p_func) {
		if (optional_parameters > 0) {
			codegen.opcodes.push_back(GDScriptFunction::OPCODE_JUMP_TO_DEF_ARGUMENT);
			defarg_addr.push_back(codegen.opcodes.size());
			for (int i = p_func->parameters.size() - optional_parameters; i < p_func->parameters.size(); i++) {
				_parse_expression(codegen, p_func->parameters[i]->default_value, stack_level, true);
				defarg_addr.push_back(codegen.opcodes.size());
			}

			defarg_addr.invert();
		}

		Error err = _parse_block(codegen, p_func->body, stack_level);
		if (err) {
			return err;
		}

		func_name = p_func->identifier->name;
	} else {
		if (p_for_ready) {
			func_name = "_ready";
		} else {
			func_name = "_init";
		}
	}

	codegen.opcodes.push_back(GDScriptFunction::OPCODE_END);

	/*
	if (String(p_func->name)=="") { //initializer func
		gdfunc = &p_script->initializer;
	*/
	//} else { //regular func
	p_script->member_functions[func_name] = memnew(GDScriptFunction);
	GDScriptFunction *gdfunc = p_script->member_functions[func_name];
	//}

	if (p_func) {
		gdfunc->_static = p_func->is_static;
		gdfunc->rpc_mode = p_func->rpc_mode;
		// FIXME: Add actual parameters types;
		gdfunc->argument_types.resize(p_func->parameters.size());
		for (int i = 0; i < p_func->parameters.size(); i++) {
			gdfunc->argument_types.write[i] = GDScriptDataType(); //_gdtype_from_datatype(p_func->argument_types[i]);
		}
		gdfunc->return_type = GDScriptDataType(); //_gdtype_from_datatype(p_func->return_type);
	} else {
		gdfunc->_static = false;
		gdfunc->rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;
		gdfunc->return_type = GDScriptDataType();
		gdfunc->return_type.has_type = true;
		gdfunc->return_type.kind = GDScriptDataType::BUILTIN;
		gdfunc->return_type.builtin_type = Variant::NIL;
	}

#ifdef TOOLS_ENABLED
	gdfunc->arg_names = argnames;
#endif
	//constants
	if (codegen.constant_map.size()) {
		gdfunc->_constant_count = codegen.constant_map.size();
		gdfunc->constants.resize(codegen.constant_map.size());
		gdfunc->_constants_ptr = gdfunc->constants.ptrw();
		const Variant *K = nullptr;
		while ((K = codegen.constant_map.next(K))) {
			int idx = codegen.constant_map[*K];
			gdfunc->constants.write[idx] = *K;
		}
	} else {
		gdfunc->_constants_ptr = nullptr;
		gdfunc->_constant_count = 0;
	}
	//global names
	if (codegen.name_map.size()) {
		gdfunc->global_names.resize(codegen.name_map.size());
		gdfunc->_global_names_ptr = &gdfunc->global_names[0];
		for (Map<StringName, int>::Element *E = codegen.name_map.front(); E; E = E->next()) {
			gdfunc->global_names.write[E->get()] = E->key();
		}
		gdfunc->_global_names_count = gdfunc->global_names.size();

	} else {
		gdfunc->_global_names_ptr = nullptr;
		gdfunc->_global_names_count = 0;
	}

#ifdef TOOLS_ENABLED
	// Named globals
	if (codegen.named_globals.size()) {
		gdfunc->named_globals.resize(codegen.named_globals.size());
		gdfunc->_named_globals_ptr = gdfunc->named_globals.ptr();
		for (int i = 0; i < codegen.named_globals.size(); i++) {
			gdfunc->named_globals.write[i] = codegen.named_globals[i];
		}
		gdfunc->_named_globals_count = gdfunc->named_globals.size();
	}
#endif

	if (codegen.opcodes.size()) {
		gdfunc->code = codegen.opcodes;
		gdfunc->_code_ptr = &gdfunc->code[0];
		gdfunc->_code_size = codegen.opcodes.size();

	} else {
		gdfunc->_code_ptr = nullptr;
		gdfunc->_code_size = 0;
	}

	if (defarg_addr.size()) {
		gdfunc->default_arguments = defarg_addr;
		gdfunc->_default_arg_count = defarg_addr.size() - 1;
		gdfunc->_default_arg_ptr = &gdfunc->default_arguments[0];
	} else {
		gdfunc->_default_arg_count = 0;
		gdfunc->_default_arg_ptr = nullptr;
	}

	gdfunc->_argument_count = p_func ? p_func->parameters.size() : 0;
	gdfunc->_stack_size = codegen.stack_max;
	gdfunc->_call_size = codegen.call_max;
	gdfunc->name = func_name;
#ifdef DEBUG_ENABLED
	if (EngineDebugger::is_active()) {
		String signature;
		//path
		if (p_script->get_path() != String()) {
			signature += p_script->get_path();
		}
		//loc
		if (p_func) {
			signature += "::" + itos(p_func->body->start_line);
		} else {
			signature += "::0";
		}

		//function and class

		if (p_class->identifier) {
			signature += "::" + String(p_class->identifier->name) + "." + String(func_name);
		} else {
			signature += "::" + String(func_name);
		}

		gdfunc->profile.signature = signature;
	}
#endif
	gdfunc->_script = p_script;
	gdfunc->source = source;

#ifdef DEBUG_ENABLED

	{
		gdfunc->func_cname = (String(source) + " - " + String(func_name)).utf8();
		gdfunc->_func_cname = gdfunc->func_cname.get_data();
	}

#endif
	if (p_func) {
		gdfunc->_initial_line = p_func->start_line;
#ifdef TOOLS_ENABLED

		p_script->member_lines[func_name] = p_func->start_line;
#endif
	} else {
		gdfunc->_initial_line = 0;
	}

	if (codegen.debug_stack) {
		gdfunc->stack_debug = codegen.stack_debug;
	}

	if (is_initializer) {
		p_script->initializer = gdfunc;
	}

	return OK;
}

Error GDScriptNewCompiler::_parse_class_level(GDScript *p_script, const GDScriptNewParser::ClassNode *p_class, bool p_keep_state) {
	parsing_classes.insert(p_script);

	if (p_class->outer && p_class->outer->outer) {
		// Owner is not root
		if (!parsed_classes.has(p_script->_owner)) {
			if (parsing_classes.has(p_script->_owner)) {
				_set_error("Cyclic class reference for '" + String(p_class->identifier->name) + "'.", p_class);
				return ERR_PARSE_ERROR;
			}
			Error err = _parse_class_level(p_script->_owner, p_class->outer, p_keep_state);
			if (err) {
				return err;
			}
		}
	}

	p_script->native = Ref<GDScriptNativeClass>();
	p_script->base = Ref<GDScript>();
	p_script->_base = nullptr;
	p_script->members.clear();
	p_script->constants.clear();
	for (Map<StringName, GDScriptFunction *>::Element *E = p_script->member_functions.front(); E; E = E->next()) {
		memdelete(E->get());
	}
	p_script->member_functions.clear();
	p_script->member_indices.clear();
	p_script->member_info.clear();
	p_script->_signals.clear();
	p_script->initializer = nullptr;

	p_script->tool = parser->is_tool();
	p_script->name = p_class->identifier ? p_class->identifier->name : "";

	Ref<GDScriptNativeClass> native;

	GDScriptDataType base_type = _gdtype_from_datatype(p_class->base_type);
	// Inheritance
	switch (base_type.kind) {
		case GDScriptDataType::NATIVE: {
			int native_idx = GDScriptLanguage::get_singleton()->get_global_map()[base_type.native_type];
			native = GDScriptLanguage::get_singleton()->get_global_array()[native_idx];
			ERR_FAIL_COND_V(native.is_null(), ERR_BUG);
			p_script->native = native;
		} break;
		case GDScriptDataType::GDSCRIPT: {
			Ref<GDScript> base = base_type.script_type;
			p_script->base = base;
			p_script->_base = base.ptr();
			p_script->member_indices = base->member_indices;

			if (p_class->base_type.kind == GDScriptNewParser::DataType::GDSCRIPT) {
				if (!parsed_classes.has(p_script->_base)) {
					if (parsing_classes.has(p_script->_base)) {
						String class_name = p_class->identifier ? p_class->identifier->name : "<main>";
						_set_error("Cyclic class reference for '" + class_name + "'.", p_class);
						return ERR_PARSE_ERROR;
					}
					Error err = _parse_class_level(p_script->_base, p_class->base_type.gdscript_type, p_keep_state);
					if (err) {
						return err;
					}
				}
			}
		} break;
		default: {
			_set_error("Parser bug: invalid inheritance.", p_class);
			return ERR_BUG;
		} break;
	}

	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptNewParser::ClassNode::Member &member = p_class->members[i];
		if (member.type == member.VARIABLE) {
			const GDScriptNewParser::VariableNode *variable = member.variable;
			StringName name = variable->identifier->name;

			GDScript::MemberInfo minfo;
			minfo.index = p_script->member_indices.size();
			// FIXME: Getter and setter.
			// minfo.setter = p_class->variables[i].setter;
			// minfo.getter = p_class->variables[i].getter;
			minfo.rpc_mode = variable->rpc_mode;
			// FIXME: Types.
			// minfo.data_type = _gdtype_from_datatype(p_class->variables[i].data_type);

			PropertyInfo prop_info = minfo.data_type;
			prop_info.name = name;
			PropertyInfo export_info = variable->export_info;

			if (export_info.type != Variant::NIL) {
				if (!minfo.data_type.has_type) {
					prop_info.type = export_info.type;
					prop_info.class_name = export_info.class_name;
				}
				prop_info.hint = export_info.hint;
				prop_info.hint_string = export_info.hint_string;
				prop_info.usage = export_info.usage;
#ifdef TOOLS_ENABLED
				if (variable->initializer != nullptr && variable->initializer->type == GDScriptNewParser::Node::LITERAL) {
					p_script->member_default_values[name] = static_cast<const GDScriptNewParser::LiteralNode *>(variable->initializer)->value;
				}
#endif
			} else {
				prop_info.usage = PROPERTY_USAGE_SCRIPT_VARIABLE;
			}

			p_script->member_info[name] = prop_info;
			p_script->member_indices[name] = minfo;
			p_script->members.insert(name);

#ifdef TOOLS_ENABLED
			p_script->member_lines[name] = variable->start_line;
#endif
		}

		if (member.type == member.CONSTANT) {
			const GDScriptNewParser::ConstantNode *constant = member.constant;
			StringName name = constant->identifier->name;

			ERR_CONTINUE(constant->initializer->type != GDScriptNewParser::Node::LITERAL);

			const GDScriptNewParser::LiteralNode *literal = static_cast<const GDScriptNewParser::LiteralNode *>(constant->initializer);

			p_script->constants.insert(name, literal->value);
#ifdef TOOLS_ENABLED

			p_script->member_lines[name] = constant->start_line;
#endif
		}

		if (member.type == member.SIGNAL) {
			const GDScriptNewParser::SignalNode *signal = member.signal;
			StringName name = signal->identifier->name;

			GDScript *c = p_script;

			while (c) {
				if (c->_signals.has(name)) {
					_set_error("Signal '" + name + "' redefined (in current or parent class)", p_class);
					return ERR_ALREADY_EXISTS;
				}

				if (c->base.is_valid()) {
					c = c->base.ptr();
				} else {
					c = nullptr;
				}
			}

			if (native.is_valid()) {
				if (ClassDB::has_signal(native->get_name(), name)) {
					_set_error("Signal '" + name + "' redefined (original in native class '" + String(native->get_name()) + "')", p_class);
					return ERR_ALREADY_EXISTS;
				}
			}

			Vector<StringName> parameters_names;
			parameters_names.resize(signal->parameters.size());
			for (int j = 0; j < signal->parameters.size(); j++) {
				parameters_names.write[j] = signal->parameters[j]->identifier->name;
			}
			p_script->_signals[name] = parameters_names;
		}
	}

	parsed_classes.insert(p_script);
	parsing_classes.erase(p_script);

	//parse sub-classes

	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptNewParser::ClassNode::Member &member = p_class->members[i];
		if (member.type != member.CLASS) {
			continue;
		}
		const GDScriptNewParser::ClassNode *inner_class = member.m_class;
		StringName name = inner_class->identifier->name;
		Ref<GDScript> &subclass = p_script->subclasses[name];
		GDScript *subclass_ptr = subclass.ptr();

		// Subclass might still be parsing, just skip it
		if (!parsed_classes.has(subclass_ptr) && !parsing_classes.has(subclass_ptr)) {
			Error err = _parse_class_level(subclass_ptr, inner_class, p_keep_state);
			if (err) {
				return err;
			}
		}

#ifdef TOOLS_ENABLED

		p_script->member_lines[name] = inner_class->start_line;
#endif

		p_script->constants.insert(name, subclass); //once parsed, goes to the list of constants
	}

	return OK;
}

Error GDScriptNewCompiler::_parse_class_blocks(GDScript *p_script, const GDScriptNewParser::ClassNode *p_class, bool p_keep_state) {
	//parse methods

	bool has_initializer = false;
	bool has_ready = false;

	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptNewParser::ClassNode::Member &member = p_class->members[i];
		if (member.type != member.FUNCTION) {
			continue;
		}
		const GDScriptNewParser::FunctionNode *function = member.function;
		if (!has_initializer && function->identifier->name == "_init") {
			has_initializer = true;
		}
		if (!has_ready && function->identifier->name == "_ready") {
			has_ready = true;
		}
		Error err = _parse_function(p_script, p_class, function);
		if (err) {
			return err;
		}
	}

	if (!has_initializer) {
		//create a constructor
		Error err = _parse_function(p_script, p_class, nullptr);
		if (err) {
			return err;
		}
	}

	// FIXME: Add @onready.
	// if (!has_ready && p_class->ready->statements.size()) {
	// 	//create a constructor
	// 	Error err = _parse_function(p_script, p_class, nullptr, true);
	// 	if (err) {
	// 		return err;
	// 	}
	// }

#ifdef DEBUG_ENABLED

	//validate instances if keeping state

	if (p_keep_state) {
		for (Set<Object *>::Element *E = p_script->instances.front(); E;) {
			Set<Object *>::Element *N = E->next();

			ScriptInstance *si = E->get()->get_script_instance();
			if (si->is_placeholder()) {
#ifdef TOOLS_ENABLED
				PlaceHolderScriptInstance *psi = static_cast<PlaceHolderScriptInstance *>(si);

				if (p_script->is_tool()) {
					//re-create as an instance
					p_script->placeholders.erase(psi); //remove placeholder

					GDScriptInstance *instance = memnew(GDScriptInstance);
					instance->base_ref = Object::cast_to<Reference>(E->get());
					instance->members.resize(p_script->member_indices.size());
					instance->script = Ref<GDScript>(p_script);
					instance->owner = E->get();

					//needed for hot reloading
					for (Map<StringName, GDScript::MemberInfo>::Element *F = p_script->member_indices.front(); F; F = F->next()) {
						instance->member_indices_cache[F->key()] = F->get().index;
					}
					instance->owner->set_script_instance(instance);

					/* STEP 2, INITIALIZE AND CONSTRUCT */

					Callable::CallError ce;
					p_script->initializer->call(instance, nullptr, 0, ce);

					if (ce.error != Callable::CallError::CALL_OK) {
						//well, tough luck, not goinna do anything here
					}
				}
#endif
			} else {
				GDScriptInstance *gi = static_cast<GDScriptInstance *>(si);
				gi->reload_members();
			}

			E = N;
		}
	}
#endif

	for (int i = 0; i < p_class->members.size(); i++) {
		if (p_class->members[i].type != GDScriptNewParser::ClassNode::Member::CLASS) {
			continue;
		}
		const GDScriptNewParser::ClassNode *inner_class = p_class->members[i].m_class;
		StringName name = inner_class->identifier->name;
		GDScript *subclass = p_script->subclasses[name].ptr();

		Error err = _parse_class_blocks(subclass, inner_class, p_keep_state);
		if (err) {
			return err;
		}
	}

	p_script->valid = true;
	return OK;
}

void GDScriptNewCompiler::_make_scripts(GDScript *p_script, const GDScriptNewParser::ClassNode *p_class, bool p_keep_state) {
	Map<StringName, Ref<GDScript>> old_subclasses;

	if (p_keep_state) {
		old_subclasses = p_script->subclasses;
	}

	p_script->subclasses.clear();

	for (int i = 0; i < p_class->members.size(); i++) {
		if (p_class->members[i].type != GDScriptNewParser::ClassNode::Member::CLASS) {
			continue;
		}
		const GDScriptNewParser::ClassNode *inner_class = p_class->members[i].m_class;
		StringName name = inner_class->identifier->name;

		Ref<GDScript> subclass;
		String fully_qualified_name = p_script->fully_qualified_name + "::" + name;

		if (old_subclasses.has(name)) {
			subclass = old_subclasses[name];
		} else {
			Ref<GDScript> orphan_subclass = GDScriptLanguage::get_singleton()->get_orphan_subclass(fully_qualified_name);
			if (orphan_subclass.is_valid()) {
				subclass = orphan_subclass;
			} else {
				subclass.instance();
			}
		}

		subclass->_owner = p_script;
		subclass->fully_qualified_name = fully_qualified_name;
		p_script->subclasses.insert(name, subclass);

		_make_scripts(subclass.ptr(), inner_class, false);
	}
}

Error GDScriptNewCompiler::compile(const GDScriptNewParser *p_parser, GDScript *p_script, bool p_keep_state) {
	err_line = -1;
	err_column = -1;
	error = "";
	parser = p_parser;
	main_script = p_script;
	const GDScriptNewParser::ClassNode *root = parser->get_tree();

	source = p_script->get_path();

	// The best fully qualified name for a base level script is its file path
	p_script->fully_qualified_name = p_script->path;

	// Create scripts for subclasses beforehand so they can be referenced
	_make_scripts(p_script, root, p_keep_state);

	p_script->_owner = nullptr;
	Error err = _parse_class_level(p_script, root, p_keep_state);

	if (err) {
		return err;
	}

	err = _parse_class_blocks(p_script, root, p_keep_state);

	if (err) {
		return err;
	}

	return OK;
}

String GDScriptNewCompiler::get_error() const {
	return error;
}

int GDScriptNewCompiler::get_error_line() const {
	return err_line;
}

int GDScriptNewCompiler::get_error_column() const {
	return err_column;
}

GDScriptNewCompiler::GDScriptNewCompiler() {
}
