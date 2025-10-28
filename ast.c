#include "ast.h"
#include <stdlib.h>

ast_expr_t create_ast_expr_const(unsigned long value){
	return (ast_expr_t){.type = AST_EXPR_CONST, .constant.value = value};
}
ast_expr_t create_ast_expr_variable(const char* id){
	return (ast_expr_t){.type = AST_EXPR_VARIABLE, .variable.id = id};
}
ast_expr_t create_ast_expr_op(ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right){
	ast_expr_t* l = 0;
	ast_expr_t* r = 0;

	switch (op){
		case AST_EXPR_OP_ADD:
		case AST_EXPR_OP_SUB:
		case AST_EXPR_OP_MUL:
		case AST_EXPR_OP_DIV:
		case AST_EXPR_OP_MOD:
		case AST_EXPR_OP_LT:
		case AST_EXPR_OP_GT:
		case AST_EXPR_OP_LE:
		case AST_EXPR_OP_GE:
		case AST_EXPR_OP_EQ:
		case AST_EXPR_OP_NE:
		case AST_EXPR_OP_BXOR:
		case AST_EXPR_OP_BAND:
		case AST_EXPR_OP_BOR:
		case AST_EXPR_OP_LAND:
		case AST_EXPR_OP_LOR:
			r = malloc(sizeof(ast_expr_t));
			*r = right;
	
		case AST_EXPR_OP_BNOT:
		case AST_EXPR_OP_LNOT:
		case AST_EXPR_OP_NEG:
			l = malloc(sizeof(ast_expr_t));
			*l = left;
	}

	return (ast_expr_t){.type = AST_EXPR_OP, .op.op = op, .op.left = l, .op.right = r};
}
