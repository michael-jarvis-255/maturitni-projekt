#include <string.h>
#include "llvm.h"

static unsigned long ast_variable_ptr_hash(ast_variable_ptr p){
	unsigned long h = 1337;
	while (p){
		h = (h*6364136223846793005) + (unsigned long)p;
		p = (void*)((unsigned long)p >> 8);
	}
	return h;
}
static inline bool ast_variable_ptr_cmp(ast_variable_ptr a, ast_variable_ptr b){ return a == b; }
create_hashmap_type_impl(ast_variable_ptr, unsigned int, var2reg_map);

create_list_type_impl(llvm_inst, false);
create_list_type_impl(llvm_basic_block, false);

static unsigned int llvm_add_block(llvm_function_t* f){
	llvm_basic_block_t* lastblock = &f->blocks.data[f->blocks.len - 1];
	f->next_reg = lastblock->regbase + lastblock->instructions.len;

	llvm_basic_block_list_append(&f->blocks, (llvm_basic_block_t){
		.regbase = f->next_reg,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});
	return f->blocks.len-1;
}

static unsigned int llvm_add_inst(llvm_function_t* f, llvm_inst_t inst){
	llvm_basic_block_t* block = &f->blocks.data[f->blocks.len];
	llvm_inst_list_append(&block->instructions, inst);
	return block->regbase + block->instructions.len - 1;
}

static llvm_value_t llvm_emit_ast_expr(ast_expr_t* expr, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (expr->type){
		case AST_EXPR_CONST:
			return (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=expr->constant.value };
		case AST_EXPR_VAR_REF:
			int reg = var2reg_map_get(var2reg, expr->var_ref, -1);
			if (reg != -1)
				return (llvm_value_t){ .type=LLVM_VALUE_REG, .reg.reg=reg, .reg.ast_type=expr->var_ref->type_ref };
			else 
				return (llvm_value_t){ .type=LLVM_VALUE_UNDEF }; // variable has not been bound to a register yet, thus is undefined
		case AST_EXPR_FUNC_CALL:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_EXPR_UNOP:
			llvm_value_t operand = llvm_emit_ast_expr(expr->unop.operand, var2reg, f);
			if (operand.type == LLVM_VALUE_REG && operand.reg.ast_type->kind == AST_DATATYPE_STRUCTURED){
				printf("ERROR: cannot use structured data type in expression\n"); // TODO: better error msg
				return (llvm_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (operand.type == LLVM_VALUE_REG && operand.reg.ast_type->kind == AST_DATATYPE_FLOAT){
				printf("ERROR: floats not yet supported\n"); // TODO: better error msg
				return (llvm_value_t){ .type=LLVM_VALUE_POISON };
			}
			if (operand.type == LLVM_VALUE_POISON){
				return (llvm_value_t){ .type=LLVM_VALUE_POISON }; // TODO: is this valid to do according to llvm langauge docs?
			}
			llvm_inst_t inst; // TODO: variable types, register types
			switch (expr->unop.op){
				case AST_EXPR_UNOP_NEG: // TODO: types
					inst = (llvm_inst_t){
						.type = LLVM_INST_SUB,
						.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=0 },
						.binop.second = operand
					};
					break;
				case AST_EXPR_UNOP_BNOT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_XOR,
						.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=0xffffffff }, // TODO: use bitwidth of operand.reg.ast_type
						.binop.second = operand
					};
					break;
				case AST_EXPR_UNOP_LNOT:
					inst = (llvm_inst_t){
						.type = LLVM_INST_ICMP,
						.icmp.cond = LLVM_ICMP_EQ,
						.icmp.op1 = (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=0 },
						.icmp.op2 = operand,
					};
					break;
			}
			return (llvm_value_t){ .type=LLVM_VALUE_REG, .reg.reg=llvm_add_inst(f, inst), .reg.ast_type=0 }; // TODO: assign ast type
		case AST_EXPR_BINOP:		printf("<unimplemented>\n"); exit(1); // TODO
	}
}

static void llvm_merge_var2reg_maps(llvm_function_t* f, unsigned int block1, var2reg_map_t* var2reg1, unsigned int block2, var2reg_map_t* var2reg2){
	for (var2reg_map_iterator_t iter=var2reg_map_iter(var2reg1); iter.current; iter = var2reg_map_iter_next(iter)){
		const ast_variable_t* var = iter.current->key;
		unsigned int reg1 = iter.current->value;
		unsigned int reg2 = var2reg_map_get(var2reg2, var, -1);
		if (reg1 == reg2) continue;
			
		unsigned int new = llvm_add_inst(f, (llvm_inst_t){
			.type=LLVM_INST_PHI,
			.phi.label1 = block1,
			.phi.reg1 = reg1,
			.phi.label2 = block2,
			.phi.reg2 = reg2
		});
		var2reg_map_set(var2reg1, var, new);
		var2reg_map_set(var2reg2, var, new);
	}
	for (var2reg_map_iterator_t iter=var2reg_map_iter(var2reg2); iter.current; iter = var2reg_map_iter_next(iter)){
		const ast_variable_t* var = iter.current->key;
		unsigned int reg2 = iter.current->value;
		unsigned int reg1 = var2reg_map_get(var2reg1, var, -1);
		if (reg1 == reg2) continue;
			
		unsigned int new = llvm_add_inst(f, (llvm_inst_t){
			.type=LLVM_INST_PHI,
			.phi.label1 = block1,
			.phi.reg1 = reg1,
			.phi.label2 = block2,
			.phi.reg2 = reg2
		});
		var2reg_map_set(var2reg1, var, new);
		var2reg_map_set(var2reg2, var, new);
	}
}

static void llvm_emit_ast_stmt(ast_stmt_t* stmt, var2reg_map_t* var2reg, llvm_function_t* f){
	switch (stmt->type){
		case AST_STMT_IF:
			llvm_value_t cond = llvm_emit_ast_expr(&stmt->if_.cond, var2reg, f);
			cond = llvm_cast_to_i1(cond, f); // TODO
			unsigned int base_block_idx = f->blocks.len - 1;
			llvm_basic_block_t* base_block = &f->blocks.data[f->blocks.len];

			unsigned int iftrue_block_idx = llvm_add_block(f);
			llvm_basic_block_t* iftrue_block = &f->blocks.data[iftrue_block_idx];
			var2reg_map_t var2reg_iftrue = var2reg_map_copy(var2reg);
			llvm_emit_ast_stmt(stmt->if_.iftrue, &var2reg_iftrue, f);

			unsigned int next_block = llvm_add_block(f);
			iftrue_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_JMP, .jmp.target=next_block };
			base_block->term_inst = (llvm_term_inst_t){ .type=LLVM_TERM_INST_BR, .br.cond=cond, .br.iftrue=iftrue_block_idx, .br.iffalse=next_block };

			llvm_merge_var2reg_maps(f, base_block_idx, var2reg, iftrue_block_idx, &var2reg_iftrue);
			var2reg_map_free(&var2reg_iftrue);
			return;
		case AST_STMT_IF_ELSE:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_EXPR:
			llvm_emit_ast_expr(&stmt->expr, var2reg, f);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				llvm_emit_ast_stmt(&stmt->block.stmtlist.data[i], var2reg, f);
			}
			return;
		case AST_STMT_ASSIGN:	printf("<unimplemented>\n"); exit(1); // TODO
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
	f.next_reg = 0;
	// TODO init return type
	// TODO init argument list

	var2reg_map_t var2reg = create_var2reg_map();
	for (unsigned int i=0; i < func.args.len; i++){
		var2reg_map_insert(&var2reg, &func.args.data[i], f.next_reg);
		f.next_reg++;
	}

	llvm_basic_block_list_append(&f.blocks, (llvm_basic_block_t){
		.regbase = f.next_reg,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});

	llvm_emit_ast_stmt(func.body, &var2reg, &f);
	var2reg_map_free(&var2reg);
	return f;
}
