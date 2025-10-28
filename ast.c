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
	return (ast_expr_t){
		.type = AST_EXPR_CONST,
		.constant.value = value
	};
}
ast_expr_t create_ast_expr_variable(const char* id){
	char* name = strcpy(malloc(strlen(id)+1), id);
	return (ast_expr_t){
		.type = AST_EXPR_VARIABLE,
		.variable.id = name
	};
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


void print_ast_expr(const ast_expr_t* exp){
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


ast_stmt_t create_ast_stmt_block(){
	return (ast_stmt_t){
		.type = AST_STMT_BLOCK,
		.block.cap = 4,
		.block.len = 0,
		.block.stmtlist = malloc(sizeof(ast_stmt_t)*4)
	};
}
ast_stmt_t create_ast_stmt_expr(ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_EXPR,
		.expr = expr
	};
}
ast_stmt_t create_ast_stmt_assign(const char* name, ast_expr_t value){
	return (ast_stmt_t){
		.type = AST_STMT_ASSIGN,
		.assign.name = name,
		.assign.val = value
	};
}
ast_stmt_t create_ast_stmt_if(ast_expr_t cond, ast_stmt_t iftrue){
	ast_stmt_t* iftrue_ptr = malloc(sizeof(ast_stmt_t));
	*iftrue_ptr = iftrue;
	return (ast_stmt_t){
		.type = AST_STMT_IF,
		.if_.cond = cond,
		.if_.iftrue = iftrue_ptr
	};
}
ast_stmt_t create_ast_stmt_if_else(ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse){
	ast_stmt_t* iftrue_ptr = malloc(sizeof(ast_stmt_t));
	*iftrue_ptr = iftrue;
	ast_stmt_t* iffalse_ptr = malloc(sizeof(ast_stmt_t));
	*iffalse_ptr = iffalse;
	return (ast_stmt_t){
		.type = AST_STMT_IF_ELSE,
		.if_else.cond = cond,
		.if_else.iftrue = iftrue_ptr,
		.if_else.iffalse = iffalse_ptr
	};
}
ast_stmt_t create_ast_stmt_declare(const char* type, const char* name){
	return (ast_stmt_t){
		.type = AST_STMT_DECLARE,
		.declare.type = type,
		.declare.name = name,
		.declare.val = 0
	};
}
ast_stmt_t create_ast_stmt_declare_assign(const char* type, const char* name, ast_expr_t value){
	ast_expr_t* valptr = malloc(sizeof(ast_expr_t));
	*valptr = value;
	return (ast_stmt_t){
		.type = AST_STMT_DECLARE,
		.declare.type = type,
		.declare.name = name,
		.declare.val = valptr
	};
}

void ast_stmt_block_append(ast_stmt_t* block, ast_stmt_t stmt){
	if (block->type != AST_STMT_BLOCK) {
		printf("INTERNAL ERROR!\n\tfile: %s\n\tline:%i", __FILE__, __LINE__);
		exit(-1);
	}
	if (block->block.cap == block->block.len){
		block->block.cap *= 2;
		block->block.stmtlist = reallocarray(block->block.stmtlist, block->block.cap, sizeof(ast_stmt_t));
	}
	block->block.stmtlist[block->block.len] = stmt;
	block->block.len++;
}


void print_ast_stmt(const ast_stmt_t* stmt, int depth){
	switch (stmt->type){
		case AST_STMT_IF:
			printf("%*s", depth*2, "");
			printf("if ");
			print_ast_expr(&stmt->if_.cond);
			printf(" {\n");
			print_ast_stmt(stmt->if_.iftrue, depth+1);
			printf("%*s", depth*2, "");
			printf("}\n");
			break;
		case AST_STMT_IF_ELSE:
			printf("%*s", depth*2, "");
			printf("if ");
			print_ast_expr(&stmt->if_else.cond);
			printf(" {\n");
			print_ast_stmt(stmt->if_else.iftrue, depth+1);
			printf("%*s", depth*2, "");
			printf("} else {\n");
			print_ast_stmt(stmt->if_else.iffalse, depth+1);
			printf("%*s", depth*2, "");
			printf("}\n");
			break;
		case AST_STMT_EXPR:
			printf("%*s", depth*2, "");
			print_ast_expr(&stmt->expr);
			printf(";");
			break;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.len; i++){
				print_ast_stmt(&stmt->block.stmtlist[i], depth);
			}
			break;
		case AST_STMT_DECLARE:
			printf("%*s", depth*2, "");
			printf("%s %s", stmt->declare.type, stmt->declare.name);
			if (stmt->declare.val){
				printf(" = ");
				print_ast_expr(stmt->declare.val);
				printf(";");
			}
			printf("\n");
			break;
		case AST_STMT_ASSIGN:
			printf("%*s", depth*2, "");
			printf("%s = ", stmt->assign.name);
			print_ast_expr(&stmt->assign.val);
			printf(";\n");
			break;
	}
}

