#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

bool ast_expr_op_is_unary(ast_expr_op_enum_t op){
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
		case AST_EXPR_OP_SHR:
		case AST_EXPR_OP_SHL:
			return false;
	
		case AST_EXPR_OP_BNOT:
		case AST_EXPR_OP_LNOT:
		case AST_EXPR_OP_NEG:
			return true;
	}
}

const char* ast_expr_op_string(ast_expr_op_enum_t op){
	switch (op){
		case AST_EXPR_OP_ADD: return "+";
		case AST_EXPR_OP_SUB: return "-";
		case AST_EXPR_OP_MUL: return "*";
		case AST_EXPR_OP_DIV: return "/";
		case AST_EXPR_OP_MOD: return "%";
		case AST_EXPR_OP_LT: return "<";
		case AST_EXPR_OP_GT: return ">";
		case AST_EXPR_OP_LE: return "<=";
		case AST_EXPR_OP_GE: return ">=";
		case AST_EXPR_OP_EQ: return "==";
		case AST_EXPR_OP_NE: return "!=";
		case AST_EXPR_OP_BXOR: return "^";
		case AST_EXPR_OP_BAND: return "&";
		case AST_EXPR_OP_BOR: return "|";
		case AST_EXPR_OP_LAND: return "&&";
		case AST_EXPR_OP_LOR: return "||";
		case AST_EXPR_OP_SHR: return ">>";
		case AST_EXPR_OP_SHL: return "<<";
		case AST_EXPR_OP_BNOT: return "~";
		case AST_EXPR_OP_LNOT: return "!";
		case AST_EXPR_OP_NEG: return "-";
	}
}

ast_expr_t create_ast_expr_const(unsigned long value){
	return (ast_expr_t){.type = AST_EXPR_CONST, .constant.value = value};
}
ast_expr_t create_ast_expr_variable(const char* id){
	char* name = strcpy(malloc(strlen(id)+1), id);
	return (ast_expr_t){.type = AST_EXPR_VARIABLE, .variable.id = name};
}
ast_expr_t create_ast_expr_op(ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right){
	ast_expr_t* l = 0;
	ast_expr_t* r = 0;

	l = malloc(sizeof(ast_expr_t));
	*l = left;
	
	if (!ast_expr_op_is_unary(op)){
		r = malloc(sizeof(ast_expr_t));
		*r = right;
	}
	
	return (ast_expr_t){.type = AST_EXPR_OP, .op.op = op, .op.left = l, .op.right = r};
}


void print_ast_expr(ast_expr_t* exp){
	switch (exp->type){
		case AST_EXPR_CONST:
			printf("%lu", exp->constant.value);
			return;

		case AST_EXPR_VARIABLE:
			printf("%s", exp->variable.id);
			return;

		case AST_EXPR_FUNC_CALL:
			printf("TODO");
			return;

		case AST_EXPR_OP:
			printf("(");
			if (!ast_expr_op_is_unary(exp->op.op)){
				print_ast_expr(exp->op.left);
			}
			printf("%s", ast_expr_op_string(exp->op.op));
			if (ast_expr_op_is_unary(exp->op.op)){
				print_ast_expr(exp->op.left);
			}else{
				print_ast_expr(exp->op.right);
			}
			printf(")");
			return;
	}
}
