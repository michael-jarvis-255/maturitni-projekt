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

static llvm_basic_block_t* llvm_add_block(llvm_function_t* f){
	llvm_basic_block_t* lastblock = &f->blocks.data[f->blocks.len - 1];
	f->next_reg = lastblock->regbase + lastblock->instructions.len;

	llvm_basic_block_list_append(&f->blocks, (llvm_basic_block_t){
		.regbase = f->next_reg,
		.instructions = create_llvm_inst_list(),
		.term_inst = {.type=LLVM_TERM_INST_NULL}
	});
	return &f->blocks.data[f->blocks.len - 1];
}

static unsigned int llvm_add_inst(llvm_basic_block_t* basic_block, llvm_inst_t inst){
	llvm_inst_list_append(&basic_block->instructions, inst);
	return basic_block->regbase + basic_block->instructions.len - 1;
}

static llvm_type_t ast2llvm_type(ast_datatype_t* t){
	
}

static llvm_value_t llvm_emit_ast_expr(ast_expr_t* expr, var2reg_map_t* var2reg, llvm_basic_block_t* current_block, llvm_function_t* f){
	switch (expr->type){
		case AST_EXPR_CONST:
			return (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=expr->constant.value };
		case AST_EXPR_VAR_REF:
			int reg = var2reg_map_get(var2reg, expr->var_ref, -1);
			if (reg != -1)
				return (llvm_value_t){ .type=LLVM_VALUE_REG, .reg.reg=reg, .reg.ast_type=expr->var_ref->type_ref };
			else 
				return (llvm_value_t){ .type=LLVM_VALUE_UNDEF };
		case AST_EXPR_FUNC_CALL:	printf("<unimplemented>\n"); exit(1);
		case AST_EXPR_UNOP:
			llvm_value_t operand = llvm_emit_ast_expr(expr->unop.operand, var2reg, current_block, f);
			llvm_inst_t inst; // TODO: variable types, register types
			switch (expr->unop.op){
				case AST_EXPR_UNOP_NEG:
					inst = (llvm_inst_t){
						.type = LLVM_INST_SUB,
						.binop.first = (llvm_value_t){ .type=LLVM_VALUE_INTEGRAL, .integral=0 },
						.binop.second = operand
					};
					break;
				case AST_EXPR_UNOP_BNOT:
					// TODO
				case AST_EXPR_UNOP_LNOT:
					// TODO
			}
			// TODO
		case AST_EXPR_BINOP:		printf("<unimplemented>\n"); exit(1);
	}
}

static void llvm_emit_ast_stmt(ast_stmt_t* stmt, var2reg_map_t* var2reg, llvm_basic_block_t* current_block, llvm_function_t* f){
	switch (stmt->type){
		case AST_STMT_IF:		printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_IF_ELSE:	printf("<unimplemented>\n"); exit(1); // TODO
		case AST_STMT_EXPR:
			llvm_emit_ast_expr(&stmt->expr, var2reg, current_block, f);
			return;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				llvm_emit_ast_stmt(&stmt->block.stmtlist.data[i], var2reg, current_block, f);
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
	// init return type
	// init argument list

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
}
