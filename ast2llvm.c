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

#define POISON_TYPED_VALUE ((llvm_typed_value_t){ .type=LLVM_TVALUE_POISON })
#define POISON_VALUE ((llvm_value_t){ .type=LLVM_VALUE_POISON })

static inline llvm_label_t llvm_function_last_label(const llvm_function_t* f){
	return (llvm_label_t){ .idx = f->blocks.len - 1 }; 
}
static inline llvm_reg_t llvm_block_last_reg(const llvm_basic_block_t* b){
	return (llvm_reg_t){ .idx = b->instructions.len - 1};
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
static inline llvm_typed_value_t llvm_var2reg_get_typed_value(const var2reg_map_t* var2reg, const ast_variable_t* var, loc_t loc){
	llvm_reg_t reg = var2reg_map_get(var2reg, var, LLVM_INVALID_REG);
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG)){
		printf_error(loc, "use of undefined variable '%s'", var->name);
		return POISON_TYPED_VALUE;
	}
	return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=reg, .typed_reg.ast_type=var->type_ref };
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
			return (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=t->bitwidth };
		case AST_DATATYPE_STRUCTURED:
			return (llvm_type_t){ .type=LLVM_TYPE_STRUCT };
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

static llvm_typed_value_t ast2llvm_emit_expr(const ast_expr_t* expr, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (expr->type){
		case AST_EXPR_CONST:
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_INT_CONST, .int_const=expr->constant.value };
		case AST_EXPR_VAR_REF:
			return llvm_var2reg_get_typed_value(var2reg, expr->var_ref, expr->loc);
		case AST_EXPR_FUNC_CALL:	printf("<unimplemented (func call)>\n"); exit(1); // TODO
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
						.type = LLVM_INST_ICMP,
						.icmp.type = optype,
						.icmp.cond = LLVM_ICMP_EQ,
						.icmp.op1 = (llvm_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=0 },
						.icmp.op2 = llvm_untype_value(operand)
					};
					break;
			}
			return (llvm_typed_value_t){ .type=LLVM_TVALUE_REG, .typed_reg.reg=llvm_add_inst(f, inst), .typed_reg.ast_type=0 }; // TODO: assign ast type
		}


		case AST_EXPR_BINOP:
		{
			// TODO: short-circuiting && and ||
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

			// catch constant expressions
			if (left_operand.type == LLVM_TVALUE_INT_CONST && right_operand.type == LLVM_TVALUE_INT_CONST){
				return llvm_int_const_binop(expr, left_operand, right_operand);
			}

			ast_datatype_t* restype;
			bool signed_op;
			if (left_operand.type == LLVM_TVALUE_REG){
				restype   = left_operand.typed_reg.ast_type;
				signed_op = left_operand.typed_reg.ast_type->signed_;
			}else{
				restype   = right_operand.typed_reg.ast_type;
				signed_op = right_operand.typed_reg.ast_type->signed_;
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
					inst.type = LLVM_INST_MUL; // TODO: should i32*i32 give i64?
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
	}
}

static void merge_blocks(llvm_function_t* f, unsigned int n, ...){ // variadic arguments must be "llvm_label_t, var2reg_map_t*, " n times
	if (n == 0) return;

	va_list args;
	va_start(args, n);

	llvm_label_t labels[n];
	var2reg_map_t* maps[n];
	for (unsigned int i=0; i<n; i++){
		labels[i] = va_arg(args, llvm_label_t);
		maps[i] = va_arg(args, var2reg_map_t*);
	}
	va_end(args);

	if (n > LLVM_PHI_MAX){
		printf("INTERNAL ERROR: n > LLVM_PHI_MAX\n");
		exit(1);
	}

	ast_variable_ptr_set_t vars_to_merge = create_ast_variable_ptr_set();
	for (unsigned int i=1; i<n; i++){
		for (var2reg_map_iterator_t iter=var2reg_map_iter(maps[i]); iter.current; iter = var2reg_map_iter_next(iter)){
			const ast_variable_t* var = iter.current->key;
			if (LLVM_REG_EQ(var2reg_map_get(maps[0], var, LLVM_INVALID_REG), iter.current->value))
				continue;

			ast_variable_ptr_set_add(&vars_to_merge, var);
		}
	}

	for (ast_variable_ptr_set_iterator_t iter=ast_variable_ptr_set_iter(&vars_to_merge); iter.current; iter = ast_variable_ptr_set_iter_next(iter)){
		const ast_variable_t* var = iter.current->value;
		llvm_inst_t inst = (llvm_inst_t){
			.type=LLVM_INST_PHI,
			.phi.count=n
		};
		for (unsigned int i=0; i<n; i++){
			inst.phi.labels[i] = labels[i];
			llvm_reg_t r = var2reg_map_get(maps[i], var, LLVM_INVALID_REG);
			inst.phi.values[i] = LLVM_REG_EQ(r, LLVM_INVALID_REG) ? (llvm_value_t){ .type=LLVM_VALUE_UNDEF } : (llvm_value_t){ .type=LLVM_VALUE_REG, .reg=r };
		}
		llvm_reg_t new = llvm_add_inst(f, inst);
		for (unsigned int i=0; i<n; i++){
			var2reg_map_set(maps[i], var, new);
		}
	}
	ast_variable_ptr_set_free(&vars_to_merge);
}

static void ast2llvm_emit_stmt(ast_stmt_t* stmt, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (stmt->type){
		case AST_STMT_IF:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_.cond, var2reg, f), f);
			llvm_label_t base_label = llvm_function_last_label(f);
			llvm_basic_block_t* base_block = &f->blocks.data[base_label.idx];

			llvm_label_t iftrue_label = llvm_add_block(f);
			llvm_basic_block_t* iftrue_block = &f->blocks.data[iftrue_label.idx];
			var2reg_map_t var2reg_iftrue = var2reg_map_copy(var2reg);
			ast2llvm_emit_stmt(stmt->if_.iftrue, &var2reg_iftrue, f);

			llvm_label_t next_label = llvm_add_block(f);
			iftrue_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_label, .br.iffalse=next_label };

			merge_blocks(f, 3,
				base_label,    var2reg,
				iftrue_label, &var2reg_iftrue
			);
			
			var2reg_map_free(&var2reg_iftrue);
			return;
		}
		case AST_STMT_IF_ELSE:
		{
			llvm_value_t cond = llvm_cast_to_i1(ast2llvm_emit_expr(&stmt->if_else.cond, var2reg, f), f);
			llvm_label_t base_label = llvm_function_last_label(f);
			llvm_basic_block_t* base_block = &f->blocks.data[base_label.idx];

			llvm_label_t iftrue_label = llvm_add_block(f);
			llvm_basic_block_t* iftrue_block = &f->blocks.data[iftrue_label.idx];
			var2reg_map_t var2reg_iftrue = var2reg_map_copy(var2reg);
			ast2llvm_emit_stmt(stmt->if_else.iftrue, &var2reg_iftrue, f);

			llvm_label_t iffalse_label = llvm_add_block(f);
			llvm_basic_block_t* iffalse_block = &f->blocks.data[iffalse_label.idx];
			var2reg_map_t var2reg_iffalse = var2reg_map_copy(var2reg);
			ast2llvm_emit_stmt(stmt->if_else.iffalse, &var2reg_iffalse, f);

			llvm_label_t next_label = llvm_add_block(f);
			iftrue_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			iffalse_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_label, .br.iffalse=iffalse_label };

			merge_blocks(f, 3,
				base_label,     var2reg,
				iftrue_label,  &var2reg_iftrue,
				iffalse_label, &var2reg_iffalse
			);

			var2reg_map_free(&var2reg_iftrue);
			var2reg_map_free(&var2reg_iffalse);
			return;
		}
		case AST_STMT_EXPR:
			ast2llvm_emit_expr(&stmt->expr, var2reg, f);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				ast2llvm_emit_stmt(&stmt->block.stmtlist.data[i], var2reg, f);
			}
			return;
		case AST_STMT_ASSIGN:
			{
				llvm_typed_value_t val = ast2llvm_emit_expr(&stmt->assign.val, var2reg, f);
				llvm_reg_t new;
				switch (val.type){
					case LLVM_TVALUE_REG:
						if (ast_datatype_eq(stmt->assign.var_ref->type_ref, val.typed_reg.ast_type)){
							new = val.typed_reg.reg;
						}
						else{
							printf_error(stmt->loc, "attempt to assign type '%s' to variable '%s' of type '%s'", val.typed_reg.ast_type->name, stmt->assign.var_ref->name, stmt->assign.var_ref->type_ref->name);
							new = llvm_add_inst(f, (llvm_inst_t){ .type=LLVM_INST_NOP, .nop.value=POISON_VALUE});
						}
						break;
					case LLVM_TVALUE_INT_CONST:
					case LLVM_TVALUE_POISON:
						new = llvm_add_inst(f, (llvm_inst_t){ .type=LLVM_INST_NOP, .nop.value=llvm_untype_value(val)});
						break;
				}
				var2reg_map_set(var2reg, stmt->assign.var_ref, new);
				return;
			}
		case AST_STMT_BREAK:	printf("<unimplemented (break)>\n"); exit(1); // TODO
		case AST_STMT_CONTINUE:	printf("<unimplemented (continue)>\n"); exit(1); // TODO
		case AST_STMT_RETURN:
			{
				llvm_typed_value_t val = ast2llvm_emit_expr(&stmt->return_.val, var2reg, f);
				// TODO: check value type matches function return type
				llvm_basic_block_t* block = &f->blocks.data[f->blocks.len - 1];
				block->term_inst = (llvm_term_inst_t){
					.type = LLVM_TERM_INST_RET,
					.ret.value = llvm_untype_value(val)
				};
				llvm_add_block(f); // add a dead block to consume any extra instructions
				return;
			}
		case AST_STMT_FOR:		printf("<unimplemented (for)>\n"); exit(1); // TODO
		case AST_STMT_WHILE:	printf("<unimplemented (while)>\n"); exit(1); // TODO
	}
}

llvm_function_t ast2llvm_emit_func(ast_func_t func){
	llvm_function_t f;
	f.name = strdup(func.name);
	f.blocks = create_llvm_basic_block_list();
	// TODO init return type
	// TODO init argument list

	var2reg_map_t var2reg = create_var2reg_map();
	for (unsigned int i=0; i < func.args.len; i++){
		var2reg_map_insert(&var2reg, &func.args.data[i]->var, (llvm_reg_t){.idx=i});
	}

	llvm_basic_block_list_append(&f.blocks, (llvm_basic_block_t){
		.regbase = func.args.len,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});

	ast2llvm_emit_stmt(func.body, &var2reg, &f);
	var2reg_map_free(&var2reg);
	return f;
}
