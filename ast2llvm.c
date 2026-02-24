#include <string.h>
#include <stdarg.h>
#include "ast2llvm.h"
#include "set.h"
#include "message.h"
#include "ast/print.h"

typedef enum {
	LLVM_TVALUE_INT_CONST = LLVM_VALUE_INT_CONST,
	LLVM_TVALUE_DOUBLE_CONST = LLVM_VALUE_DOUBLE_CONST,
	LLVM_TVALUE_REG = LLVM_VALUE_REG,
	LLVM_TVALUE_POISON = LLVM_VALUE_POISON,
} llvm_typed_value_enum_t;

typedef struct llvm_typed_value_t {
	llvm_typed_value_enum_t type;
	const ast_datatype_t* ast_type; // nullable for POISON (when type cannot be deduced) and INT_CONST (when type isn't specified)
	union {
		bignum_t* int_const;
		double double_const;
		llvm_reg_t reg;
	};
} llvm_typed_value_t;

typedef const ast_variable_t* ast_variable_ptr;
create_hashmap_type_header(ast_variable_ptr, llvm_reg_t, var2reg_map);

create_list_type_header(llvm_typed_value, false);
create_list_type_impl(llvm_typed_value, false)

#define LLVM_TYPED_POISON(t) ((llvm_typed_value_t){ .type=LLVM_TVALUE_POISON, .ast_type=t })
#define LLVM_TYPED_REG(r, t) ((llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .reg=r, .ast_type=t })
#define POISON_VALUE ((llvm_value_t){ .type=LLVM_VALUE_POISON })

static llvm_label_t llvm_function_last_label(const llvm_function_t* f){
	return (llvm_label_t){ .idx = f->blocks.len - 1 };
}
static llvm_basic_block_t* llvm_function_last_block(llvm_function_t* f){
	return &f->blocks.data[f->blocks.len - 1];
}
static llvm_reg_t llvm_block_last_reg(const llvm_basic_block_t* b){
	return (llvm_reg_t){ .idx = b->regbase + b->instructions.len - 1};
}
static llvm_basic_block_t* llvm_get_block(llvm_function_t* f, llvm_label_t label){
	return &f->blocks.data[label.idx];
}
static llvm_value_t llvm_untype_value(const llvm_typed_value_t tv){
	llvm_value_t v;
	switch (tv.type){
		case LLVM_TVALUE_INT_CONST:
			v.type = LLVM_VALUE_INT_CONST;
			v.int_const = tv.int_const;
			break;
		case LLVM_TVALUE_DOUBLE_CONST:
			v.type = LLVM_VALUE_DOUBLE_CONST;
			v.double_const = tv.double_const;
			break;
		case LLVM_TVALUE_REG:
			v.type = LLVM_VALUE_REG;
			v.reg = tv.reg;
			break;
		case LLVM_TVALUE_POISON:
			v.type = LLVM_VALUE_POISON;
			break;
	}
	return v;
}
static ast_datatype_t* find_ast_type(const scope_t* global_scope, const char* name){ // TODO: maybe built-in types can be stored globally somewhere?
	return &scope_get(global_scope, name)->type_;
}

static unsigned long ast_variable_ptr_hash(ast_variable_ptr p){
	unsigned long h = 1337;
	while (p){
		h = (h*6364136223846793005) + (unsigned long)p;
		p = (void*)((unsigned long)p >> 8);
	}
	return h;
}
static inline bool ast_variable_ptr_cmp(ast_variable_ptr a, ast_variable_ptr b){ return a != b; }
create_hashmap_type_impl(ast_variable_ptr, llvm_reg_t, var2reg_map)
create_set_type_header(ast_variable_ptr);
create_set_type_impl(ast_variable_ptr)

create_list_type_impl(llvm_inst, false)
create_list_type_impl(llvm_basic_block, false)

static llvm_label_t llvm_add_block(llvm_function_t* f){
	llvm_basic_block_t* lastblock = &f->blocks.data[f->blocks.len - 1];

	llvm_basic_block_list_append(&f->blocks, (llvm_basic_block_t){
		.regbase = lastblock->regbase + lastblock->instructions.len,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});
	return llvm_function_last_label(f);
}

static llvm_reg_t llvm_add_inst(llvm_function_t* f, llvm_inst_t inst){
	llvm_basic_block_t* block = &f->blocks.data[f->blocks.len-1];
	llvm_inst_list_append(&block->instructions, inst);
	return llvm_block_last_reg(block);
}

static llvm_type_t ast_type_to_llvm_type(const ast_datatype_t* t){
	switch (t->kind){
		case AST_DATATYPE_FLOAT:
			return (llvm_type_t){ .type=LLVM_TYPE_FLOAT, .bitwidth=t->floating.bitwidth };
		case AST_DATATYPE_INTEGRAL:
			return (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .bitwidth=t->integral.bitwidth };
		case AST_DATATYPE_STRUCTURED:
		{
			llvm_type_t type = (llvm_type_t){
				.type=LLVM_TYPE_STRUCT,
				.structure.member_count = t->structure.members.len,
				.structure.member_types = malloc(sizeof(llvm_type_t)*t->structure.members.len)
			};
			for (unsigned int i = 0; i < type.structure.member_count; i++){
				type.structure.member_types[i] = ast_type_to_llvm_type(t->structure.members.data[i].type_ref);
			}
			return type;
		}
		case AST_DATATYPE_POINTER:
			return (llvm_type_t){ .type=LLVM_TYPE_POINTER };
		case AST_DATATYPE_VOID:
			break;
	}
	puts("INTERNAL ERROR");
	exit(1);
}

static llvm_typed_value_t ast2llvm_read_lvalue(const var2reg_map_t* var2reg, const ast_lvalue_t lvalue, llvm_function_t* f){
	llvm_reg_t reg = var2reg_map_get(var2reg, lvalue.base_var, LLVM_INVALID_REG);
	llvm_value_t base_ptr;
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG)){
		base_ptr = (llvm_value_t){
			.type = LLVM_VALUE_GLOBAL,
			.global_name = strdup(lvalue.base_var->name),
		};
	}else{
		base_ptr = (llvm_value_t){
			.type = LLVM_VALUE_REG,
			.reg = reg
		};
	}
	const ast_datatype_t* type = lvalue.base_var->type_ref;
	reg = llvm_add_inst(f, (llvm_inst_t){
		.type = LLVM_INST_LOAD,
		.load.ptr = base_ptr,
		.load.type = ast_type_to_llvm_type(type)
	});

	for (unsigned int i=0; i < lvalue.member_access.len; i++){
		ast_lvalue_member_access_t member_access = lvalue.member_access.data[i];
		if (member_access.deref){
			type = type->pointer.base;
			reg = llvm_add_inst(f, (llvm_inst_t){
				.type = LLVM_INST_LOAD,
				.load.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg },
				.load.type = ast_type_to_llvm_type(type)
			});
		}
		reg = llvm_add_inst(f, (llvm_inst_t){
			.type = LLVM_INST_EXTRACT_VALUE,
			.extract.aggregate_type = ast_type_to_llvm_type(type),
			.extract.value = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg },
			.extract.member_idx = member_access.member_idx
		});
		type = type->structure.members.data[member_access.member_idx].type_ref;
	}

	return LLVM_TYPED_REG(reg, type);
}

static llvm_typed_value_t ast2llvm_get_lvalue_pointer(const var2reg_map_t* var2reg, const ast_lvalue_t lvalue, llvm_function_t* f){
	llvm_reg_t reg = var2reg_map_get(var2reg, lvalue.base_var, LLVM_INVALID_REG);
	llvm_value_t ptr;
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG)){ // global variable
		ptr = (llvm_value_t){ .type=LLVM_VALUE_GLOBAL, .global_name=strdup(lvalue.base_var->name) };
	}else{
		ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg };
	}
	ast_datatype_t* type = lvalue.base_var->type_ref; // the type that 'reg' is pointing to

	for (unsigned int i=0; i < lvalue.member_access.len; i++){
		ast_lvalue_member_access_t member_access = lvalue.member_access.data[i];
		if (member_access.deref){
			reg = llvm_add_inst(f, (llvm_inst_t){
				.type = LLVM_INST_LOAD,
				.load.ptr = ptr,
				.load.type = ast_type_to_llvm_type(type)
			});
			ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg };
			type = type->pointer.base;
		}
		reg = llvm_add_inst(f, (llvm_inst_t){
			.type = LLVM_INST_GET_ELEMENT_PTR,
			.getelementptr.aggregate_type = ast_type_to_llvm_type(type),
			.getelementptr.ptr = ptr,
			.getelementptr.member_idx = member_access.member_idx
		});
		ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg };
		type = type->structure.members.data[member_access.member_idx].type_ref;
	}

	return LLVM_TYPED_REG(reg, get_ast_pointer_type(type));
}

static void ast2llvm_alloca_scope(const scope_t* scope, var2reg_map_t* var2reg, llvm_function_t* f){
	for (scope_iterator_t iter = scope_iter(scope); iter.current; iter = scope_iter_next(iter)){
		ast_id_t* id = iter.current->value;
		if (id->type != AST_ID_VAR) continue;
		ast_variable_t* var = &id->var;
		llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
			.type = LLVM_INST_ALLOCA,
			.alloca.type = ast_type_to_llvm_type(var->type_ref)
		});
		var2reg_map_insert(var2reg, var, reg);
	}
}

static void ast2llvm_alloca_stmt(const ast_stmt_t* stmt, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (stmt->type){
		case AST_STMT_IF:
			ast2llvm_alloca_stmt(stmt->if_.iftrue, var2reg, f);
			break;
		case AST_STMT_IF_ELSE:
			ast2llvm_alloca_stmt(stmt->if_else.iftrue, var2reg, f);
			ast2llvm_alloca_stmt(stmt->if_else.iffalse, var2reg, f);
			break;
		case AST_STMT_BLOCK:
			if (stmt->block.local_scope)
				ast2llvm_alloca_scope(stmt->block.local_scope, var2reg, f);
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++)
				ast2llvm_alloca_stmt(&stmt->block.stmtlist.data[i], var2reg, f);
			break;
		case AST_STMT_FOR:
			ast2llvm_alloca_scope(stmt->for_.local_scope, var2reg, f);
			ast2llvm_alloca_stmt(stmt->for_.step, var2reg, f);
			ast2llvm_alloca_stmt(stmt->for_.init, var2reg, f);
			ast2llvm_alloca_stmt(stmt->for_.body, var2reg, f);
			break;
		case AST_STMT_WHILE:
			ast2llvm_alloca_stmt(stmt->while_.body, var2reg, f);
			break;
		case AST_STMT_EXPR:
		case AST_STMT_ASSIGN:
		case AST_STMT_BREAK:
		case AST_STMT_CONTINUE:
		case AST_STMT_RETURN:
			break;
	}
}

// if err == 0, print an error if cannot cast
// else, set *err to whether cast failed, and cast is treated as implicit
static llvm_typed_value_t ast2llvm_cast(llvm_typed_value_t value, const ast_datatype_t* trgt_type, llvm_function_t* f, bool* err, loc_t loc){
	if (err) *err = false;
	const bool implicit = err ? true : false;
	const char* warning_string = 0;
	const ast_datatype_t* src_type = value.ast_type;

	if (value.type == LLVM_TVALUE_POISON && src_type == 0)
		goto no_cast;

	if (value.type == LLVM_TVALUE_INT_CONST && src_type == 0){
		switch (trgt_type->kind){
			case AST_DATATYPE_VOID: goto error;
			case AST_DATATYPE_POINTER: // TODO
				goto error;
			case AST_DATATYPE_FLOAT:
			{
				double x = bignum_to_double(value.int_const);
				free_bignum(value.int_const);
				value = (llvm_typed_value_t){ .type=LLVM_TVALUE_DOUBLE_CONST, .double_const=x, .ast_type=trgt_type };
				goto no_cast;
			}
			case AST_DATATYPE_STRUCTURED:
				if (err){
					*err = true;
				}else{
					printf_error(loc, "cannot cast integer constant to type '%s'", trgt_type->name);
				}
				return LLVM_TYPED_POISON(trgt_type);
			case AST_DATATYPE_INTEGRAL:
			{
				bool changed = bignum_trunc(value.int_const, trgt_type->integral.bitwidth, trgt_type->integral.signed_);
				if (changed){
					char* str = bignum_to_string(value.int_const);
					printf_warning(loc, "casting integer constant to type '%s' changes value to '%s'", trgt_type->name, str);
					free(str);
				}
				goto no_cast;
			}
		}
	}

	if (ast_datatype_eq(src_type, trgt_type))
		return value;

	switch (src_type->kind){
		case AST_DATATYPE_VOID: goto error;
		case AST_DATATYPE_POINTER:
			switch (trgt_type->kind){
				case AST_DATATYPE_VOID: goto error;
				case AST_DATATYPE_POINTER:
					goto no_cast;
				case AST_DATATYPE_STRUCTURED:
				case AST_DATATYPE_FLOAT:
					goto error;
				case AST_DATATYPE_INTEGRAL:
				{
					llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
						.type = LLVM_INST_PTR_TO_INT,
						.conversion.from = ast_type_to_llvm_type(src_type),
						.conversion.to = ast_type_to_llvm_type(trgt_type),	
						.conversion.value = llvm_untype_value(value)
					});
					value = LLVM_TYPED_REG(reg, trgt_type);

					// TODO: warn if cast truncates
					if (trgt_type->integral.signed_){
						warning_string = "casting ptr to signed integer";
						goto implicit_warning;
					}
					goto no_cast;
				}
			}
		case AST_DATATYPE_STRUCTURED:
			goto error; // case where struct types are the same is already handled
		case AST_DATATYPE_FLOAT:
			switch (trgt_type->kind){
				case AST_DATATYPE_VOID: goto error;
				case AST_DATATYPE_FLOAT:
					if (src_type->floating.bitwidth < trgt_type->floating.bitwidth){
						llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
							.type = LLVM_INST_FPEXT,
							.conversion.from = ast_type_to_llvm_type(src_type),
							.conversion.to = ast_type_to_llvm_type(trgt_type),	
							.conversion.value = llvm_untype_value(value)
						});
						value = LLVM_TYPED_REG(reg, trgt_type);
						goto no_cast;
					}
					if (src_type->floating.bitwidth > trgt_type->floating.bitwidth){
						llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
							.type = LLVM_INST_FPTRUNC,
							.conversion.from = ast_type_to_llvm_type(src_type),
							.conversion.to = ast_type_to_llvm_type(trgt_type),	
							.conversion.value = llvm_untype_value(value)
						});
						value = LLVM_TYPED_REG(reg, trgt_type);
						warning_string = "floating point truncation";
						goto implicit_warning;
					}
					goto no_cast;
				case AST_DATATYPE_POINTER:
				case AST_DATATYPE_STRUCTURED:
					goto error;
				case AST_DATATYPE_INTEGRAL:
				{
					llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
						.type = trgt_type->integral.signed_ ? LLVM_INST_FLOAT_TO_SINT : LLVM_INST_FLOAT_TO_UINT,
						.conversion.from = ast_type_to_llvm_type(src_type),
						.conversion.to = ast_type_to_llvm_type(trgt_type),	
						.conversion.value = llvm_untype_value(value)
					});
					value = LLVM_TYPED_REG(reg, trgt_type);
					goto no_cast;
				}
			}
			puts("internal error"); exit(1);
		case AST_DATATYPE_INTEGRAL:
			switch (trgt_type->kind){
				case AST_DATATYPE_VOID: goto error;
				case AST_DATATYPE_POINTER:
				{
					llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
						.type = LLVM_INST_INT_TO_PTR,
						.conversion.from = ast_type_to_llvm_type(src_type),
						.conversion.to = ast_type_to_llvm_type(trgt_type),	
						.conversion.value = llvm_untype_value(value)
					});
					value = LLVM_TYPED_REG(reg, trgt_type);

					// TODO: warn if cast truncates
					goto no_cast;
				}
				case AST_DATATYPE_STRUCTURED:
					goto error;
				case AST_DATATYPE_FLOAT:
				{
					llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
						.type = src_type->integral.signed_ ? LLVM_INST_SINT_TO_FLOAT : LLVM_INST_SINT_TO_FLOAT,
						.conversion.from = ast_type_to_llvm_type(src_type),
						.conversion.to = ast_type_to_llvm_type(trgt_type),	
						.conversion.value = llvm_untype_value(value)
					});
					value = LLVM_TYPED_REG(reg, trgt_type);
					goto no_cast;
				}
				case AST_DATATYPE_INTEGRAL:
				{
					bool src_signed = src_type->integral.signed_;
					bool trgt_signed = trgt_type->integral.signed_;
					unsigned int src_width = src_type->integral.bitwidth;
					unsigned int trgt_width = trgt_type->integral.bitwidth;

					if (src_width == trgt_width){
						if (src_signed == trgt_signed)
							goto no_cast;
						
						warning_string = "mismatched signedness";
						goto implicit_warning;
					}

					if (src_width > trgt_width){
						llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
							.type = LLVM_INST_TRUNC,
							.conversion.value = llvm_untype_value(value),
							.conversion.from = ast_type_to_llvm_type(src_type),
							.conversion.to = ast_type_to_llvm_type(trgt_type)	
						});
						value = LLVM_TYPED_REG(reg, trgt_type);
						
						warning_string = src_signed == trgt_signed ? "truncating may change value" : "truncating may change value, mismatched signedness";
						goto implicit_warning;
					}

					// src_width < trgt_width
					if (src_signed == false){
						llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
							.type = LLVM_INST_ZEXT,
							.conversion.value = llvm_untype_value(value),
							.conversion.from = ast_type_to_llvm_type(src_type),
							.conversion.to = ast_type_to_llvm_type(trgt_type)	
						});
						return LLVM_TYPED_REG(reg, trgt_type);
					}
					
					// src_signed == true
					llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
						.type = LLVM_INST_SEXT,
						.conversion.value = llvm_untype_value(value),
						.conversion.from = ast_type_to_llvm_type(src_type),
						.conversion.to = ast_type_to_llvm_type(trgt_type)	
					});
					value = LLVM_TYPED_REG(reg, trgt_type);

					if (src_signed == trgt_signed)
						return value;
					warning_string = "mismatched signedness";
					goto implicit_warning;
				}
			}
	}

error:
	if (err){
		*err = true;
	}else{
		printf_error(loc, "invalid cast from type '%s' to type '%s'", src_type->name, trgt_type->name);
	}
	return LLVM_TYPED_POISON(trgt_type);

implicit_warning:
	if (implicit){
		printf_warning(loc, "implicit cast from type '%s' to type '%s': %s", src_type->name, trgt_type->name, warning_string);
	}
no_cast:
	value.ast_type = trgt_type;
	return value;
}

static llvm_value_t llvm_cast_to_i1(llvm_typed_value_t val, llvm_function_t* f, const ast_t ast){
	switch (val.type){
		case LLVM_TVALUE_INT_CONST:
			bignum_set_uint(val.int_const, bignum_is_nonzero(val.int_const));
			return llvm_untype_value(val);
		case LLVM_TVALUE_POISON:
			return POISON_VALUE;
		case LLVM_TVALUE_DOUBLE_CONST:
		{	
			llvm_reg_t out = llvm_add_inst(f, 
			(llvm_inst_t){
				.type = LLVM_INST_FCMP,
				.fcmp.cond = LLVM_FCMP_UNE,
				.fcmp.type = ast_type_to_llvm_type(val.ast_type),
				.fcmp.op1 = llvm_untype_value(val),
				.fcmp.op2 = (llvm_value_t){ .type = LLVM_VALUE_DOUBLE_CONST, .double_const = (double)0.0 }
			});
			return (llvm_value_t){ .type = LLVM_VALUE_REG, .reg = out };
		}
		case LLVM_TVALUE_REG:
			switch (val.ast_type->kind){
				case AST_DATATYPE_VOID:
					puts("internal error"); exit(1);
				case AST_DATATYPE_POINTER:
				{	
					llvm_reg_t out = llvm_add_inst(f, 
					(llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = LLVM_ICMP_NE,
						.icmp.type = ast_type_to_llvm_type(val.ast_type),
						.icmp.op1 = llvm_untype_value(val),
						.icmp.op2 = (llvm_value_t){ .type = LLVM_VALUE_NULL_PTR }
					});
					return (llvm_value_t){ .type = LLVM_VALUE_REG, .reg = out };
				}
				case AST_DATATYPE_FLOAT:
				{
					bool err;
					llvm_reg_t out = llvm_add_inst(f,
					(llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.fcmp.cond = LLVM_FCMP_ONE,
						.fcmp.type = ast_type_to_llvm_type(val.ast_type),
						.fcmp.op1 = llvm_untype_value(val),
						.fcmp.op2 = llvm_untype_value(ast2llvm_cast(
								(llvm_typed_value_t){
									.type = LLVM_TVALUE_DOUBLE_CONST,
									.ast_type = find_ast_type(ast.global_scope, "f64"),
									.double_const = (double)0.0
								},
								val.ast_type, f, &err, (loc_t){0}))
					});
					if (err){ puts("internal error"); exit(1); }
					return (llvm_value_t){ .type = LLVM_VALUE_REG, .reg = out }; 
				}
				case AST_DATATYPE_STRUCTURED:
					return POISON_VALUE; // TODO: error
				case AST_DATATYPE_INTEGRAL:
				{	
					llvm_reg_t out = llvm_add_inst(f, 
					(llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = LLVM_ICMP_NE,
						.icmp.type = ast_type_to_llvm_type(val.ast_type),
						.icmp.op1 = llvm_untype_value(val),
						.icmp.op2 = (llvm_value_t){ .type = LLVM_VALUE_INT_CONST, .int_const = create_bignum() }
					});
					return (llvm_value_t){ .type = LLVM_VALUE_REG, .reg = out };
				}
			}
	}
	puts("internal error"); exit(1);
}

static llvm_typed_value_t ast2llvm_emit_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f, ast_t ast);
static llvm_typed_value_t ast2llvm_emit_short_circuiting_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f, const ast_t ast){
	// NOTE: expr must be AST_EXPR_BINOP of type AST_EXPR_BINOP_LAND or AST_EXPR_BINOP_LOR
	llvm_typed_value_t left_operand = ast2llvm_emit_expr(expr->binop.left, var2reg, f, ast);
	llvm_value_t left;
	if (left_operand.type == LLVM_TVALUE_REG && left_operand.ast_type->kind == AST_DATATYPE_STRUCTURED){
		printf_error(expr->binop.left->loc, "struct type '%s' is not supported for binary operation '%s'", left_operand.ast_type->name, ast_expr_binop_string(expr->binop.op));
		left = POISON_VALUE;
	}else{
		left = llvm_cast_to_i1(left_operand, f, ast);
	}
	
	llvm_label_t base_label = llvm_function_last_label(f);
	llvm_label_t long_start_label = llvm_add_block(f);
	llvm_typed_value_t right_operand = ast2llvm_emit_expr(expr->binop.right, var2reg, f, ast);
	llvm_value_t right;

	// catch structured types and floats
	if (right_operand.type == LLVM_TVALUE_REG && right_operand.ast_type->kind == AST_DATATYPE_STRUCTURED){
		printf_error(expr->binop.right->loc, "struct type '%s' is not supported for binary operation '%s'", right_operand.ast_type->name, ast_expr_binop_string(expr->binop.op));
		right = POISON_VALUE;
	}else{
		right = llvm_cast_to_i1(right_operand, f, ast);
	}
	
	llvm_label_t long_end_label = llvm_function_last_label(f);
	llvm_label_t next_label = llvm_add_block(f);
	
	llvm_get_block(f, long_end_label)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
	
	if (expr->binop.op == AST_EXPR_BINOP_LAND){
		llvm_get_block(f, base_label)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=left, .br.iftrue=long_start_label, .br.iffalse=next_label };
	}else{
		llvm_get_block(f, base_label)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=left, .br.iftrue=next_label, .br.iffalse=long_start_label };
	}

	llvm_reg_t out = llvm_add_inst(f, (llvm_inst_t){
		.type=LLVM_INST_PHI,
		.phi.type=LLVM_TYPE_I1,
		.phi.count=2,
		.phi.labels = {base_label, long_end_label},
		.phi.values = {left, right}
	});
	return LLVM_TYPED_REG(out, find_ast_type(ast.global_scope, "bool"));
}

static llvm_typed_value_t ast2llvm_int_const_binop(ast_expr_binop_enum_t op, llvm_typed_value_t left_operand, llvm_typed_value_t right_operand){
	// NOTE: both left_operand and right_operand must be LLVM_TVALUE_INT_CONST and have .ast_type == 0
	switch (op){
		case AST_EXPR_BINOP_ADD:
			bignum_add(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_SUB:
			bignum_sub(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_MUL:
			bignum_mul(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_DIV:
			bignum_div(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_MOD:
			bignum_mod(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_LT:
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) < 0); break;
		case AST_EXPR_BINOP_GT: 
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) > 0); break;
		case AST_EXPR_BINOP_LE:
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) <= 0); break;
		case AST_EXPR_BINOP_GE:
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) >= 0); break;
		case AST_EXPR_BINOP_EQ:
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) == 0); break;
		case AST_EXPR_BINOP_NE:
			bignum_set_uint(left_operand.int_const, bignum_cmp(left_operand.int_const, right_operand.int_const) != 0); break;
		case AST_EXPR_BINOP_SHL:
			printf("<shift left unimplemented>\n"); exit(1); // TODO
		case AST_EXPR_BINOP_SHR:
			printf("<shift right unimplemented>\n"); exit(1); // TODO
		case AST_EXPR_BINOP_BXOR:
			bignum_xor(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_BAND:
			bignum_and(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_BOR:
			bignum_or(left_operand.int_const, right_operand.int_const); break;
		case AST_EXPR_BINOP_LAND:
			bignum_set_uint(left_operand.int_const, (bignum_cmp_uint(left_operand.int_const, 0)==0) && (bignum_cmp_uint(left_operand.int_const, 0)==0)); break;
		case AST_EXPR_BINOP_LOR:
			bignum_set_uint(left_operand.int_const, (bignum_cmp_uint(left_operand.int_const, 0)==0) || (bignum_cmp_uint(left_operand.int_const, 0)==0)); break;
	}
	free_bignum(right_operand.int_const);
	return left_operand;
}


static llvm_typed_value_t ast2llvm_emit_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f, const ast_t ast){
	switch (expr->type){
		case AST_EXPR_INT_CONST:
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_INT_CONST, .int_const=bignum_copy(expr->int_constant), .ast_type=0 };
		case AST_EXPR_DOUBLE_CONST:
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_DOUBLE_CONST, .double_const=expr->double_constant, .ast_type=find_ast_type(ast.global_scope, "f64") };
		case AST_EXPR_LVALUE:
			return ast2llvm_read_lvalue(var2reg, expr->lvalue, f);
		case AST_EXPR_FUNC_CALL:
		{
			llvm_typed_value_list_t arglist = create_llvm_typed_value_list();
			for (unsigned int i=0; i < expr->func_call.arglist.len; i++){
				llvm_typed_value_t arg = ast2llvm_emit_expr(&expr->func_call.arglist.data[i], var2reg, f, ast);
				llvm_typed_value_list_append(&arglist, arg);
			}

			if (arglist.len > expr->func_call.func_ref->args.len){
				printf_error(expr->loc, "too many arguments for call to function '%s'", expr->func_call.func_ref->name);
				printf_info(expr->func_call.func_ref->declare_loc, "declared here");
				shallow_free_llvm_typed_value_list(&arglist);
				return LLVM_TYPED_POISON(expr->func_call.func_ref->return_type_ref);
			}
			if (arglist.len < expr->func_call.func_ref->args.len) {
				printf_error(expr->loc, "too few arguments for call to function '%s'", expr->func_call.func_ref->name);
				printf_info(expr->func_call.func_ref->declare_loc, "declared here");
				shallow_free_llvm_typed_value_list(&arglist);
				return LLVM_TYPED_POISON(expr->func_call.func_ref->return_type_ref);
			}
			for (unsigned int i=0; i < arglist.len; i++){
				const ast_datatype_t* expected_argtype = expr->func_call.func_ref->args.data[i]->type_ref;
				const ast_datatype_t* given_argtype = expr->func_call.func_ref->args.data[i]->type_ref;
				if (!given_argtype) continue;
				
				if (ast_datatype_eq(expected_argtype, given_argtype)) 
					continue;
				
				printf_error(expr->func_call.arglist.data[i].loc, "incompatible argument of type '%s' (expecting '%s')", given_argtype->name, expected_argtype->name);
				printf_info(expr->func_call.func_ref->declare_loc, "declared here");
				shallow_free_llvm_typed_value_list(&arglist);
				return LLVM_TYPED_POISON(expr->func_call.func_ref->return_type_ref);
			}

			const ast_datatype_t* rettype = expr->func_call.func_ref->return_type_ref;
			llvm_inst_t inst;
			if (rettype->kind == AST_DATATYPE_VOID){
				inst = (llvm_inst_t){
					.type=LLVM_INST_VOID_CALL,
					.call.name=strdup(expr->func_call.func_ref->name),
					.call.args=create_llvm_arg_list()
				};
			}else{
				inst = (llvm_inst_t){
					.type=LLVM_INST_CALL,
					.call.name=strdup(expr->func_call.func_ref->name),
					.call.rettype=ast_type_to_llvm_type(rettype),
					.call.args=create_llvm_arg_list()
				};
			}
			for (unsigned int i=0; i<arglist.len; i++){
				llvm_arg_list_append(&inst.call.args, (llvm_arg_t){
					.type=ast_type_to_llvm_type(expr->func_call.func_ref->args.data[i]->type_ref),
					.val=llvm_untype_value(arglist.data[i])
				});
			}
			llvm_reg_t ret = llvm_add_inst(f, inst);
			shallow_free_llvm_typed_value_list(&arglist);
			return rettype->kind == AST_DATATYPE_VOID ? LLVM_TYPED_POISON(rettype) : LLVM_TYPED_REG(ret, rettype);
		}
		case AST_EXPR_UNOP:
		{
			llvm_typed_value_t operand = ast2llvm_emit_expr(expr->unop.operand, var2reg, f, ast);
			if (operand.type == LLVM_TVALUE_POISON && operand.ast_type == 0) return operand;
			if (operand.type == LLVM_TVALUE_INT_CONST && operand.ast_type == 0){
				switch (expr->unop.op){
					case AST_EXPR_UNOP_DEREF:
						print_error(expr->loc, "cannot dereference integer constant (cast it to a pointer type first)");
						return LLVM_TYPED_POISON(0);
					case AST_EXPR_UNOP_BNOT:
						printf_error(expr->loc, "cannot use '%s' on integer constant (cast it to a sized integer first)", ast_expr_unop_string(expr->unop.op));
						return LLVM_TYPED_POISON(0);
					case AST_EXPR_UNOP_NEG:
						bignum_negate(operand.int_const);
						return operand;
					case AST_EXPR_UNOP_LNOT:
						bignum_set_uint(operand.int_const, !bignum_is_nonzero(operand.int_const));
						return operand;
				}
			}
			switch (operand.ast_type->kind){
				case AST_DATATYPE_VOID:
					printf_error(expr->unop.operand->loc, "void type '%s' is not supported for unary operation '%s'", operand.ast_type->name, ast_expr_unop_string(expr->unop.op));
					return LLVM_TYPED_POISON(0);
				case AST_DATATYPE_STRUCTURED:
					printf_error(expr->unop.operand->loc, "struct type '%s' is not supported for unary operation '%s'", operand.ast_type->name, ast_expr_unop_string(expr->unop.op));
					return LLVM_TYPED_POISON(0);
				case AST_DATATYPE_FLOAT:
					switch (expr->unop.op){
						case AST_EXPR_UNOP_DEREF:
							printf_error(expr->unop.operand->loc, "cannot dereference non-pointer value of type '%s'", operand.ast_type->name);
							return LLVM_TYPED_POISON(0);
						case AST_EXPR_UNOP_BNOT:
							printf_error(expr->unop.operand->loc, "floating point type '%s' is not supported for unary operation '%s'", operand.ast_type->name, ast_expr_unop_string(expr->unop.op));
							return LLVM_TYPED_POISON(0);
						case AST_EXPR_UNOP_NEG:
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_FNEG,
								.fneg.type = ast_type_to_llvm_type(operand.ast_type),
								.fneg.value = llvm_untype_value(operand)
							}), operand.ast_type);						
						case AST_EXPR_UNOP_LNOT:
						{
							bignum_t* num = create_bignum();
							bignum_set_uint(num, 0);
							llvm_value_t res = llvm_cast_to_i1(operand, f, ast);
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_ICMP,
								.icmp.cond = LLVM_ICMP_EQ,
								.icmp.type = LLVM_TYPE_I1,
								.icmp.op1 = res,
								.icmp.op2 = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=num }
							}), find_ast_type(ast.global_scope, "bool"));
						}
					}
					puts("internal error"); exit(1);
				case AST_DATATYPE_POINTER:
					switch (expr->unop.op){
						case AST_EXPR_UNOP_NEG:
						case AST_EXPR_UNOP_BNOT:
						case AST_EXPR_UNOP_LNOT:
						{
							bignum_t* num = create_bignum();
							bignum_set_uint(num, 0);
							llvm_value_t res = llvm_cast_to_i1(operand, f, ast);
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_ICMP,
								.icmp.cond = LLVM_ICMP_EQ,
								.icmp.type = LLVM_TYPE_I1,
								.icmp.op1 = res,
								.icmp.op2 = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=num }
							}), find_ast_type(ast.global_scope, "bool"));
						}
						case AST_EXPR_UNOP_DEREF:
						{
							ast_datatype_t* base_type = operand.ast_type->pointer.base;
							llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_LOAD,
								.load.type = ast_type_to_llvm_type(base_type),
								.load.ptr = llvm_untype_value(operand)
							});
							return LLVM_TYPED_REG(reg, base_type);
						}
					}
					puts("internal error"); exit(1);
				case AST_DATATYPE_INTEGRAL:
					switch (expr->unop.op){
						case AST_EXPR_UNOP_NEG:
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
									.type = LLVM_INST_SUB,
									.binop.type = ast_type_to_llvm_type(operand.ast_type),
									.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=create_bignum() },
									.binop.second = llvm_untype_value(operand)
								}), operand.ast_type);
						case AST_EXPR_UNOP_BNOT:
						{
							llvm_type_t optype;
							optype = ast_type_to_llvm_type(operand.ast_type);
							bignum_t* val = create_bignum();
							bignum_set_uint(val, 1);
							bignum_shift_left_uint(val, optype.bitwidth);
							bignum_sub_uint(val, 1);
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
									.type = LLVM_INST_XOR,
									.binop.type = optype,
									.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=val },
									.binop.second = llvm_untype_value(operand)
								}), operand.ast_type);
						}
						case AST_EXPR_UNOP_LNOT:
							return LLVM_TYPED_REG(llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_ICMP,
								.icmp.type = ast_type_to_llvm_type(operand.ast_type),
								.icmp.cond = LLVM_ICMP_EQ,
								.icmp.op1 = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=create_bignum() },
								.icmp.op2 = llvm_untype_value(operand)
							}), find_ast_type(ast.global_scope, "bool"));
						case AST_EXPR_UNOP_DEREF:
							printf_error(expr->loc, "cannot dereference non-pointer value of type '%s'", operand.ast_type->name);
							return LLVM_TYPED_POISON(0);
					}
					puts("internal error"); exit(1);
				}
				puts("internal error"); exit(1);
		}

		case AST_EXPR_BINOP:
		{
			// short-circuiting && and ||
			if (expr->binop.op == AST_EXPR_BINOP_LAND || expr->binop.op == AST_EXPR_BINOP_LOR){
				return ast2llvm_emit_short_circuiting_expr(expr, var2reg, f, ast);
			}
			llvm_typed_value_t left_operand = ast2llvm_emit_expr(expr->binop.left, var2reg, f, ast);
			llvm_typed_value_t right_operand = ast2llvm_emit_expr(expr->binop.right, var2reg, f, ast);
			const ast_datatype_t* const left_type_original = left_operand.ast_type;
			const ast_datatype_t* const right_type_original = right_operand.ast_type;

			// catch poison
			if ((left_operand.type == LLVM_TVALUE_POISON && left_operand.ast_type == 0)
			|| (right_operand.type == LLVM_TVALUE_POISON && right_operand.ast_type == 0)){
				return LLVM_TYPED_POISON(0);
			}

			// catch untyped int constants
			if (left_operand.type == LLVM_TVALUE_INT_CONST && left_operand.ast_type == 0 && right_operand.type == LLVM_TVALUE_INT_CONST && right_operand.ast_type == 0){
				return ast2llvm_int_const_binop(expr->binop.op, left_operand, right_operand);
			}

			// type checks and casts
			bool err;
			if (left_operand.type == LLVM_TVALUE_INT_CONST && left_operand.ast_type == 0){
				switch (right_operand.ast_type->kind){
					case AST_DATATYPE_VOID: goto binop_invalid_types;
					case AST_DATATYPE_STRUCTURED: goto binop_invalid_types;
					case AST_DATATYPE_POINTER: goto binop_invalid_types;
					case AST_DATATYPE_FLOAT:
					case AST_DATATYPE_INTEGRAL:
						left_operand = ast2llvm_cast(left_operand, right_operand.ast_type, f, &err, expr->binop.left->loc);
						break;
				}
			}else switch (left_operand.ast_type->kind){
				case AST_DATATYPE_VOID: goto binop_invalid_types;
				case AST_DATATYPE_STRUCTURED: goto binop_invalid_types; 
				case AST_DATATYPE_POINTER: goto binop_invalid_types;
				case AST_DATATYPE_FLOAT:
					if (right_operand.type == LLVM_TVALUE_INT_CONST && right_operand.ast_type == 0){
						right_operand = ast2llvm_cast(right_operand, left_operand.ast_type, f, &err, expr->binop.right->loc);
					} else switch (right_operand.ast_type->kind){
						case AST_DATATYPE_VOID: goto binop_invalid_types;
						case AST_DATATYPE_STRUCTURED: goto binop_invalid_types;
						case AST_DATATYPE_POINTER: goto binop_invalid_types;
						case AST_DATATYPE_FLOAT:
							if (left_operand.ast_type->floating.bitwidth > right_operand.ast_type->floating.bitwidth){
								right_operand = ast2llvm_cast(right_operand, left_operand.ast_type, f, &err, expr->binop.right->loc);
							}else{
								left_operand = ast2llvm_cast(left_operand, right_operand.ast_type, f, &err, expr->binop.left->loc);
							}
							break;
						case AST_DATATYPE_INTEGRAL:
							right_operand = ast2llvm_cast(right_operand, left_operand.ast_type, f, &err, expr->binop.right->loc);
							break;
					}
					break;
				case AST_DATATYPE_INTEGRAL:
					if (right_operand.type == LLVM_TVALUE_INT_CONST && right_operand.ast_type == 0){
						right_operand = ast2llvm_cast(right_operand, left_operand.ast_type, f, &err, expr->binop.right->loc);
					} else switch (right_operand.ast_type->kind){
						case AST_DATATYPE_VOID: goto binop_invalid_types;
						case AST_DATATYPE_STRUCTURED: goto binop_invalid_types;
						case AST_DATATYPE_POINTER: goto binop_invalid_types;
						case AST_DATATYPE_FLOAT:
							left_operand = ast2llvm_cast(left_operand, right_operand.ast_type, f, &err, expr->binop.left->loc);
							break;
						case AST_DATATYPE_INTEGRAL:
						{
							// both left and right are integral
							if (left_operand.ast_type->integral.bitwidth > right_operand.ast_type->integral.bitwidth){
								right_operand = ast2llvm_cast(right_operand, left_operand.ast_type, f, &err, expr->binop.right->loc);
							}else{
								left_operand = ast2llvm_cast(left_operand, right_operand.ast_type, f, &err, expr->binop.left->loc);
							}
							break;
						}
					}
					break;
			}
			if (err) {
				print_error(expr->loc, "internal error");
				return LLVM_TYPED_POISON(0);
			}

			const ast_datatype_t* operand_ast_type = left_operand.ast_type;
			llvm_type_t optype = ast_type_to_llvm_type(operand_ast_type);
			llvm_inst_t inst;
			const ast_datatype_t* restype = operand_ast_type;
			
			switch (operand_ast_type->kind){
				case AST_DATATYPE_VOID:
				case AST_DATATYPE_STRUCTURED:
				case AST_DATATYPE_POINTER:
					print_error(expr->loc, "internal error");
					return LLVM_TYPED_POISON(0);
				case AST_DATATYPE_INTEGRAL:
				{
					bool signed_op = operand_ast_type->integral.signed_;
					switch (expr->binop.op){
						case AST_EXPR_BINOP_ADD: inst.type = LLVM_INST_ADD; goto int_binop;
						case AST_EXPR_BINOP_SUB: inst.type = LLVM_INST_SUB; goto int_binop;
						case AST_EXPR_BINOP_MUL: inst.type = LLVM_INST_MUL; goto int_binop;
						case AST_EXPR_BINOP_BXOR: inst.type = LLVM_INST_XOR; goto int_binop;
						case AST_EXPR_BINOP_BAND: inst.type = LLVM_INST_AND; goto int_binop;
						case AST_EXPR_BINOP_BOR: inst.type = LLVM_INST_OR; goto int_binop;
						case AST_EXPR_BINOP_SHL: inst.type = LLVM_INST_SHL; goto int_binop;
						case AST_EXPR_BINOP_SHR: inst.type = signed_op ? LLVM_INST_ASHR : LLVM_INST_LSHR; goto int_binop;
						case AST_EXPR_BINOP_DIV: inst.type = signed_op ? LLVM_INST_SDIV : LLVM_INST_UDIV; goto int_binop;
						case AST_EXPR_BINOP_MOD: inst.type = signed_op ? LLVM_INST_SREM : LLVM_INST_UREM; goto int_binop;
						int_binop:
							inst.binop.type = optype;
							inst.binop.first = llvm_untype_value(left_operand);
							inst.binop.second = llvm_untype_value(right_operand);
							break;
							
						case AST_EXPR_BINOP_LT: inst.icmp.cond = signed_op ? LLVM_ICMP_SLT : LLVM_ICMP_ULT; goto icmp;
						case AST_EXPR_BINOP_GT: inst.icmp.cond = signed_op ? LLVM_ICMP_SGT : LLVM_ICMP_UGT; goto icmp;
						case AST_EXPR_BINOP_LE: inst.icmp.cond = signed_op ? LLVM_ICMP_SLE : LLVM_ICMP_ULE; goto icmp;
						case AST_EXPR_BINOP_GE: inst.icmp.cond = signed_op ? LLVM_ICMP_SGE : LLVM_ICMP_UGE; goto icmp;
						case AST_EXPR_BINOP_EQ: inst.icmp.cond = LLVM_ICMP_EQ; goto icmp;
						case AST_EXPR_BINOP_NE: inst.icmp.cond = LLVM_ICMP_NE; goto icmp;
						icmp:
							inst.type = LLVM_INST_ICMP;
							inst.icmp.type = optype;
							inst.icmp.op1 = llvm_untype_value(left_operand);
							inst.icmp.op2 = llvm_untype_value(right_operand);
							restype = find_ast_type(ast.global_scope, "bool");
							break;
						
						case AST_EXPR_BINOP_LAND:
						case AST_EXPR_BINOP_LOR:
							printf("internal error\n"); exit(1);
					}
					llvm_reg_t out = llvm_add_inst(f, inst);
					if (inst.type == LLVM_INST_ICMP){ // icmp instruction always returns i1 instead of optype
						out = llvm_add_inst(f,
							(llvm_inst_t){
								.type = LLVM_INST_ZEXT,
								.conversion.from = LLVM_TYPE_I1,
								.conversion.to = optype,
								.conversion.value = (llvm_value_t){.type=LLVM_VALUE_REG, .reg=out},
							});
					}
					return LLVM_TYPED_REG(out, operand_ast_type);
				}
				case AST_DATATYPE_FLOAT:
				{
					switch (expr->binop.op){
						case AST_EXPR_BINOP_ADD: inst.type = LLVM_INST_FADD; goto fp_binop;
						case AST_EXPR_BINOP_SUB: inst.type = LLVM_INST_FSUB; goto fp_binop;
						case AST_EXPR_BINOP_MUL: inst.type = LLVM_INST_FMUL; goto fp_binop;
						case AST_EXPR_BINOP_DIV: inst.type = LLVM_INST_FDIV; goto fp_binop;
						case AST_EXPR_BINOP_MOD: inst.type = LLVM_INST_FREM; goto fp_binop;
						fp_binop:
							inst.binop.type = optype;
							inst.binop.first = llvm_untype_value(left_operand);
							inst.binop.second = llvm_untype_value(right_operand);
							break;
						
						case AST_EXPR_BINOP_LT: inst.fcmp.cond = LLVM_FCMP_OLT; goto fcmp;
						case AST_EXPR_BINOP_GT: inst.fcmp.cond = LLVM_FCMP_OGT; goto fcmp;
						case AST_EXPR_BINOP_LE: inst.fcmp.cond = LLVM_FCMP_OLE; goto fcmp;
						case AST_EXPR_BINOP_GE: inst.fcmp.cond = LLVM_FCMP_OGE; goto fcmp;
						case AST_EXPR_BINOP_EQ: inst.fcmp.cond = LLVM_FCMP_OEQ; goto fcmp;
						case AST_EXPR_BINOP_NE: inst.fcmp.cond = LLVM_FCMP_ONE; goto fcmp;
						fcmp:
							inst.type = LLVM_INST_FCMP;
							inst.fcmp.type = optype;
							inst.fcmp.op1 = llvm_untype_value(left_operand);
							inst.fcmp.op2 = llvm_untype_value(right_operand);
							restype = find_ast_type(ast.global_scope, "bool");
							break;
							
						case AST_EXPR_BINOP_SHL: goto binop_invalid_types;
						case AST_EXPR_BINOP_SHR: goto binop_invalid_types;
						case AST_EXPR_BINOP_BXOR: goto binop_invalid_types;
						case AST_EXPR_BINOP_BAND: goto binop_invalid_types;
						case AST_EXPR_BINOP_BOR: goto binop_invalid_types;
						case AST_EXPR_BINOP_LAND:
						case AST_EXPR_BINOP_LOR:
							printf("internal error\n"); exit(1);
					}
				}
			}
			llvm_reg_t out = llvm_add_inst(f, inst);
			return LLVM_TYPED_REG(out, restype);

			binop_invalid_types:
			{
				const char* kinds[2];
				const char* names[2];
				for (unsigned int i=0; i<2; i++){
					const ast_datatype_t* type = (const ast_datatype_t*[2]){left_type_original, right_type_original}[i];

					if (type == 0){
						kinds[i] = "";
						names[i] = "integer constant";
					} else {
						names[i] = type->name;
						switch (type->kind){
							case AST_DATATYPE_VOID: kinds[i] = "void type "; break;
							case AST_DATATYPE_FLOAT: kinds[i] = "floating point "; break;
							case AST_DATATYPE_INTEGRAL: kinds[i] = "integer "; break;
							case AST_DATATYPE_POINTER: kinds[i] = "pointer "; break;
							case AST_DATATYPE_STRUCTURED: kinds[i] = "struct "; break;
						}
					}
				}
				printf_error(expr->loc, "invalid types %s'%s' and %s'%s' for binary operation '%s'",
					kinds[0], names[0],
					kinds[1], names[1],
					ast_expr_binop_string(expr->binop.op)
				);
				return LLVM_TYPED_POISON(0);
			}
		}
		case AST_EXPR_REF:
			return ast2llvm_get_lvalue_pointer(var2reg, expr->ref.lvalue, f);
		case AST_EXPR_CAST:
		{
			llvm_typed_value_t val = ast2llvm_emit_expr(expr->cast.expr, var2reg, f, ast);
			return ast2llvm_cast(val, expr->cast.type_ref, f, 0, expr->loc);
		}
	}
	puts("INTERNAL ERROR");
	exit(1);
}

static void ast2llvm_emit_stmt(ast_stmt_t* stmt, const var2reg_map_t* var2reg, llvm_function_t* f, const ast_func_t ast_func, const ast_t ast){
	switch (stmt->type){
		case AST_STMT_IF:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_.cond, var2reg, f, ast), f, ast);
			llvm_label_t base_label = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iftrue_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_.iftrue, var2reg, f, ast_func, ast);
			llvm_label_t iftrue_end = llvm_function_last_label(f);			

			llvm_label_t next_label = llvm_add_block(f);
			f->blocks.data[iftrue_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[base_label.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_start, .br.iffalse=next_label };

			return;
		}
		case AST_STMT_IF_ELSE:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_else.cond, var2reg, f, ast), f, ast);
			llvm_label_t base_label = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iftrue_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_else.iftrue, var2reg, f, ast_func, ast);
			llvm_label_t iftrue_end = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iffalse_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_else.iffalse, var2reg, f, ast_func, ast);
			llvm_label_t iffalse_end = llvm_function_last_label(f);

			llvm_label_t next_label = llvm_add_block(f);
			f->blocks.data[iftrue_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[iffalse_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[base_label.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_start, .br.iffalse=iffalse_start };

			return;
		}
		case AST_STMT_EXPR:
			ast2llvm_emit_expr(&stmt->expr, var2reg, f, ast);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				ast2llvm_emit_stmt(&stmt->block.stmtlist.data[i], var2reg, f, ast_func, ast);
			}
			return;
		case AST_STMT_ASSIGN:
			{
				llvm_typed_value_t val = ast2llvm_emit_expr(&stmt->assign.val, var2reg, f, ast);
				const ast_datatype_t* trgt_type = stmt->assign.lvalue.type;

				if (val.type == LLVM_TVALUE_POISON && val.ast_type == 0)
					return;
				if (val.type == LLVM_TVALUE_INT_CONST && val.ast_type == 0){
					bool err;
					val = ast2llvm_cast(val, trgt_type, f, &err, stmt->loc);
					if (err){
						printf_error(stmt->loc, "attempt to assign integer constant to %s '%s' of type '%s'",
							stmt->assign.lvalue.member_access.len ? "member" : "variable",
							stmt->assign.lvalue.member_access.len ?
								stmt->assign.lvalue.type->structure.members.data[stmt->assign.lvalue.member_access.data[stmt->assign.lvalue.member_access.len-1].member_idx].type_ref->name
								: trgt_type->name,
							trgt_type->name
						);
					}
				}
					
				const ast_datatype_t* src_type = val.ast_type;

				if (!ast_datatype_eq(trgt_type, src_type)){
					bool err;
					val = ast2llvm_cast(val, trgt_type, f, &err, stmt->loc);
					if (err){
						printf_error(stmt->loc, "attempt to assign type '%s' to %s '%s' of type '%s'",
							src_type->name,
							stmt->assign.lvalue.member_access.len ? "member" : "variable",
							stmt->assign.lvalue.member_access.len ?
								stmt->assign.lvalue.type->structure.members.data[stmt->assign.lvalue.member_access.data[stmt->assign.lvalue.member_access.len-1].member_idx].type_ref->name
								: stmt->assign.lvalue.base_var->name,
							trgt_type->name
						);
					}
				}
				
				llvm_typed_value_t ptr = ast2llvm_get_lvalue_pointer(var2reg, stmt->assign.lvalue, f);
				if (ptr.type == LLVM_TVALUE_POISON)
					return;
				llvm_add_inst(f, (llvm_inst_t){
					.type = LLVM_INST_STORE,
					.store.type = ast_type_to_llvm_type(trgt_type),
					.store.value = llvm_untype_value(val),
					.store.ptr = llvm_untype_value(ptr)
				});
				return;
			}
		case AST_STMT_BREAK:	printf("<unimplemented (break)>\n"); exit(1); // TODO
		case AST_STMT_CONTINUE:	printf("<unimplemented (continue)>\n"); exit(1); // TODO
		case AST_STMT_RETURN:
		{
			if (!f->has_return){
				if (stmt->return_.val){
					printf_error(stmt->loc, "return value in function returning 'void'");
				}
				
				llvm_function_last_block(f)->term_inst = (llvm_term_inst_t){ .type = LLVM_TERM_INST_RET_VOID };
				llvm_add_block(f);
				return;
			}
			
			llvm_typed_value_t val = ast2llvm_emit_expr(stmt->return_.val, var2reg, f, ast);
			if (val.type == LLVM_TVALUE_POISON && val.ast_type == 0) return;
			if (val.type == LLVM_TVALUE_INT_CONST && val.ast_type == 0){
				bool err;
				val = ast2llvm_cast(val, ast_func.return_type_ref, f, &err, stmt->loc);
				if (err){
					printf_error(stmt->loc, "invalid return of integer constant (expected type '%s')", ast_func.return_type_ref->name);
					val = LLVM_TYPED_POISON(ast_func.return_type_ref);
				}
			}else{
				bool err;
				val = ast2llvm_cast(val, ast_func.return_type_ref, f, &err, stmt->loc);
				if (err){
					printf_error(stmt->loc, "invalid return type '%s' (expected '%s')", val.ast_type->name, ast_func.return_type_ref->name);
					val = LLVM_TYPED_POISON(ast_func.return_type_ref);
				}
			}
			
			llvm_function_last_block(f)->term_inst = (llvm_term_inst_t){
				.type = LLVM_TERM_INST_RET,
				.ret.type = f->rettype,
				.ret.value = llvm_untype_value(val)
			};
			llvm_add_block(f); // add a dead block to consume any extra instructions
			return;
		}
		case AST_STMT_FOR:
		{
			ast2llvm_emit_stmt(stmt->for_.init, var2reg, f, ast_func, ast);
			llvm_label_t base_label = llvm_function_last_label(f);
			
			llvm_label_t cond_start = llvm_add_block(f);
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->for_.cond, var2reg, f, ast), f, ast);
			llvm_label_t cond_end = llvm_function_last_label(f);

			llvm_label_t body_start = llvm_add_block(f);
			ast2llvm_emit_stmt(stmt->for_.body, var2reg, f, ast_func, ast);
			ast2llvm_emit_stmt(stmt->for_.step, var2reg, f, ast_func, ast);
			llvm_label_t body_end = llvm_function_last_label(f);

			llvm_label_t next = llvm_add_block(f);
			llvm_get_block(f, base_label)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			llvm_get_block(f, cond_end)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=body_start, .br.iffalse=next };
			llvm_get_block(f, body_end)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			return;
		}
		case AST_STMT_WHILE:
		{
			llvm_label_t base_label = llvm_function_last_label(f);
			
			llvm_label_t cond_start = llvm_add_block(f);
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->while_.cond, var2reg, f, ast), f, ast);
			llvm_label_t cond_end = llvm_function_last_label(f);
			
			llvm_label_t body_start = llvm_add_block(f);
			ast2llvm_emit_stmt(stmt->while_.body, var2reg, f, ast_func, ast);
			llvm_label_t body_end = llvm_function_last_label(f);

			llvm_label_t next = llvm_add_block(f);
			llvm_get_block(f, base_label)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			llvm_get_block(f, cond_end)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=body_start, .br.iffalse=next };
			llvm_get_block(f, body_end)->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			return;
		}
	}
}

static llvm_function_t ast2llvm_emit_func(ast_func_t func, ast_t ast){
	llvm_function_t f;
	f.name = strdup(func.name);
	f.blocks = create_llvm_basic_block_list();

	if (func.return_type_ref->kind == AST_DATATYPE_VOID){
		f.has_return = false;
	}else{
		f.has_return = true;
		f.rettype = ast_type_to_llvm_type(func.return_type_ref);
	}

	f.arg_count = func.args.len;
	f.args = malloc(f.arg_count*sizeof(llvm_type_t));
	for (unsigned int i=0; i < func.args.len; i++){
		f.args[i] = ast_type_to_llvm_type(func.args.data[i]->type_ref);
	}

	llvm_basic_block_list_append(&f.blocks, (llvm_basic_block_t){
		.regbase = func.args.len,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});

	var2reg_map_t var2reg = create_var2reg_map();
	
	ast2llvm_alloca_scope(func.local_scope, &var2reg, &f);
	for (unsigned int i=0; i < func.args.len; i++){
		ast_variable_t* var = func.args.data[i];
		llvm_add_inst(&f, (llvm_inst_t){
			.type = LLVM_INST_STORE,
			.store.type = ast_type_to_llvm_type(var->type_ref),
			.store.value = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=(llvm_reg_t){ .idx=i } },
			.store.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=var2reg_map_get(&var2reg, var, LLVM_INVALID_REG) }
		});
	}
	
	ast2llvm_alloca_stmt(func.body, &var2reg, &f);
	ast2llvm_emit_stmt(func.body, &var2reg, &f, func, ast);
	var2reg_map_free(&var2reg);

	if (f.has_return){
		f.blocks.data[f.blocks.len-1].term_inst = (llvm_term_inst_t){
			.type = LLVM_TERM_INST_RET,
			.ret.type = ast_type_to_llvm_type(func.return_type_ref),
			.ret.value = (llvm_value_t){ .type=LLVM_VALUE_UNDEF }
		};
	}else{
		f.blocks.data[f.blocks.len-1].term_inst = (llvm_term_inst_t){ .type = LLVM_TERM_INST_RET_VOID };
	}
	return f;
}

static llvm_global_def_t ast2llvm_emit_global(ast_global_t global){
	return (llvm_global_def_t){
		.name = strdup(global.var.name),
		.type = ast_type_to_llvm_type(global.var.type_ref),
		.init_val = POISON_VALUE // TODO
	};
}

llvm_program_t ast2llvm(ast_t ast){
	llvm_program_t llvm;
	llvm.source_path = strdup(ast.source_path);
	llvm.function_count = 0;
	llvm.global_count = 0;

	for (scope_iterator_t iter = scope_iter(ast.global_scope); iter.current; iter = scope_iter_next(iter))
		switch (iter.current->value->type){
			case AST_ID_FUNC: llvm.function_count++; break;
			case AST_ID_GLOBAL: llvm.global_count++; break;
			case AST_ID_VAR: break;
			case AST_ID_TYPE: break;
		}

	llvm.functions = malloc(sizeof(llvm_function_t)*llvm.function_count);
	llvm.globals = malloc(sizeof(llvm_global_def_t)*llvm.global_count);

	unsigned int functions_emitted = 0;
	unsigned int globals_emitted = 0;
	for (scope_iterator_t iter = scope_iter(ast.global_scope); iter.current; iter = scope_iter_next(iter))
		switch (iter.current->value->type){
			case AST_ID_FUNC:
				llvm.functions[functions_emitted] = ast2llvm_emit_func(iter.current->value->func, ast);
				functions_emitted++;
				break;
			case AST_ID_GLOBAL:
				llvm.globals[globals_emitted] = ast2llvm_emit_global(iter.current->value->global);
				globals_emitted++;
				break;
			case AST_ID_VAR: break;
			case AST_ID_TYPE: break;
		}

	return llvm;
}
