#include "ast.h"
#include "message.h"
#include "parse/main.h"
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>

create_list_type_impl(ast_variable, true)
create_list_type_impl(ast_variable_ptr, false)
create_list_type_impl(ast_stmt, true)
create_list_type_impl(ast_expr, true)
create_list_type_impl(ast_lvalue_member_access, false)

bool received_error;

void free_ast_variable_v(ast_variable_t var){
	free(var.name);
}
void free_ast_variable(ast_variable_t* var){
	free_ast_variable_v(*var);
	free(var);
}

void free_ast_id_v(ast_id_t id){
	switch (id.type){
		case AST_ID_TYPE:
			free_ast_datatype(&id.type_);
			break;
		case AST_ID_VAR:
			free_ast_variable_v(id.var);
			break;
		case AST_ID_FUNC:
			free_ast_stmt(id.func.body);
			free_scope(id.func.local_scope);
			free((void*)id.func.name);
			free(id.func.args.data);
			break;
		case AST_ID_GLOBAL:
			free(id.global.var.name);
			if (id.global.init) free_ast_expr(id.global.init);
			break;
			
	}
}
void free_ast_id(ast_id_t* id){
	free_ast_id_v(*id);
	free(id);
}

ast_lvalue_t create_ast_lvalue(ast_variable_t* var){
	return (ast_lvalue_t){
		.base_var = var,
		.type = var->type_ref,
		.member_access = create_ast_lvalue_member_access_list()
	};
}

void ast_lvalue_extend(ast_lvalue_t* lvalue, bool deref, ast_name_t member_name){
	if (deref){
		if (lvalue->type->kind != AST_DATATYPE_POINTER){
			printf_error(lvalue->loc, "invalid type access with '->' (type '%s' is not a pointer)", lvalue->type->name);
			free(member_name.name);
			return; // TODO: return some kind of error value
		}
		lvalue->type = lvalue->type->pointer.base;
	}
	if (lvalue->type->kind != AST_DATATYPE_STRUCTURED){
		printf_error(lvalue->loc, "invalid type access with '.' (type '%s' is not a struct)", lvalue->type->name); // TODO: wrong when deref == true
		free(member_name.name);
		return; // TODO: return some kind of error value
	}
	for (unsigned int i=0; i < lvalue->type->structure.members.len; i++){
		const ast_variable_t* member = &lvalue->type->structure.members.data[i];
		if (strcmp(member->name, member_name.name) == 0){
			lvalue->type = member->type_ref;
			ast_lvalue_member_access_list_append(&lvalue->member_access, (ast_lvalue_member_access_t){.deref = deref, .member_idx = i});
			free(member_name.name);
			return;
		}
	}
	printf_error(lvalue->loc, "struct type '%s' does not contain member '%s'", lvalue->type->name, member_name.name);
	free(member_name.name);
	return; // TODO: return some kind of error value
}

void free_ast_lvalue_v(ast_lvalue_t lvalue){
	shallow_free_ast_lvalue_member_access_list(&lvalue.member_access);
}

ast_expr_t create_ast_expr_int_const(loc_t loc, bignum_t* value){
	return (ast_expr_t){
		.type = AST_EXPR_INT_CONST,
		.loc = loc,
		.int_constant = value
	};
}
ast_expr_t create_ast_expr_double_const(loc_t loc, double value){
	return (ast_expr_t){
		.type = AST_EXPR_DOUBLE_CONST,
		.loc = loc,
		.double_constant = value
	};
}
ast_expr_t create_ast_expr_lvalue(loc_t loc, ast_lvalue_t lvalue){
	return (ast_expr_t){
		.type = AST_EXPR_LVALUE,
		.loc = loc,
		.lvalue = lvalue
	};
}
ast_expr_t create_ast_expr_func_call(loc_t loc, ast_func_t* func_ref){
	return (ast_expr_t){
		.type = AST_EXPR_FUNC_CALL,
		.loc = loc,
		.func_call.func_ref = func_ref,
		.func_call.arglist = create_ast_expr_list()
	};
}
ast_expr_t create_ast_expr_binop(loc_t loc, ast_expr_binop_enum_t op, ast_expr_t left, ast_expr_t right){
	return (ast_expr_t){
		.type = AST_EXPR_BINOP,
		.loc = loc,
		.binop.op = op,
		.binop.left = convert_to_ptr(left),
		.binop.right = convert_to_ptr(right)
	};
}
ast_expr_t create_ast_expr_unop(loc_t loc, ast_expr_unop_enum_t op, ast_expr_t opernad){
	return (ast_expr_t){
		.type = AST_EXPR_UNOP,
		.loc = loc,
		.unop.op = op,
		.unop.operand = convert_to_ptr(opernad),
	};
}

ast_expr_t create_ast_expr_ref(loc_t loc, ast_lvalue_t lvalue){
	return (ast_expr_t){
		.type = AST_EXPR_REF,
		.loc = loc,
		.ref.lvalue = lvalue
	};
}

ast_expr_t create_ast_expr_cast(loc_t loc, ast_expr_t expr, const ast_datatype_t* type){
	return (ast_expr_t){
		.type = AST_EXPR_CAST,
		.loc = loc,
		.cast.expr = convert_to_ptr(expr),
		.cast.type_ref = type
	};
}

void free_ast_expr(ast_expr_t* exp){
	free_ast_expr_v(*exp);
	free(exp);
}
void free_ast_expr_v(ast_expr_t exp){
	switch (exp.type){
		case AST_EXPR_INT_CONST:
			free_bignum(exp.int_constant);
			break;
		case AST_EXPR_DOUBLE_CONST:
			break;
		case AST_EXPR_REF:
			free_ast_lvalue_v(exp.ref.lvalue);
			break;
		case AST_EXPR_LVALUE:
			free_ast_lvalue_v(exp.lvalue);
			break;
		case AST_EXPR_FUNC_CALL:
			deep_free_ast_expr_list(&exp.func_call.arglist);
			break;
		case AST_EXPR_UNOP:
			free_ast_expr(exp.unop.operand);
			break;
		case AST_EXPR_BINOP:
			free_ast_expr(exp.binop.left);
			free_ast_expr(exp.binop.right);
			break;
		case AST_EXPR_CAST:
			free_ast_expr(exp.cast.expr);
			break;
	}
}

ast_stmt_t create_ast_stmt_block(loc_t loc, ast_stmt_list_t stmt_list, scope_t* local_scope){
	return (ast_stmt_t){
		.type = AST_STMT_BLOCK,
		.loc = loc,
		.block.stmtlist = stmt_list,
		.block.local_scope = local_scope
	};
}
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_EXPR,
		.loc = loc,
		.expr = expr
	};
}
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_lvalue_t lvalue, ast_expr_t value){
	return (ast_stmt_t){
		.type = AST_STMT_ASSIGN,
		.loc = loc,
		.assign.lvalue = lvalue,
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
ast_stmt_t create_ast_stmt_return(loc_t loc, ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_RETURN,
		.loc = loc,
		.return_.val = convert_to_ptr(expr)
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
ast_stmt_t create_ast_stmt_for(loc_t loc, ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body, scope_t* local_scope){
	return (ast_stmt_t){
		.type = AST_STMT_FOR,
		.loc = loc,
		.for_.local_scope = local_scope,
		.for_.cond = cond,
		.for_.init = convert_to_ptr(init),
		.for_.step = convert_to_ptr(step),
		.for_.body = convert_to_ptr(body)
	};
}

void free_ast_stmt(ast_stmt_t* stmt){
	free_ast_stmt_v(*stmt);
	free(stmt);
}
void free_ast_stmt_v(ast_stmt_t stmt){
	switch (stmt.type){
		case AST_STMT_IF:
			free_ast_stmt(stmt.if_.iftrue);
			free_ast_expr_v(stmt.if_.cond);
			break;
		case AST_STMT_IF_ELSE:
			free_ast_stmt(stmt.if_else.iffalse);
			free_ast_stmt(stmt.if_else.iftrue);
			free_ast_expr_v(stmt.if_else.cond);
			break;
		case AST_STMT_EXPR:
			free_ast_expr_v(stmt.expr);
			break;
		case AST_STMT_BLOCK:
			deep_free_ast_stmt_list(&stmt.block.stmtlist);
			if (stmt.block.local_scope)
				free_scope(stmt.block.local_scope);
			break;
		case AST_STMT_ASSIGN:
			free_ast_lvalue_v(stmt.assign.lvalue);
			free_ast_expr_v(stmt.assign.val);
			break;
		case AST_STMT_BREAK:
		case AST_STMT_CONTINUE:
			break;
		case AST_STMT_RETURN:
			free_ast_expr(stmt.return_.val);
			break;
		case AST_STMT_WHILE:
			free_ast_expr_v(stmt.while_.cond);
			free_ast_stmt(stmt.while_.body);
			break;
		case AST_STMT_FOR:
			free_scope(stmt.for_.local_scope);
			free_ast_expr_v(stmt.for_.cond);
			free_ast_stmt(stmt.for_.init);
			free_ast_stmt(stmt.for_.step);
			free_ast_stmt(stmt.for_.body);
			break;
	}
}

ast_datatype_t ast_datatype_duplicate(const ast_datatype_t* original, loc_t declare_loc, char* new_name){
	ast_datatype_t type = (ast_datatype_t){
		.declare_loc = declare_loc,
		.name = new_name,
		.kind = original->kind,
		.ptr_type = 0
	};
	switch (original->kind){
		case AST_DATATYPE_VOID:
			break;
		case AST_DATATYPE_INTEGRAL:
			type.integral.bitwidth = original->integral.bitwidth;
			type.integral.signed_ = original->integral.signed_;
			break;
		case AST_DATATYPE_FLOAT:
			type.floating.bitwidth = original->floating.bitwidth;
			break;
		case AST_DATATYPE_STRUCTURED:
		{
			ast_variable_list_t members = create_ast_variable_list();
			for (unsigned int i=0; i < original->structure.members.len; i++){
				ast_variable_t* member = &original->structure.members.data[i];
				ast_variable_list_append(&members, (ast_variable_t){
					.declare_loc = member->declare_loc,
					.name = strdup(member->name),
					.type_ref = member->type_ref
				});
			}
			type.structure.members = members;
			break;
		}
		case AST_DATATYPE_POINTER:
			type.pointer.base = original->pointer.base;
			break;
	}
	return type;
}

bool ast_datatype_eq(const ast_datatype_t* a, const ast_datatype_t* b){
	if (a->kind != b->kind) return false;
	switch (a->kind){
		case AST_DATATYPE_VOID: return true;
		case AST_DATATYPE_INTEGRAL:
			if (a->integral.bitwidth != b->integral.bitwidth) return false;
			if (a->integral.signed_ != b->integral.signed_) return false;
			break;
		case AST_DATATYPE_FLOAT:
			if (a->floating.bitwidth != b->floating.bitwidth) return false;
			break;
		case AST_DATATYPE_STRUCTURED:
			return a == b; // TODO: compare members?
		case AST_DATATYPE_POINTER:
			return ast_datatype_eq(a->pointer.base, b->pointer.base);
	}
	return true;
}

ast_datatype_t* get_ast_pointer_type(ast_datatype_t* base){
	if (base->ptr_type) return base->ptr_type;

	char* name = malloc(strlen(base->name)+2);
	strcpy(name, base->name);
	strcat(name, "*");
	ast_datatype_t ptr = (ast_datatype_t){
		.declare_loc = base->declare_loc,
		.name = name,
		.kind = AST_DATATYPE_POINTER,
		.pointer.base = base,
		.ptr_type = 0
	};
	return base->ptr_type = convert_to_ptr(ptr);
}

void free_ast_datatype(ast_datatype_t* type){
	free(type->name);
	type->name = 0;
	switch (type->kind){
		case AST_DATATYPE_INTEGRAL:
		case AST_DATATYPE_FLOAT:
		case AST_DATATYPE_POINTER:
		case AST_DATATYPE_VOID:
			break;
		case AST_DATATYPE_STRUCTURED:
			for (unsigned int i = 0; i < type->structure.members.len; i++){
				free(type->structure.members.data[i].name);
			}
			shallow_free_ast_variable_list(&type->structure.members);
			break;
	}
	if (type->ptr_type){
		free_ast_datatype(type->ptr_type);
		free(type->ptr_type);
	}
}

void free_ast_v(ast_t ast){
	free_scope(ast.global_scope);
}
