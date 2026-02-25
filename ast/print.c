#include "print.h"
#include <stdio.h>

static inline void nspaces(unsigned int n){ printf("%*.s", n, ""); }
static inline void ntabs(unsigned int n){ nspaces(n*4); }

const char* ast_expr_unop_string(ast_expr_unop_enum_t op){
	switch (op){
		case AST_EXPR_UNOP_BNOT: return "~";
		case AST_EXPR_UNOP_LNOT: return "!";
		case AST_EXPR_UNOP_NEG: return "-";
		case AST_EXPR_UNOP_DEREF: return "*";
	}
	printf("INTERNAL ERROR\n");
	exit(1);
}

const char* ast_expr_binop_string(ast_expr_binop_enum_t op){
	switch (op){
		case AST_EXPR_BINOP_ADD: return "+";
		case AST_EXPR_BINOP_SUB: return "-";
		case AST_EXPR_BINOP_MUL: return "*";
		case AST_EXPR_BINOP_DIV: return "/";
		case AST_EXPR_BINOP_MOD: return "%";
		case AST_EXPR_BINOP_LT: return "<";
		case AST_EXPR_BINOP_GT: return ">";
		case AST_EXPR_BINOP_LE: return "<=";
		case AST_EXPR_BINOP_GE: return ">=";
		case AST_EXPR_BINOP_EQ: return "==";
		case AST_EXPR_BINOP_NE: return "!=";
		case AST_EXPR_BINOP_BXOR: return "^";
		case AST_EXPR_BINOP_BAND: return "&";
		case AST_EXPR_BINOP_BOR: return "|";
		case AST_EXPR_BINOP_LAND: return "&&";
		case AST_EXPR_BINOP_LOR: return "||";
		case AST_EXPR_BINOP_SHR: return ">>";
		case AST_EXPR_BINOP_SHL: return "<<";
	}
	printf("INTERNAL ERROR\n");
	exit(1);
}

void print_ast_lvalue(ast_lvalue_t lvalue){
	printf("%s", lvalue.base_var->name);
	ast_datatype_t* type = lvalue.base_var->type_ref;
	for (unsigned int i=0; i < lvalue.member_access.len; i++){
		ast_lvalue_member_access_t member_access = lvalue.member_access.data[i];
		if (member_access.deref){
			printf("->");
			type = type->pointer.base;
		}else{
			printf(".");
		}
		printf("%s", type->structure.members.data[member_access.member_idx].name);
		type = type->structure.members.data[member_access.member_idx].type_ref;
	}
}

void print_ast_id(const ast_id_t* id, int depth){
	switch (id->type){
		case AST_ID_TYPE:
			ntabs(depth); printf("type '%s':\n", id->type_.name);
			switch (id->type_.kind){
				case AST_DATATYPE_VOID:
					ntabs(depth+1); printf("void\n");
					break;
				case AST_DATATYPE_INTEGRAL:
					ntabs(depth+1); printf("integeral\n");
					ntabs(depth+1); printf("signed: %s\n", id->type_.integral.signed_ ? "signed" : "unsigned");
					ntabs(depth+1); printf("bitwidth: %u\n", id->type_.integral.bitwidth);
					break;
				case AST_DATATYPE_FLOAT:
					ntabs(depth+1); printf("floating point\n");
					ntabs(depth+1); printf("bitwidth: %u\n", id->type_.floating.bitwidth);
					break;
				case AST_DATATYPE_POINTER:
					ntabs(depth+1); printf("pointer to type '%s'\n", id->type_.pointer.base->name);
					break;
				case AST_DATATYPE_STRUCTURED:
					ntabs(depth+1); printf("struct {\n");
					for (unsigned int i=0; i < id->type_.structure.members.len; i++){
						ntabs(depth+2); printf("%s %s;\n", id->type_.structure.members.data[i].type_ref->name, id->type_.structure.members.data[i].name);
					}
					ntabs(depth+1); printf("}\n");
					break;
			}
			break;
		case AST_ID_VAR:
			ntabs(depth); printf("variable '%s': type: '%s'\n", id->var.name, id->var.type_ref->name);
			break;
		case AST_ID_FUNC:
			ntabs(depth); printf("function '%s':\n", id->func.name);
			ntabs(depth+1); printf("return type: '%s'\n", id->func.return_type_ref->name);
			ntabs(depth+1); printf("arguments:\n");
			for (unsigned int i=0; i<id->func.args.len; i++){
				ntabs(depth+2); printf("'%s' of type '%s'\n", id->func.args.data[i]->name, id->func.args.data[i]->type_ref->name);
			}
			ntabs(depth+1); printf("scope:\n");
			print_ast_scope(id->func.local_scope, depth+2);
			ntabs(depth+1); printf("body:\n");
			print_ast_stmt(id->func.body, depth+2);
			break;
		case AST_ID_GLOBAL:
			ntabs(depth); printf("global '%s': type: '%s'\n", id->global.var.name, id->global.var.type_ref->name); // TODO: print initial value
			break;
	}
}

void print_ast_expr(const ast_expr_t* exp){
	switch (exp->type){
		case AST_EXPR_INT_CONST:
		{
			char* str = bignum_to_string(exp->int_constant);
			printf("%s", str);
			free(str);
			return;
		}
		case AST_EXPR_DOUBLE_CONST:
			printf("%E", exp->double_constant);
			return;
		case AST_EXPR_LVALUE:
			print_ast_lvalue(exp->lvalue);
			return;

		case AST_EXPR_FUNC_CALL:
			printf("%s(", exp->func_call.func_ref->name);
			for (unsigned int i=0; i < exp->func_call.arglist.len; i++){
				print_ast_expr(&exp->func_call.arglist.data[i]);
				if (i+1 < exp->func_call.arglist.len){
					printf(", ");
				}
			}
			printf(")");
			return;
			
		case AST_EXPR_UNOP:
			printf("%s", ast_expr_unop_string(exp->unop.op));
			print_ast_expr(exp->unop.operand);
			return;
			
		case AST_EXPR_BINOP:
			printf("(");
			print_ast_expr(exp->binop.left);
			printf("%s", ast_expr_binop_string(exp->binop.op));
			print_ast_expr(exp->binop.right);
			printf(")");
			return;

		case AST_EXPR_REF:
			printf("*");
			print_ast_lvalue(exp->ref.lvalue);
			return;

		case AST_EXPR_CAST:
			printf("(%s)(", exp->cast.type_ref->name);
			print_ast_expr(exp->cast.expr);
			printf(")");
			return;
	}
}

void print_ast_stmt(const ast_stmt_t* stmt, int depth){
	switch (stmt->type){
		case AST_STMT_IF:
			ntabs(depth); printf("if ");
			print_ast_expr(&stmt->if_.cond);
			printf(" {\n");
			print_ast_stmt(stmt->if_.iftrue, depth+1);
			ntabs(depth); printf("}\n");
			break;
		case AST_STMT_IF_ELSE:
			ntabs(depth); printf("if ");
			print_ast_expr(&stmt->if_else.cond);
			printf(" {\n");
			print_ast_stmt(stmt->if_else.iftrue, depth+1);
			ntabs(depth); printf("} else {\n");
			print_ast_stmt(stmt->if_else.iffalse, depth+1);
			ntabs(depth); printf("}\n");
			break;
		case AST_STMT_EXPR:
			ntabs(depth); print_ast_expr(&stmt->expr);
			printf(";\n");
			break;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				print_ast_stmt(&stmt->block.stmtlist.data[i], depth);
			}
			break;
		case AST_STMT_ASSIGN:
			ntabs(depth);
			print_ast_lvalue(stmt->assign.lvalue); printf(" = ");
			print_ast_expr(&stmt->assign.val);
			printf(";\n");
			break;
		case AST_STMT_BREAK:
			ntabs(depth); printf("break;\n");
			break;
		case AST_STMT_CONTINUE:
			ntabs(depth); printf("continue;\n");
			break;
		case AST_STMT_RETURN:
			ntabs(depth); printf("return ");
			print_ast_expr(stmt->return_.val);
			printf(";\n");
			break;
		case AST_STMT_WHILE:
			ntabs(depth); printf("while ");
			print_ast_expr(&stmt->while_.cond);
			printf(" {\n");
			print_ast_stmt(stmt->while_.body, depth+1);
			ntabs(depth); printf("}\n");
			break;
		case AST_STMT_FOR: // TODO: print scope?
			ntabs(depth); printf("for (\n");
			print_ast_stmt(stmt->for_.init, depth+1);
			ntabs(depth); print_ast_expr(&stmt->while_.cond);
			printf(";\n");
			print_ast_stmt(stmt->for_.step, depth+1);
			ntabs(depth); printf(") {\n");
			print_ast_stmt(stmt->for_.body, depth+1);
			ntabs(depth); printf("}\n");
			break;
	}
}

void print_ast_scope(const scope_t* scope, int depth){
	for (scope_iterator_t iter = scope_iter(scope); iter.current; iter = scope_iter_next(iter)){
		print_ast_id(iter.current->value, depth);
	}
}

void print_ast(const ast_t ast){
	print_ast_scope(ast.global_scope, 0);
}
