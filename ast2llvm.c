#include <string.h>
#include <stdarg.h>
#include "ast2llvm.h"
#include "set.h"

typedef enum {
	LLVM_TVALUE_INT_CONST = LLVM_VALUE_INT_CONST,
	LLVM_TVALUE_REG = LLVM_VALUE_REG,
	LLVM_TVALUE_POISON = LLVM_VALUE_POISON,
} llvm_typed_value_enum_t;

typedef struct llvm_typed_value_t {
	llvm_typed_value_enum_t type;
	union {
		unsigned long int_const;
		struct {
			llvm_reg_t reg;
			ast_datatype_t* ast_type;
		} typed_reg;
	};
} llvm_typed_value_t;

typedef const ast_variable_t* ast_variable_ptr;
create_hashmap_type_header(ast_variable_ptr, llvm_reg_t, var2reg_map);

create_list_type_header(llvm_typed_value, false);
create_list_type_impl(llvm_typed_value, false)

#define POISON_TYPED_VALUE ((llvm_typed_value_t){ .type=LLVM_TVALUE_POISON })
#define POISON_VALUE ((llvm_value_t){ .type=LLVM_VALUE_POISON })
#define LLVM_TYPE_I1 ((llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=1 })
#define LLVM_INT_CONST(x) ((llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=x })

static inline llvm_label_t llvm_function_last_label(const llvm_function_t* f){
	return (llvm_label_t){ .idx = f->blocks.len - 1 };
}
static inline llvm_basic_block_t* llvm_function_last_block(llvm_function_t* f){
	return &f->blocks.data[f->blocks.len - 1];
}
static inline llvm_reg_t llvm_block_last_reg(const llvm_basic_block_t* b){
	return (llvm_reg_t){ .idx = b->regbase + b->instructions.len - 1};
}
static inline llvm_value_t llvm_untype_value(const llvm_typed_value_t tv){
	llvm_value_t v;
	switch (tv.type){
			case LLVM_TVALUE_INT_CONST:
				v.type = LLVM_VALUE_INT_CONST;
				v.int_const = tv.int_const;
				break;
			case LLVM_TVALUE_REG:
				v.type = LLVM_VALUE_REG;
				v.reg = tv.typed_reg.reg;
				break;
			case LLVM_TVALUE_POISON:
				v.type = LLVM_VALUE_POISON;
				break;
	}
	return v;
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

static llvm_type_t ast_type_to_llvm_type(ast_datatype_t* t){
	switch (t->kind){
		case AST_DATATYPE_FLOAT:
			return (llvm_type_t){ .type=LLVM_TYPE_FLOAT };
		case AST_DATATYPE_INTEGRAL:
			return (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=t->integral.bitwidth };
		case AST_DATATYPE_STRUCTURED:
			llvm_type_t type = (llvm_type_t){
				.type=LLVM_TYPE_STRUCT,
				.structure.member_count = t->structure.members.len,
				.structure.member_types = malloc(sizeof(llvm_type_t)*t->structure.members.len)
			};
			for (unsigned int i = 0; i < type.structure.member_count; i++){
				type.structure.member_types[i] = ast_type_to_llvm_type(t->structure.members.data[i].type_ref);
			}
			return type;
		case AST_DATATYPE_POINTER:
			return (llvm_type_t){ .type=LLVM_TYPE_POINTER };
	}
}

static inline llvm_typed_value_t llvm_var2reg_get_typed_value(const var2reg_map_t* var2reg, const ast_variable_t* var, loc_t loc, llvm_function_t* f){
	llvm_reg_t ptr = var2reg_map_get(var2reg, var, LLVM_INVALID_REG);
	if (LLVM_REG_EQ(ptr, LLVM_INVALID_REG)){
		printf_error(loc, "use of undefined variable '%s'", var->name); // TODO: this can only happens for global vars after the alloca change 
		return POISON_TYPED_VALUE;
	}
	llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
		.type = LLVM_INST_LOAD,
		.load.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=ptr },
		.load.type = ast_type_to_llvm_type(var->type_ref)
	});
	return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=reg, .typed_reg.ast_type=var->type_ref };
}

static llvm_typed_value_t ast2llvm_read_lvalue(const var2reg_map_t* var2reg, const ast_lvalue_t lvalue, loc_t loc, llvm_function_t* f){
	llvm_reg_t reg = var2reg_map_get(var2reg, lvalue.base_var, LLVM_INVALID_REG);
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG)){
		printf_error(loc, "use of global variable '%s' is unimplemented", lvalue.base_var->name); // TODO: implement it
		return POISON_TYPED_VALUE;
	}
	ast_datatype_t* type = lvalue.base_var->type_ref;
	reg = llvm_add_inst(f, (llvm_inst_t){
		.type = LLVM_INST_LOAD,
		.load.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg },
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

	return (llvm_typed_value_t){ .type = LLVM_TVALUE_REG, .typed_reg.reg=reg, .typed_reg.ast_type=type };
}

static llvm_typed_value_t ast2llvm_get_lvalue_pointer(const var2reg_map_t* var2reg, const ast_lvalue_t lvalue, loc_t loc, llvm_function_t* f){
	llvm_reg_t reg = var2reg_map_get(var2reg, lvalue.base_var, LLVM_INVALID_REG);
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG)){
		printf_error(loc, "use of global variable '%s' is unimplemented", lvalue.base_var->name); // TODO: implement it
		return POISON_TYPED_VALUE;
	}
	ast_datatype_t* type = lvalue.base_var->type_ref; // the type that 'reg' is pointing to

	for (unsigned int i=0; i < lvalue.member_access.len; i++){
		ast_lvalue_member_access_t member_access = lvalue.member_access.data[i];
		if (member_access.deref){
			reg = llvm_add_inst(f, (llvm_inst_t){
				.type = LLVM_INST_LOAD,
				.load.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg },
				.load.type = ast_type_to_llvm_type(type)
			});
			type = type->pointer.base;
		}
		reg = llvm_add_inst(f, (llvm_inst_t){
			.type = LLVM_INST_GET_ELEMENT_PTR,
			.getelementptr.aggregate_type = ast_type_to_llvm_type(type),
			.getelementptr.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=reg },
			.getelementptr.member_idx = member_access.member_idx
		});
		type = type->structure.members.data[member_access.member_idx].type_ref;
	}

	return (llvm_typed_value_t){ .type = LLVM_TVALUE_REG, .typed_reg.reg=reg, .typed_reg.ast_type=get_ast_pointer_type(type) };
}

static void ast2llvm_alloca_context(const context_t* ctx, var2reg_map_t* var2reg, llvm_function_t* f){
	for (context_iterator_t iter = context_iter(ctx); iter.current; iter = context_iter_next(iter)){
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
			if (stmt->block.context)
				ast2llvm_alloca_context(stmt->block.context, var2reg, f);
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++)
				ast2llvm_alloca_stmt(&stmt->block.stmtlist.data[i], var2reg, f);
			break;
		case AST_STMT_FOR:
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

static llvm_typed_value_t llvm_int_const_binop(const ast_expr_t* expr, llvm_typed_value_t left_operand, llvm_typed_value_t right_operand){
	// NOTE: both left_operand and right_operand must be LLVM_TVALUE_INT_CONST
	// NOTE: expr must be AST_EXPR_BINOP
	switch (expr->binop.op){
		case AST_EXPR_BINOP_ADD:
			left_operand.int_const = left_operand.int_const + right_operand.int_const; break;
		case AST_EXPR_BINOP_SUB:
			left_operand.int_const = left_operand.int_const - right_operand.int_const; break;
		case AST_EXPR_BINOP_MUL:
			left_operand.int_const = left_operand.int_const * right_operand.int_const; break;
		case AST_EXPR_BINOP_DIV: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const / right_operand.int_const; break;
		case AST_EXPR_BINOP_MOD: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const % right_operand.int_const; break;
		case AST_EXPR_BINOP_LT: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const < right_operand.int_const; break;
		case AST_EXPR_BINOP_GT: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const > right_operand.int_const; break;
		case AST_EXPR_BINOP_LE: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const <= right_operand.int_const; break;
		case AST_EXPR_BINOP_GE: // TODO: signedness ?
			left_operand.int_const = left_operand.int_const >= right_operand.int_const; break;
		case AST_EXPR_BINOP_EQ:
			left_operand.int_const = left_operand.int_const == right_operand.int_const; break;
		case AST_EXPR_BINOP_NE:
			left_operand.int_const = left_operand.int_const != right_operand.int_const; break;
		case AST_EXPR_BINOP_SHL:
			left_operand.int_const = left_operand.int_const << right_operand.int_const; break;
		case AST_EXPR_BINOP_SHR:
			left_operand.int_const = left_operand.int_const >> right_operand.int_const; break;
		case AST_EXPR_BINOP_BXOR:
			left_operand.int_const = left_operand.int_const ^ right_operand.int_const; break;
		case AST_EXPR_BINOP_BAND:
			left_operand.int_const = left_operand.int_const & right_operand.int_const; break;
		case AST_EXPR_BINOP_BOR:
			left_operand.int_const = left_operand.int_const | right_operand.int_const; break;
		case AST_EXPR_BINOP_LAND:
			left_operand.int_const = left_operand.int_const || right_operand.int_const; break;
		case AST_EXPR_BINOP_LOR:
			left_operand.int_const = left_operand.int_const && right_operand.int_const; break;
	}
	return left_operand;
}

static unsigned long ulong_truncate(unsigned long x, unsigned int bitwidth){
	if (bitwidth == 64) return x;
	unsigned long mask = (1l << bitwidth) - 1;
	return x & mask;
}

static llvm_value_t llvm_cast_to_i1(llvm_typed_value_t val, llvm_function_t* f){
	llvm_reg_t out = llvm_add_inst(f, 
		(llvm_inst_t){
			.type = LLVM_INST_ICMP,
			.icmp.cond = LLVM_ICMP_NE,
			.icmp.type = ast_type_to_llvm_type(val.typed_reg.ast_type),
			.icmp.op1 = llvm_untype_value(val),
			.icmp.op2 = (llvm_value_t){ .type = LLVM_VALUE_INT_CONST, .int_const = 0}
		});
	return (llvm_value_t){ .type = LLVM_VALUE_REG, .reg = out };
}

static llvm_typed_value_t ast2llvm_emit_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f);
static llvm_typed_value_t ast2llvm_emit_short_circuiting_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f){
	// NOTE: expr must be AST_EXPR_BINOP of type AST_EXPR_BINOP_LAND or AST_EXPR_BINOP_LOR
	llvm_typed_value_t left_operand = ast2llvm_emit_expr(expr->binop.left, var2reg, f);

	// catch poison
	if (left_operand.type == LLVM_TVALUE_POISON){
		ast2llvm_emit_expr(expr->binop.right, var2reg, f);
		return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
	}
	// catch structured types
	if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
		printf_error(expr->binop.left->loc, "struct type '%s' is not supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		ast2llvm_emit_expr(expr->binop.right, var2reg, f);
		return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
	}
	// catch floating point types
	if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT){
		printf_error(expr->binop.left->loc, "floating point type '%s' is not (yet) supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		ast2llvm_emit_expr(expr->binop.right, var2reg, f);
		return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
	}
	//catch pointer types
	if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_POINTER){
		printf_error(expr->binop.left->loc, "pointer type '%s' is not supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		ast2llvm_emit_expr(expr->binop.right, var2reg, f);
		return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
	}

	
	llvm_value_t left = llvm_cast_to_i1(left_operand, f);
	llvm_label_t base_label = llvm_function_last_label(f);
	llvm_basic_block_t* base_block = &f->blocks.data[base_label.idx];

	llvm_add_block(f);
	llvm_typed_value_t right_operand = ast2llvm_emit_expr(expr->binop.right, var2reg, f);
	llvm_label_t long_label = llvm_function_last_label(f);
	llvm_basic_block_t* long_block = &f->blocks.data[long_label.idx];
	llvm_value_t right;

	// catch structured types and floats
	if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
		printf_error(expr->binop.right->loc, "struct type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		right = (llvm_value_t){ .type=LLVM_VALUE_POISON };
	} else if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT){
		printf_error(expr->binop.right->loc, "floating point type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		right = (llvm_value_t){ .type=LLVM_VALUE_POISON };
	} else if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_POINTER){
		printf_error(expr->binop.right->loc, "pointer type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
		return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
	} else {
		right = llvm_cast_to_i1(right_operand, f);
	}
	
	llvm_label_t next_label = llvm_add_block(f);
	long_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
	if (expr->binop.op == AST_EXPR_BINOP_LAND){
		base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=left, .br.iftrue=long_label, .br.iffalse=next_label };
	}else{
		base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=left, .br.iftrue=next_label, .br.iffalse=long_label };
	}

	llvm_reg_t out = llvm_add_inst(f, (llvm_inst_t){
		.type=LLVM_INST_PHI,
		.phi.type=LLVM_TYPE_I1,
		.phi.count=2,
		.phi.labels = {base_label, long_label},
		.phi.values = {left, right}
	});
	return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=out, .typed_reg.ast_type=&context_get(&top_level_context, "bool", (void*)0)->type_ };
}

static llvm_typed_value_t ast2llvm_emit_expr(const ast_expr_t* expr, const var2reg_map_t* var2reg, llvm_function_t* f){
	switch (expr->type){
		case AST_EXPR_CONST:
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_INT_CONST, .int_const=expr->constant.value };
		case AST_EXPR_LVALUE:
			return ast2llvm_read_lvalue(var2reg, expr->lvalue, expr->loc, f);
		case AST_EXPR_FUNC_CALL:
		{
			llvm_typed_value_list_t arglist = create_llvm_typed_value_list();
			for (unsigned int i=0; i < expr->func_call.arglist.len; i++){
				llvm_typed_value_t arg = ast2llvm_emit_expr(&expr->func_call.arglist.data[i], var2reg, f);
				llvm_typed_value_list_append(&arglist, arg);
			}

			if (arglist.len > expr->func_call.func_ref->args.len){
				printf_error(expr->loc, "too many arguments for call to function '%s'", expr->func_call.func_ref->name);
				printf_info(expr->func_call.func_ref->declare_loc, "declared here");
				return POISON_TYPED_VALUE;
			}
			if (arglist.len < expr->func_call.func_ref->args.len) {
				printf_error(expr->loc, "too few arguments for call to function '%s'", expr->func_call.func_ref->name);
				printf_info(expr->func_call.func_ref->declare_loc, "declared here");
				return POISON_TYPED_VALUE;
			}
			for (unsigned int i=0; i < arglist.len; i++){
				switch (arglist.data[i].type){
					case LLVM_TVALUE_POISON: break; // ok
					case LLVM_TVALUE_INT_CONST:
						if (ast_datatype_eq(&context_get(&top_level_context, "u64", (void*)0)->type_, expr->func_call.func_ref->args.data[i]->var.type_ref)) 
							break;
						printf_error(expr->func_call.arglist.data[i].loc, "incompatible argument of type 'u64' (expecting '%s')",
							expr->func_call.func_ref->args.data[i]->var.type_ref->name);
						printf_info(expr->func_call.func_ref->declare_loc, "declared here");
						return POISON_TYPED_VALUE;
					case LLVM_TVALUE_REG:
						if (ast_datatype_eq(arglist.data[i].typed_reg.ast_type, expr->func_call.func_ref->args.data[i]->var.type_ref))
							break;
						printf_error(expr->func_call.arglist.data[i].loc, "incompatible argument of type '%s' (expecting '%s')",
							arglist.data[i].typed_reg.ast_type->name,
							expr->func_call.func_ref->args.data[i]->var.type_ref->name);
						printf_info(expr->func_call.func_ref->declare_loc, "declared here");
						return POISON_TYPED_VALUE;
				}
			}

			llvm_inst_t inst = (llvm_inst_t){
				.type=LLVM_INST_CALL,
				.call.name=strdup(expr->func_call.func_ref->name),
				.call.rettype=ast_type_to_llvm_type(expr->func_call.func_ref->return_type_ref), // TODO functions returning 'void'
				.call.args=create_llvm_arg_list()
			};
			for (unsigned int i=0; i<arglist.len; i++){
				llvm_arg_list_append(&inst.call.args, (llvm_arg_t){
					.type=ast_type_to_llvm_type(expr->func_call.func_ref->args.data[i]->var.type_ref),
					.val=llvm_untype_value(arglist.data[i])
				});
			}
			llvm_reg_t ret = llvm_add_inst(f, inst);
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=ret, .typed_reg.ast_type=expr->func_call.func_ref->return_type_ref };
		}
		case AST_EXPR_UNOP:
		{
			llvm_typed_value_t operand = ast2llvm_emit_expr(expr->unop.operand, var2reg, f);
			switch (operand.type){
				case LLVM_TVALUE_POISON: return operand;
				case LLVM_TVALUE_INT_CONST:
					switch (expr->unop.op){
						case AST_EXPR_UNOP_NEG: operand.int_const = -operand.int_const; break;
						case AST_EXPR_UNOP_BNOT: operand.int_const = ~operand.int_const; break;
						case AST_EXPR_UNOP_LNOT: operand.int_const = !operand.int_const; break;
						case AST_EXPR_UNOP_DEREF:
							print_error(expr->loc, "cannot dereference integer constant of type 'u64'");
							return POISON_TYPED_VALUE;
					}
					return operand;
				case LLVM_TVALUE_REG: break;
			}
			switch (operand.typed_reg.ast_type->kind){
				case AST_DATATYPE_STRUCTURED:
					printf_error(expr->unop.operand->loc, "struct type '%s' is not supported for unary operation '%s'", operand.typed_reg.ast_type->name, ast_expr_unop_string(expr->unop.op));
					return POISON_TYPED_VALUE;
				case AST_DATATYPE_FLOAT:
					printf_error(expr->unop.operand->loc, "floating point type '%s' is not yet supported for unary operation '%s'", operand.typed_reg.ast_type->name, ast_expr_unop_string(expr->unop.op));
					return POISON_TYPED_VALUE;
				case AST_DATATYPE_POINTER:
					switch (expr->unop.op){
						case AST_EXPR_UNOP_NEG:
						case AST_EXPR_UNOP_BNOT:
						case AST_EXPR_UNOP_LNOT:
							printf_error(expr->unop.operand->loc, "pointer type '%s' is not supported for unary operation '%s'", operand.typed_reg.ast_type->name, ast_expr_unop_string(expr->unop.op));
							return POISON_TYPED_VALUE;
						case AST_EXPR_UNOP_DEREF:
						{
							ast_datatype_t* base_type = operand.typed_reg.ast_type->pointer.base;
							llvm_reg_t reg = llvm_add_inst(f, (llvm_inst_t){
								.type = LLVM_INST_LOAD,
								.load.type = ast_type_to_llvm_type(base_type),
								.load.ptr = llvm_untype_value(operand)
							});
							return (llvm_typed_value_t){ .type = LLVM_TVALUE_REG, .typed_reg.reg = reg, .typed_reg.ast_type = base_type };
						}
					}
				case AST_DATATYPE_INTEGRAL:
					break;
			}
			llvm_type_t optype;
			optype = ast_type_to_llvm_type(operand.typed_reg.ast_type);
			llvm_inst_t inst;
			switch (expr->unop.op){
				case AST_EXPR_UNOP_NEG:
					inst = (llvm_inst_t){
						.type = LLVM_INST_SUB,
						.binop.type = optype,
						.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=0 },
						.binop.second = llvm_untype_value(operand)
					};
					break;
				case AST_EXPR_UNOP_BNOT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_XOR,
						.binop.type = optype,
						.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=0xffffffff }, // TODO: use bitwidth of optype?
						.binop.second = llvm_untype_value(operand)
					};
					break;
				case AST_EXPR_UNOP_LNOT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP, // TODO: make ast type 'bool' or zext to ast type
						.icmp.type = optype,
						.icmp.cond = LLVM_ICMP_EQ,
						.icmp.op1 = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=0 },
						.icmp.op2 = llvm_untype_value(operand)
					};
					break;
				case AST_EXPR_UNOP_DEREF:
					printf_error(expr->loc, "cannot dereference value of type '%s'", operand.typed_reg.ast_type->name);
					return POISON_TYPED_VALUE;
			}
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=llvm_add_inst(f, inst), .typed_reg.ast_type=operand.typed_reg.ast_type };
		}

		case AST_EXPR_BINOP:
		{
			// short-circuiting && and ||
			if (expr->binop.op == AST_EXPR_BINOP_LAND || expr->binop.op == AST_EXPR_BINOP_LOR){
				return ast2llvm_emit_short_circuiting_expr(expr, var2reg, f);
			}
			llvm_typed_value_t left_operand = ast2llvm_emit_expr(expr->binop.left, var2reg, f);
			llvm_typed_value_t right_operand = ast2llvm_emit_expr(expr->binop.right, var2reg, f);

			// catch poison
			if (left_operand.type == LLVM_TVALUE_POISON || right_operand.type == LLVM_TVALUE_POISON){
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}

			// catch mistyped operands
			if (left_operand.type == LLVM_TVALUE_REG && right_operand.type == LLVM_TVALUE_REG
				&& !ast_datatype_eq(left_operand.typed_reg.ast_type, right_operand.typed_reg.ast_type)){
				printf_error(expr->loc, "binary operation '%s' doesn't support differing types '%s' and '%s'", ast_expr_binop_string(expr->binop.op), left_operand.typed_reg.ast_type->name, right_operand.typed_reg.ast_type->name);
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}

			// catch structured types
			if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
				printf_error(expr->binop.left->loc, "struct type '%s' is not supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}
			if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
				printf_error(expr->binop.right->loc, "struct type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}

			// catch floating point types
			if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT){
				printf_error(expr->binop.left->loc, "floating point type '%s' is not supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}
			if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT){
				printf_error(expr->binop.right->loc, "floating point type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}

			// catch pointer types
			if (left_operand.type == LLVM_TVALUE_REG && left_operand.typed_reg.ast_type->kind == AST_DATATYPE_POINTER){
				printf_error(expr->binop.left->loc, "pointer type '%s' is not supported for binary operation '%s'", left_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}
			if (right_operand.type == LLVM_TVALUE_REG && right_operand.typed_reg.ast_type->kind == AST_DATATYPE_POINTER){
				printf_error(expr->binop.right->loc, "pointer type '%s' is not supported for binary operation '%s'", right_operand.typed_reg.ast_type->name, ast_expr_binop_string(expr->binop.op));
				return (llvm_typed_value_t){ .type=LLVM_TVALUE_POISON };
			}

			// catch constant expressions
			if (left_operand.type == LLVM_TVALUE_INT_CONST && right_operand.type == LLVM_TVALUE_INT_CONST){
				return llvm_int_const_binop(expr, left_operand, right_operand);
			}

			ast_datatype_t* restype;
			bool signed_op;
			if (left_operand.type == LLVM_TVALUE_REG){
				restype   = left_operand.typed_reg.ast_type;
				signed_op = left_operand.typed_reg.ast_type->integral.signed_;
			}else{
				restype   = right_operand.typed_reg.ast_type;
				signed_op = right_operand.typed_reg.ast_type->integral.signed_;
			}
			llvm_type_t optype = ast_type_to_llvm_type(restype);

			// catch constant int truncations
			if (left_operand.type == LLVM_TVALUE_INT_CONST && (left_operand.int_const != ulong_truncate(left_operand.int_const, optype.int_bitwidth))){
				printf_warning(expr->loc, "using integer constant in binary operation '%s' with type '%s' truncates it's value from %lu to %lu",
					ast_expr_binop_string(expr->binop.op), right_operand.typed_reg.ast_type->name, left_operand.int_const, ulong_truncate(left_operand.int_const, optype.int_bitwidth));
				left_operand.int_const = ulong_truncate(left_operand.int_const, optype.int_bitwidth);
			} else if (right_operand.type == LLVM_TVALUE_INT_CONST && (right_operand.int_const != ulong_truncate(right_operand.int_const, optype.int_bitwidth))){
				printf_warning(expr->loc, "using integer constant in binary operation '%s' with type '%s' truncates it's value from %lu to %lu",
					ast_expr_binop_string(expr->binop.op), left_operand.typed_reg.ast_type->name, right_operand.int_const, ulong_truncate(right_operand.int_const, optype.int_bitwidth));
				right_operand.int_const = ulong_truncate(right_operand.int_const, optype.int_bitwidth);
			}

			llvm_inst_t inst = (llvm_inst_t){ // this preset holds for most of the operations
				.binop.type = optype,
				.binop.first = llvm_untype_value(left_operand),
				.binop.second = llvm_untype_value(right_operand),
			};
			switch (expr->binop.op){
				case AST_EXPR_BINOP_ADD:
					inst.type = LLVM_INST_ADD;
					break;
				case AST_EXPR_BINOP_SUB:
					inst.type = LLVM_INST_SUB;
					break;
				case AST_EXPR_BINOP_MUL:
					inst.type = LLVM_INST_MUL;
					break;
				case AST_EXPR_BINOP_DIV:
					inst.type = signed_op ? LLVM_INST_SDIV : LLVM_INST_UDIV;
					break;
				case AST_EXPR_BINOP_MOD:
					inst.type = signed_op ? LLVM_INST_SREM : LLVM_INST_UREM;
					break;
				case AST_EXPR_BINOP_LT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = signed_op ? LLVM_ICMP_SLT : LLVM_ICMP_ULT,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_GT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = signed_op ? LLVM_ICMP_SGT : LLVM_ICMP_UGT,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_LE:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = signed_op ? LLVM_ICMP_SLE : LLVM_ICMP_ULE,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_GE:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = signed_op ? LLVM_ICMP_SGE : LLVM_ICMP_UGE,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_EQ:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = LLVM_ICMP_EQ,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_NE:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = LLVM_ICMP_NE,
						.icmp.type = optype,
						.icmp.op1 = llvm_untype_value(left_operand),
						.icmp.op2 = llvm_untype_value(right_operand)
					};
					break;
				case AST_EXPR_BINOP_SHL:
					inst.type = LLVM_INST_SHL;
					break;
				case AST_EXPR_BINOP_SHR:
					inst.type = signed_op ? LLVM_INST_ASHR : LLVM_INST_LSHR;
					break;
				case AST_EXPR_BINOP_BXOR:
					inst.type = LLVM_INST_XOR;
					break;
				case AST_EXPR_BINOP_BAND:
					inst.type = LLVM_INST_AND;
					break;
				case AST_EXPR_BINOP_BOR:
					inst.type = LLVM_INST_OR;
					break;
				case AST_EXPR_BINOP_LAND:
					printf("<unimplemented (land)>\n"); exit(1);
				case AST_EXPR_BINOP_LOR:
					printf("<unimplemented (lor)>\n"); exit(1);
			}
			llvm_reg_t out = llvm_add_inst(f, inst);
			if (inst.type == LLVM_INST_ICMP){ // icmp instruction always returns i1 instead of optype
				out = llvm_add_inst(f,
					(llvm_inst_t){
						.type = LLVM_INST_ZEXT,
						.ext.from = LLVM_I1,
						.ext.to = optype,
						.ext.operand = (llvm_value_t){.type=LLVM_VALUE_REG, .reg=out},
					});
			}
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=out, .typed_reg.ast_type = restype };
		}
		case AST_EXPR_REF:
			return ast2llvm_get_lvalue_pointer(var2reg, expr->ref.lvalue, expr->loc, f);
	}
}

static void ast2llvm_emit_stmt(ast_stmt_t* stmt, const var2reg_map_t* var2reg, llvm_function_t* f, ast_func_t ast_func){
	switch (stmt->type){
		case AST_STMT_IF:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_.cond, var2reg, f), f);
			llvm_label_t base_label = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iftrue_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_.iftrue, var2reg, f, ast_func);
			llvm_label_t iftrue_end = llvm_function_last_label(f);			

			llvm_label_t next_label = llvm_add_block(f);
			f->blocks.data[iftrue_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[base_label.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_start, .br.iffalse=next_label };

			return;
		}
		case AST_STMT_IF_ELSE:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_else.cond, var2reg, f), f);
			llvm_label_t base_label = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iftrue_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_else.iftrue, var2reg, f, ast_func);
			llvm_label_t iftrue_end = llvm_function_last_label(f);

			llvm_add_block(f);
			llvm_label_t iffalse_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->if_else.iffalse, var2reg, f, ast_func);
			llvm_label_t iffalse_end = llvm_function_last_label(f);

			llvm_label_t next_label = llvm_add_block(f);
			f->blocks.data[iftrue_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[iffalse_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			f->blocks.data[base_label.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_start, .br.iffalse=iffalse_start };

			return;
		}
		case AST_STMT_EXPR:
			ast2llvm_emit_expr(&stmt->expr, var2reg, f);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				ast2llvm_emit_stmt(&stmt->block.stmtlist.data[i], var2reg, f, ast_func);
			}
			return;
		case AST_STMT_ASSIGN:
			{
				llvm_typed_value_t val = ast2llvm_emit_expr(&stmt->assign.val, var2reg, f);
				ast_datatype_t* target_type = stmt->assign.lvalue.type;
				switch (val.type){
					case LLVM_TVALUE_REG:
						if (!ast_datatype_eq(target_type, val.typed_reg.ast_type)){
							printf_error(stmt->loc, "attempt to assign type '%s' to %s '%s' of type '%s'",
								val.typed_reg.ast_type->name,
								target_type->name,
								stmt->assign.lvalue.member_access.len ? "member" : "variable",
								stmt->assign.lvalue.member_access.len ?
									stmt->assign.lvalue.type->structure.members.data[stmt->assign.lvalue.member_access.data[stmt->assign.lvalue.member_access.len-1].member_idx].type_ref->name
									: stmt->assign.lvalue.base_var->name
								);
							val = POISON_TYPED_VALUE;
						}
						break;
					case LLVM_TVALUE_INT_CONST:
						if (!ast_datatype_eq(target_type, &context_get(&top_level_context, "u64", (void*)0)->type_)){
							printf_error(stmt->loc, "attempt to assign type 'u64' to %s '%s' of type '%s'",
								target_type->name,
								stmt->assign.lvalue.member_access.len ? "member" : "variable",
								stmt->assign.lvalue.member_access.len ?
									stmt->assign.lvalue.type->structure.members.data[stmt->assign.lvalue.member_access.data[stmt->assign.lvalue.member_access.len-1].member_idx].type_ref->name
									: stmt->assign.lvalue.base_var->name
								);
							val = POISON_TYPED_VALUE;
						}
						break;
					case LLVM_TVALUE_POISON:
						break;
				}
				llvm_typed_value_t ptr = ast2llvm_get_lvalue_pointer(var2reg, stmt->assign.lvalue, stmt->loc, f);
				if (ptr.type == LLVM_TVALUE_POISON)
					return;
				llvm_add_inst(f, (llvm_inst_t){
					.type = LLVM_INST_STORE,
					.store.type = ast_type_to_llvm_type(target_type),
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
				
				llvm_typed_value_t val = ast2llvm_emit_expr(stmt->return_.val, var2reg, f);

				switch (val.type){
					case LLVM_TVALUE_POISON: break;
					case LLVM_TVALUE_INT_CONST:
						if (!ast_datatype_eq(&context_get(&top_level_context, "u64", (void*)0)->type_, ast_func.return_type_ref)){
							printf_error(stmt->loc, "invalid return type 'u64' (expected '%s')", ast_func.return_type_ref->name);
							val = POISON_TYPED_VALUE;
						}
						break;
					case LLVM_TVALUE_REG:
						if (!ast_datatype_eq(val.typed_reg.ast_type, ast_func.return_type_ref)){
							printf_error(stmt->loc, "invalid return type '%s' (expected '%s')", val.typed_reg.ast_type->name, ast_func.return_type_ref->name);
							val = POISON_TYPED_VALUE;
						}
						break;
				}
				
				llvm_function_last_block(f)->term_inst = (llvm_term_inst_t){
					.type = LLVM_TERM_INST_RET,
					.ret.type = f->rettype,
					.ret.value = llvm_untype_value(val)
				};
				llvm_add_block(f); // add a dead block to consume any extra instructions
				return;
			}
		case AST_STMT_FOR:		printf("<unimplemented (for)>\n"); exit(1); // TODO
		case AST_STMT_WHILE:
		{
			llvm_label_t base_label = llvm_function_last_label(f);
			
			llvm_add_block(f);
			llvm_label_t cond_start = llvm_function_last_label(f);
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->while_.cond, var2reg, f), f);
			llvm_label_t cond_end = llvm_function_last_label(f);
			
			llvm_add_block(f);
			llvm_label_t body_start = llvm_function_last_label(f);
			ast2llvm_emit_stmt(stmt->while_.body, var2reg, f, ast_func);
			llvm_label_t body_end = llvm_function_last_label(f);

			llvm_label_t next = llvm_add_block(f);
			f->blocks.data[base_label.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			f->blocks.data[cond_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=body_start, .br.iffalse=next };
			f->blocks.data[body_end.idx].term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=cond_start };
			return;
		}
	}
}

llvm_function_t ast2llvm_emit_func(ast_func_t func){
	llvm_function_t f;
	f.name = strdup(func.name);
	f.blocks = create_llvm_basic_block_list();

	if (func.return_type_ref){
		f.has_return = true;
		f.rettype = ast_type_to_llvm_type(func.return_type_ref);
	}else{
		f.has_return = false;
	}

	f.arg_count = func.args.len;
	f.args = malloc(f.arg_count*sizeof(llvm_type_t));
	for (unsigned int i=0; i < func.args.len; i++){
		f.args[i] = ast_type_to_llvm_type(func.args.data[i]->var.type_ref);
	}

	llvm_basic_block_list_append(&f.blocks, (llvm_basic_block_t){
		.regbase = func.args.len,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});

	var2reg_map_t var2reg = create_var2reg_map();
	
	ast2llvm_alloca_context(func.context, &var2reg, &f);
	for (unsigned int i=0; i < func.args.len; i++){
		ast_variable_t* var = &func.args.data[i]->var;
		llvm_add_inst(&f, (llvm_inst_t){
			.type = LLVM_INST_STORE,
			.store.type = ast_type_to_llvm_type(var->type_ref),
			.store.value = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=(llvm_reg_t){ .idx=i } },
			.store.ptr = (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=var2reg_map_get(&var2reg, var, LLVM_INVALID_REG) }
		});
	}
	
	ast2llvm_alloca_stmt(func.body, &var2reg, &f);

	ast2llvm_emit_stmt(func.body, &var2reg, &f, func);
	var2reg_map_free(&var2reg);
	return f;
}
