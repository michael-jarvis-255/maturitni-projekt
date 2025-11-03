#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

create_list_type_impl(ast_argdef)
create_list_type_impl(ast_decl)
create_list_type_impl(ast_stmt)

#define convert_to_ptr(x) memcpy(malloc(sizeof(x)), &x, sizeof(x))

static bool ast_expr_op_is_unary(ast_expr_op_enum_t op){
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

static const char* ast_expr_op_string(ast_expr_op_enum_t op){
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

ast_expr_t create_ast_expr_const(loc_t loc, unsigned long value){
	return (ast_expr_t){
		.type = AST_EXPR_CONST,
		.loc = loc,
		.constant.value = value
	};
}
ast_expr_t create_ast_expr_variable(loc_t loc, ast_identifier_t id){
	return (ast_expr_t){
		.type = AST_EXPR_VARIABLE,
		.loc = loc,
		.variable = id
	};
}
ast_expr_t create_ast_expr_op(loc_t loc, ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right){
	return (ast_expr_t){
		.type = AST_EXPR_OP,
		.loc = loc,
		.op.op = op,
		.op.left = convert_to_ptr(left),
		.op.right = ast_expr_op_is_unary(op) ? 0 : convert_to_ptr(right)
	};
}


void print_ast_expr(const ast_expr_t* exp){
	switch (exp->type){
		case AST_EXPR_CONST:
			printf("%lu", exp->constant.value);
			return;

		case AST_EXPR_VARIABLE:
			printf("%s", exp->variable.name);
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


ast_stmt_t create_ast_stmt_block(loc_t loc){
	return (ast_stmt_t){
		.type = AST_STMT_BLOCK,
		.loc = loc,
		.block.stmtlist = create_ast_stmt_list()
	};
}
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_EXPR,
		.loc = loc,
		.expr = expr
	};
}
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_identifier_t name, ast_expr_t value){
	return (ast_stmt_t){
		.type = AST_STMT_ASSIGN,
		.loc = loc,
		.assign.name = name,
		.assign.val = value
	};
}
ast_stmt_t create_ast_stmt_if(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue){
	return (ast_stmt_t){
		.type = AST_STMT_IF,
		.loc = loc,
		.if_.cond = cond,
		.if_.iftrue = convert_to_ptr(iftrue)
	};
}
ast_stmt_t create_ast_stmt_if_else(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse){
	return (ast_stmt_t){
		.type = AST_STMT_IF_ELSE,
		.loc = loc,
		.if_else.cond = cond,
		.if_else.iftrue = convert_to_ptr(iftrue),
		.if_else.iffalse = convert_to_ptr(iffalse)
	};
}
ast_stmt_t create_ast_stmt_declare(loc_t loc, ast_type_t type, ast_identifier_t name){
	return (ast_stmt_t){
		.type = AST_STMT_DECLARE,
		.loc = loc,
		.declare.type = type,
		.declare.name = name,
		.declare.val = 0
	};
}
ast_stmt_t create_ast_stmt_declare_assign(loc_t loc, ast_type_t type, ast_identifier_t name, ast_expr_t value){
	return (ast_stmt_t){
		.type = AST_STMT_DECLARE,
		.loc = loc,
		.declare.type = type,
		.declare.name = name,
		.declare.val = convert_to_ptr(value)
	};
}

ast_stmt_t create_ast_stmt_return(loc_t loc, ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_RETURN,
		.loc = loc,
		.return_.val = expr
	};
}

ast_stmt_t create_ast_stmt_while(loc_t loc, ast_expr_t cond, ast_stmt_t body){
	return (ast_stmt_t){
		.type = AST_STMT_WHILE,
		.loc = loc,
		.while_.cond = cond,
		.while_.body = convert_to_ptr(body)
	};
}
ast_stmt_t create_ast_stmt_for(loc_t loc, ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body){
	return (ast_stmt_t){
		.type = AST_STMT_FOR,
		.loc = loc,
		.for_.cond = cond,
		.for_.init = convert_to_ptr(init),
		.for_.step = convert_to_ptr(step),
		.for_.body = convert_to_ptr(body)
	};
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
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				print_ast_stmt(&stmt->block.stmtlist.data[i], depth);
			}
			break;
		case AST_STMT_DECLARE:
			printf("%*s", depth*2, "");
			printf("%s %s", stmt->declare.type.name, stmt->declare.name.name);
			if (stmt->declare.val){
				printf(" = ");
				print_ast_expr(stmt->declare.val);
				printf(";");
			}
			printf("\n");
			break;
		case AST_STMT_ASSIGN:
			printf("%*s", depth*2, "");
			printf("%s = ", stmt->assign.name.name);
			print_ast_expr(&stmt->assign.val);
			printf(";\n");
			break;
		case AST_STMT_BREAK:
			printf("%*s", depth*2, "");
			printf("break;\n");
			break;
		case AST_STMT_CONTINUE:
			printf("%*s", depth*2, "");
			printf("continue;\n");
			break;
		case AST_STMT_RETURN:
			printf("%*s", depth*2, "");
			printf("return ");
			print_ast_expr(&stmt->return_.val);
			printf(";\n");
			break;
		case AST_STMT_WHILE:
			printf("%*s", depth*2, "");
			printf("while ");
			print_ast_expr(&stmt->while_.cond);
			printf(" {\n");
			print_ast_stmt(stmt->while_.body, depth+1);
			printf("%*s", depth*2, "");
			printf("}\n");
			break;
		case AST_STMT_FOR:
			printf("%*s", depth*2, "");
			printf("for (\n");
			print_ast_stmt(stmt->for_.init, depth+1);
			printf("%*s", depth*2+2, "");
			print_ast_expr(&stmt->while_.cond);
			printf(";\n");
			print_ast_stmt(stmt->for_.step, depth+1);
			printf("%*s", depth*2, "");
			printf(") {\n");
			print_ast_stmt(stmt->for_.body, depth+1);
			printf("%*s", depth*2, "");
			printf("}\n");
			break;
	}
}

ast_decl_t create_ast_decl_function(loc_t loc, ast_type_t returntype, ast_identifier_t name, ast_argdef_list_t args, ast_stmt_t body){
	return (ast_decl_t){
		.type = AST_DECL_FUNCTION,
		.loc = loc,
		.function.returntype = returntype,
		.function.name = name,
		.function.args = args,
		.function.body = convert_to_ptr(body)
	};
}
ast_decl_t create_ast_decl_global(loc_t loc, ast_type_t type, ast_identifier_t name){
	return (ast_decl_t){
		.type = AST_DECL_GLOBAL,
		.loc = loc,
		.global.type = type,
		.global.name = name,
		.global.init = 0
	};
}
ast_decl_t create_ast_decl_global_assign(loc_t loc, ast_type_t type, ast_identifier_t name, ast_expr_t value){
	return (ast_decl_t){
		.type = AST_DECL_GLOBAL,
		.loc = loc,
		.global.type = type,
		.global.name = name,
		.global.init = convert_to_ptr(value)
	};
}

void print_ast_decl_list(const ast_decl_list_t* decllist){
	for (unsigned int i=0; i<decllist->len; i++){
		print_ast_decl(&decllist->data[i]);
	}
}
void print_ast_decl(const ast_decl_t* decl){
	switch (decl->type){
		case AST_DECL_FUNCTION:
			printf("\n%s %s (", decl->function.returntype.name, decl->function.name.name);
			for (unsigned int i=0; i < decl->function.args.len; i++){
				if (i > 0) printf(", ");
				printf("%s %s", decl->function.args.data[i].type.name, decl->function.args.data[i].name.name);
			}
			printf(") {\n");
			print_ast_stmt(decl->function.body, 1);
			printf("}\n");
			break;
		case AST_DECL_GLOBAL:
			printf("%s %s", decl->global.type.name, decl->global.name.name);
			if (decl->global.init){
				printf(" = ");
				print_ast_expr(decl->global.init);
			}
			printf(";\n");
			break;
		case AST_DECL_TYPE:
			printf("<<unimplemented>>\n");
			break;
	}
}
