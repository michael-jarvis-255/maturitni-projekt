#include <string.h>
#include "llvm.h"

static inline llvm_label_t llvm_function_last_label(const llvm_function_t* f){
	return (llvm_label_t){ .idx = f->blocks.len - 1 }; 
}
static inline llvm_reg_t llvm_block_last_reg(const llvm_basic_block_t* b){
	return (llvm_reg_t){ .idx = b->instructions.len - 1};
}
static inline llvm_value_t llvm_untype_value(const llvm_typed_value_t tv){
	llvm_value_t v;
	v.type = tv.type;
	switch (tv.type){
			case LLVM_VALUE_INT_CONST:
				v.int_const = tv.int_const;
				break;
			case LLVM_VALUE_REG:
				v.reg = tv.typed_reg.reg;
				break;
			case LLVM_VALUE_UNDEF:
			case LLVM_VALUE_POISON:
				break;
	}
	return v;
}
static inline llvm_typed_value_t llvm_var2reg_get_typed_value(var2reg_map_t* var2reg, ast_variable_t* var){
	llvm_reg_t reg = var2reg_map_get(var2reg, var, LLVM_INVALID_REG);
	if (LLVM_REG_EQ(reg, LLVM_INVALID_REG))
		return (llvm_typed_value_t){ .type=LLVM_VALUE_UNDEF };
	return (llvm_typed_value_t){ .type=LLVM_VALUE_REG, .typed_reg.reg=reg, .typed_reg.ast_type=var->type_ref };
}

static unsigned long ast_variable_ptr_hash(ast_variable_ptr p){
	unsigned long h = 1337;
	while (p){
		h = (h*6364136223846793005) + (unsigned long)p;
		p = (void*)((unsigned long)p >> 8);
	}
	return h;
}
static inline bool ast_variable_ptr_cmp(ast_variable_ptr a, ast_variable_ptr b){ return a == b; }
create_hashmap_type_impl(ast_variable_ptr, llvm_reg_t, var2reg_map)

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
	llvm_basic_block_t* block = &f->blocks.data[f->blocks.len];
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

static unsigned int max(unsigned int a, unsigned int b){
	return a > b ? a : b;
}

static llvm_typed_value_t llvm_emit_ast_expr(ast_expr_t* expr, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (expr->type){
		case AST_EXPR_CONST:
			return (llvm_typed_value_t){ .type=LLVM_VALUE_INT_CONST, .int_const=expr->constant.value };
		case AST_EXPR_VAR_REF:
			return llvm_var2reg_get_typed_value(var2reg, expr->var_ref);
		case AST_EXPR_FUNC_CALL:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_EXPR_UNOP:
		{
			llvm_typed_value_t operand = llvm_emit_ast_expr(expr->unop.operand, var2reg, f);
			if (operand.type == LLVM_VALUE_REG && operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
				printf("ERROR: cannot use structured data type in unary operation\n"); // TODO: better error msg
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (operand.type == LLVM_VALUE_REG && operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT){
				printf("ERROR: floats not yet supported\n"); // TODO: better error msg
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (operand.type == LLVM_VALUE_POISON){
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}
			llvm_type_t optype;
			if (operand.type == LLVM_VALUE_REG){
				optype = ast_type_to_llvm_type(operand.typed_reg.ast_type);
			} else {
				optype = (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=64 };
			}
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
			return (llvm_typed_value_t){ .type=LLVM_VALUE_REG, .typed_reg.reg=llvm_add_inst(f, inst), .typed_reg.ast_type=0 }; // TODO: assign ast type
		}
		case AST_EXPR_BINOP:
		{
			// TODO: short-circuiting && and ||
			llvm_typed_value_t left_operand = llvm_emit_ast_expr(expr->binop.left, var2reg, f);
			llvm_typed_value_t right_operand = llvm_emit_ast_expr(expr->binop.right, var2reg, f);

			if (left_operand.type == LLVM_VALUE_POISON || right_operand.type == LLVM_VALUE_POISON){
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (left_operand.type == LLVM_VALUE_REG && (left_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT || left_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED)){
				printf("ERROR: structs and floats aren't allowed in operations\n");
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (right_operand.type == LLVM_VALUE_REG && (right_operand.typed_reg.ast_type->kind == AST_DATATYPE_FLOAT || right_operand.typed_reg.ast_type->kind == AST_DATATYPE_STRUCTURED)){
				printf("ERROR: structs and floats aren't allowed in operations\n");
				return (llvm_typed_value_t){ .type=LLVM_VALUE_POISON };
			}

			llvm_type_t left_type, righ_type, optype;
			if (left_operand.type == LLVM_VALUE_REG){
				left_type = ast_type_to_llvm_type(left_operand.typed_reg.ast_type);
			} else {
				left_type = (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=64 };
			}
			if (right_operand.type == LLVM_VALUE_REG){
				righ_type = ast_type_to_llvm_type(right_operand.typed_reg.ast_type);
			} else {
				righ_type = (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth=64 };
			}
			optype = (llvm_type_t){ .type=LLVM_TYPE_INTEGRAL, .int_bitwidth = max(left_type.int_bitwidth, righ_type.int_bitwidth) }; // TODO: if one opearand is LLVM_TYPE_INTEGRAL, it should be truncated to the size of the other

			// cast both operands to the same type, if they are different
			// TODO: what to do with signed / unsigned integers?
			left_operand = llvm_extend(left_operand, optype); // TODO
			righ_type = llvm_extend(righ_type, optype);

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
				case AST_EXPR_BINOP_DIV: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_MOD: // TODO: signedness 
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_LT: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_GT: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_LE: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_GE: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_EQ: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_NE: // TODO: signedness
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_SHL:
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_SHR:
					printf("<unimplemented>\n"); exit(1);
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
					printf("<unimplemented>\n"); exit(1);
				case AST_EXPR_BINOP_LOR:
					printf("<unimplemented>\n"); exit(1);
			}
			return (llvm_typed_value_t){ .type=LLVM_VALUE_REG, .typed_reg.reg=llvm_add_inst(f, inst), .typed_reg.ast_type = 0 }; // TODO: assign ast type
		}
	}
}

static void llvm_merge_var2reg_maps(llvm_function_t* f, llvm_label_t block1, var2reg_map_t* var2reg1, llvm_label_t block2, var2reg_map_t* var2reg2){
	for (var2reg_map_iterator_t iter=var2reg_map_iter(var2reg1); iter.current; iter = var2reg_map_iter_next(iter)){
		const ast_variable_t* var = iter.current->key;
		llvm_reg_t reg1 = iter.current->value;
		llvm_reg_t reg2 = var2reg_map_get(var2reg2, var, LLVM_INVALID_REG);
		if (LLVM_REG_EQ(reg1, reg2)) continue;
		
		llvm_reg_t new = llvm_add_inst(f, (llvm_inst_t){
			.type=LLVM_INST_PHI,
			.phi.label1 = block1,
			.phi.reg1 = reg1,
			.phi.label2 = block2,
			.phi.reg2 = reg2 // TODO: if reg2 is LLVM_INVALID_REG, then use LLVM_UNDEF
		});
		var2reg_map_set(var2reg1, var, new);
		var2reg_map_set(var2reg2, var, new);
	}
	for (var2reg_map_iterator_t iter=var2reg_map_iter(var2reg2); iter.current; iter = var2reg_map_iter_next(iter)){
		const ast_variable_t* var = iter.current->key;
		llvm_reg_t reg2 = iter.current->value;
		llvm_reg_t reg1 = var2reg_map_get(var2reg1, var, LLVM_INVALID_REG);
		if (LLVM_REG_EQ(reg1, reg2)) continue;
		
		llvm_reg_t new = llvm_add_inst(f, (llvm_inst_t){
			.type=LLVM_INST_PHI,
			.phi.label1 = block1,
			.phi.reg1 = reg1,
			.phi.label2 = block2,
			.phi.reg2 = reg2 // TODO: if reg2 is LLVM_INVALID_REG, then use LLVM_UNDEF
		});
		var2reg_map_set(var2reg1, var, new);
		var2reg_map_set(var2reg2, var, new);
	}
}

static void llvm_emit_ast_stmt(ast_stmt_t* stmt, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (stmt->type){
		case AST_STMT_IF:
		{
			llvm_typed_value_t cond = llvm_emit_ast_expr(&stmt->if_.cond, var2reg, f);
			cond = llvm_cast_to_i1(cond, f); // TODO
			llvm_label_t base_label = llvm_function_last_label(f);
			llvm_basic_block_t* base_block = &f->blocks.data[base_label.idx];

			llvm_label_t iftrue_label = llvm_add_block(f);
			llvm_basic_block_t* iftrue_block = &f->blocks.data[iftrue_label.idx];
			var2reg_map_t var2reg_iftrue = var2reg_map_copy(var2reg);
			llvm_emit_ast_stmt(stmt->if_.iftrue, &var2reg_iftrue, f);

			llvm_label_t next_label = llvm_add_block(f);
			iftrue_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_label };
			base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=llvm_untype_value(cond), .br.iftrue=iftrue_label, .br.iffalse=next_label };

			llvm_merge_var2reg_maps(f, base_label, var2reg, iftrue_label, &var2reg_iftrue);
			var2reg_map_free(&var2reg_iftrue);
			return;
		}
		case AST_STMT_IF_ELSE:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_EXPR:
			llvm_emit_ast_expr(&stmt->expr, var2reg, f);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				llvm_emit_ast_stmt(&stmt->block.stmtlist.data[i], var2reg, f);
			}
			return;
		case AST_STMT_ASSIGN:
			{
				llvm_typed_value_t val = llvm_emit_ast_expr(&stmt->assign.val, var2reg, f);
				// TODO: ensure that val and stmt->assign.var_ref are of the same ast type!!
				var2reg_map_set(var2reg, stmt->assign.var_ref, val.typed_reg.reg);
			}
		case AST_STMT_BREAK:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_CONTINUE:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_RETURN:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_FOR:		printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_WHILE:	printf("<unimplemented>\n"); exit(1); // TODO
	}
}

llvm_function_t llvm_emit_ast_func(ast_func_t func){
	llvm_function_t f;
	f.name = strdup(func.name);
	f.blocks = create_llvm_basic_block_list();
	// TODO init return type
	// TODO init argument list

	var2reg_map_t var2reg = create_var2reg_map();
	for (unsigned int i=0; i < func.args.len; i++){
		var2reg_map_insert(&var2reg, &func.args.data[i], (llvm_reg_t){.idx=i});
	}

	llvm_basic_block_list_append(&f.blocks, (llvm_basic_block_t){
		.regbase = func.args.len,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});

	llvm_emit_ast_stmt(func.body, &var2reg, &f);
	var2reg_map_free(&var2reg);
	return f;
}
