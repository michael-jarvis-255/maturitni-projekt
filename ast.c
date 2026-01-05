#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "hash.h"

static void free_context_ptr_v(context_ptr_t map){
	free_context_v(map);
}
create_list_type_impl(ast_variable, false)
create_list_type_impl(ast_stmt, true)
create_list_type_impl(context_ptr, true)
create_list_type_impl(ast_expr, true)
create_hashmap_type_impl(str, ast_id_t*, context)

context_t top_level_context;
static context_stack_t context_stack;
static char** source_lines;

void free_ast_id_v(ast_id_t id){
	switch (id.type){
		case AST_ID_TYPE:
			free((void*)id.type_.name);
			break;
		case AST_ID_VAR:
			free((void*)id.var.name);
			break;
		case AST_ID_FUNC:
			free_ast_stmt(id.func.body);
			free_context(id.func.context);
			free((void*)id.func.name);
			free(id.func.args.data);
			break;
	}
}
void free_ast_id(ast_id_t* id){
	free_ast_id_v(*id);
	free(id);
}
static inline void nspaces(unsigned int n){ printf("%*.s", n, ""); }
static inline void ntabs(unsigned int n){ nspaces(n*4); }
void print_ast_id(const ast_id_t* id, int depth){
	switch (id->type){
		case AST_ID_TYPE:
			ntabs(depth); printf("type '%s", id->type_.name);
			for (unsigned int i=0; i<id->type_.ptr_count; i++)
				printf("*");
			printf("':\n");
			ntabs(depth+1); printf("signed: %s\n", id->type_.signed_ ? "signed" : "unsigned");
			ntabs(depth+1); printf("bitwidth: %u\n", id->type_.bitwidth);
			break;
		case AST_ID_VAR:
			ntabs(depth); printf("variable '%s':\n", id->var.name);
			ntabs(depth+1); printf("type: '%s'\n", id->var.type_ref->name);
			break;
		case AST_ID_FUNC:
			ntabs(depth); printf("function '%s':\n", id->func.name);
			ntabs(depth+1); printf("return type: '%s'\n", id->func.return_type_ref->name);
			// TODO: arguments ?
			ntabs(depth+1); printf("context:\n");
			print_ast_context(id->func.context, depth+2);
			ntabs(depth+1); printf("body:\n");
			print_ast_stmt(id->func.body, depth+2);
			break;
	}
}

static const char* ast_expr_unop_string(ast_expr_unop_enum_t op){
	switch (op){
		case AST_EXPR_UNOP_BNOT: return "~";
		case AST_EXPR_UNOP_LNOT: return "!";
		case AST_EXPR_UNOP_NEG: return "-";
	}
	printf("INTERNAL ERROR\n");
	exit(1);
}
static const char* ast_expr_binop_string(ast_expr_binop_enum_t op){
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

ast_expr_t create_ast_expr_const(loc_t loc, unsigned long value){
	return (ast_expr_t){
		.type = AST_EXPR_CONST,
		.loc = loc,
		.constant.value = value
	};
}
ast_expr_t create_ast_expr_var_ref(loc_t loc, ast_variable_t* var_ref){
	return (ast_expr_t){
		.type = AST_EXPR_VAR_REF,
		.loc = loc,
		.var_ref = var_ref
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


void free_ast_expr(ast_expr_t* exp){
	free_ast_expr_v(*exp);
	free(exp);
}
void free_ast_expr_v(ast_expr_t exp){
	switch (exp.type){
		case AST_EXPR_CONST:
		case AST_EXPR_VAR_REF:
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
	}
}

void print_ast_expr(const ast_expr_t* exp){
	switch (exp->type){
		case AST_EXPR_CONST:
			printf("%lu", exp->constant.value);
			return;

		case AST_EXPR_VAR_REF:
			printf("%s", exp->var_ref->name);
			return;

		case AST_EXPR_FUNC_CALL:
			printf("TODO");
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
	}
}


ast_stmt_t create_ast_stmt_block(loc_t loc, bool with_context){
	context_t* ctxp = 0;
	if (with_context){
		context_t ctx = create_context();
		ctxp = convert_to_ptr(ctx);
	}
	return (ast_stmt_t){
		.type = AST_STMT_BLOCK,
		.loc = loc,
		.block.stmtlist = create_ast_stmt_list(),
		.block.context = ctxp
	};
}
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr){
	return (ast_stmt_t){
		.type = AST_STMT_EXPR,
		.loc = loc,
		.expr = expr
	};
}
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_variable_t* var_ref, ast_expr_t value){
	return (ast_stmt_t){
		.type = AST_STMT_ASSIGN,
		.loc = loc,
		.assign.var_ref = var_ref,
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
			if (stmt.block.context)
				free_context(stmt.block.context);
			break;
		case AST_STMT_ASSIGN:
			free_ast_expr_v(stmt.assign.val);
			break;
		case AST_STMT_BREAK:
		case AST_STMT_CONTINUE:
			break;
		case AST_STMT_RETURN:
			free_ast_expr_v(stmt.return_.val);
			break;
		case AST_STMT_FOR:
			free_ast_expr_v(stmt.while_.cond);
			free_ast_stmt(stmt.while_.body);
			break;
		case AST_STMT_WHILE:
			free_ast_expr_v(stmt.for_.cond);
			free_ast_stmt(stmt.for_.init);
			free_ast_stmt(stmt.for_.step);
			free_ast_stmt(stmt.for_.body);
			break;
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
			printf(";");
			break;
		case AST_STMT_BLOCK:
			for (unsigned int i=0; i < stmt->block.stmtlist.len; i++){
				print_ast_stmt(&stmt->block.stmtlist.data[i], depth);
			}
			break;
		case AST_STMT_ASSIGN:
			ntabs(depth); printf("%s = ", stmt->assign.var_ref->name);
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
			print_ast_expr(&stmt->return_.val);
			printf(";\n");
			break;
		case AST_STMT_WHILE:
			ntabs(depth); printf("while ");
			print_ast_expr(&stmt->while_.cond);
			printf(" {\n");
			print_ast_stmt(stmt->while_.body, depth+1);
			ntabs(depth); printf("}\n");
			break;
		case AST_STMT_FOR:
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

ast_decl_t create_ast_decl_function(loc_t loc, ast_datatype_t* returntype_ref, const char* name, ast_variable_list_t args, context_t* context, ast_stmt_t body){
	ast_id_t* func_id = malloc(sizeof(ast_id_t));
	func_id->type = AST_ID_FUNC;
	func_id->func.declare_loc = loc;
	func_id->func.return_type_ref = returntype_ref;
	func_id->func.name = name;
	func_id->func.args = args;
	func_id->func.context = context;
	func_id->func.body = convert_to_ptr(body);
	current_context_insert(name, func_id);

	return (ast_decl_t){.type=AST_DECL_DUMMY};
}
ast_decl_t create_ast_decl_var(loc_t loc, ast_datatype_t* type, ast_name_t name){
	ast_id_t* var_id = malloc(sizeof(ast_id_t));
	var_id->type = AST_ID_VAR;
	var_id->var.type_ref = type;
	var_id->var.declare_loc = loc;
	var_id->var.name = name.name;
	current_context_insert(name.name, var_id);

	return (ast_decl_t){.type=AST_DECL_DUMMY};
}
ast_decl_t create_ast_decl_var_assign(loc_t loc, ast_datatype_t* type, ast_name_t name, ast_expr_t value){
	ast_id_t* var_id = malloc(sizeof(ast_id_t));
	var_id->type = AST_ID_VAR;
	var_id->var.type_ref = type;
	var_id->var.declare_loc = loc;
	var_id->var.name = name.name;
	current_context_insert(name.name, var_id);

	return (ast_decl_t){.type=AST_DECL_STMT, .stmt=create_ast_stmt_assign(loc, &var_id->var, value)};
}

static void ast_context_insert_type(const char* name, ast_datatype_t type){
	ast_id_t* id = malloc(sizeof(ast_id_t));
	id->type = AST_ID_TYPE;
	id->type_ = type;
	id->type_.name = strdup(name);
	current_context_insert(name, id);
}

static void rtrim(char* str){
	char* end = str + strlen(str) - 1;
	while ((end >= str) && isspace(*end)){
		end--;
	}
	if (end < str){
		str[0] = 0;
		return;
	}
	end[1] = 0;
}

static void get_source_lines_from_file(FILE* source){
	if (fseek(source, 0, SEEK_END)){
		printf("Failed to seek file.\n");
		exit(1);
	}
	unsigned long sz = ftell(source);
	char* text = malloc(sz);

	if (fseek(source, 0, SEEK_SET)){
		printf("Failed to seek file.\n");
		exit(1);
	}
	fread(text, 1, sz, source);

	unsigned int line_count = 2;
	for (unsigned int i=0; i<sz; i++){
		if (text[i] == '\n') line_count++;
	}
	source_lines = malloc(sizeof(char*)*line_count);

	unsigned int line_start = 0;
	unsigned int line_idx = 0;
	for (unsigned int i=0; i<sz; i++){
		if (text[i] != '\n') continue;
		source_lines[line_idx] = strndup(&text[line_start], i-line_start);
		rtrim(source_lines[line_idx]);
		line_start = i+1;
		line_idx++;
	}
	source_lines[line_idx] = strndup(&text[line_start], sz-line_start);
	rtrim(source_lines[line_idx]);
	source_lines[line_count-1] = 0;
	free(text);
}

void ast_init_context(FILE* source){
	get_source_lines_from_file(source);
	context_stack = create_context_ptr_list();
	top_level_context = create_context();
	context_ptr_list_append(&context_stack, &top_level_context);

	// integer types
	ast_context_insert_type("i64", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 64,
		.signed_ = true
	});
	ast_context_insert_type("u64", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 64,
		.signed_ = false
	});
	ast_context_insert_type("i32", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 32,
		.signed_ = true
	});
	ast_context_insert_type("u32", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 32,
		.signed_ = false
	});
	ast_context_insert_type("i16", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 16,
		.signed_ = true
	});
	ast_context_insert_type("u16", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 16,
		.signed_ = false
	});
	ast_context_insert_type("i8", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 8,
		.signed_ = true
	});
	ast_context_insert_type("u8", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_INTEGRAL,
		.ptr_count = 0,
		.bitwidth = 8,
		.signed_ = false
	});

	// floating point types
	ast_context_insert_type("f64", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_FLOAT,
		.ptr_count = 0,
		.bitwidth = 64,
	});
	ast_context_insert_type("f32", (ast_datatype_t){
		.declare_loc = (loc_t){0},
		.kind = AST_DATATYPE_FLOAT,
		.ptr_count = 0,
		.bitwidth = 32,
	});
}
void current_context_insert(const char* name, ast_id_t* value){
	context_t* ctx = context_stack.data[context_stack.len-1];
	bool exists = context_get(ctx, name, 0);
	if (exists){
		printf("ERROR: name '%s' is already defined!\n", name); // TODO: better error logging
		return;
	}
	context_insert(ctx, name, value);
}
ast_id_t* context_stack_get(const char* name){
	for (int i = context_stack.len-1; i>=0; i--){
		context_t* ctx = context_stack.data[i];
		ast_id_t* val = context_get(ctx, name, 0);
		if (val) return val;
	}
	return 0;
}
void context_stack_push(context_t* context){
	context_ptr_list_append(&context_stack, context);
}
void context_stack_pop(context_t* context){
	if (context_stack.data[context_stack.len-1] != context){
		printf("INTERNAL ERROR: attempting to end context which isn't the active level.\n");
		return;
	}
	context_ptr_list_pop(&context_stack);
}

void free_context_v(context_t* context){
	context_entry_t* entry = context_to_linked_list(context);
	while (entry){
		free_ast_id(entry->value);
		context_entry_t* next = entry->next;
		free(entry);
		entry = next;
	}
}
void free_context(context_t* context){
	free_context_v(context);
	free(context);
}
void ast_cleanup_context(){
	char** l = source_lines;
	while (*l){
		free(*l);
		l++;
	}
	free(source_lines);
	source_lines = 0;

	deep_free_context_ptr_list(&context_stack);
}

void print_ast_context(const context_t* ctx, int depth){
	for (context_iterator_t iter = context_iter(ctx); iter.current; iter = context_iter_next(iter)){
		print_ast_id(iter.current->value, depth);
	}
}

void print_ast(){
	print_ast_context(&top_level_context, 0);
}

#define COLOUR_INFO "\033[36m"
#define COLOUR_WARNING "\033[33m"
#define COLOUR_ERROR "\033[31m"
#define STYLE_BOLD "\033[1m"
#define STYLE_RESET "\033[0m"

static void print_msg(loc_t loc, const char* msg_type, const char* msg, const char* colour){
	printf("%sline %u: %s%s:%s %s\n", STYLE_BOLD, loc.first_line+1, colour, msg_type, STYLE_RESET, msg);
	unsigned int tilde_count = 0;
	if (loc.first_line == loc.last_line){
		printf("%5u |    %.*s%s%.*s%s%s\n", loc.first_line+1,
			loc.first_column, source_lines[loc.first_line],
			colour,
			loc.last_column - loc.first_column, source_lines[loc.first_line] + loc.first_column,
			STYLE_RESET,
			source_lines[loc.first_line] + loc.last_column);
		tilde_count = loc.last_column - loc.first_column - 1;
	}else{
		printf("%5u |    %.*s%s%s%s\n", loc.first_line+1,
			loc.first_column, source_lines[loc.first_line],
			colour,
			source_lines[loc.first_line] + loc.first_column,
			STYLE_RESET);
		tilde_count = strlen(source_lines[loc.first_line]) - loc.first_column - 1;
	}

	printf("      |    %*s%s^", loc.first_column, "", colour);
	for (unsigned int i=0; i<tilde_count; i++){
		printf("~");
	}
	printf("%s\n", STYLE_RESET);

	if (loc.first_line != loc.last_line){
		printf("      |    %s...%s\n", colour, STYLE_RESET);
	}
	//printf("%i:%i - %i:%i\n", loc.first_line, loc.first_column, loc.last_line, loc.last_column);
}

void print_info(loc_t loc, char* msg){
	print_msg(loc, "info", msg, COLOUR_INFO);
}
void print_warning(loc_t loc, char* msg){
	print_msg(loc, "warning", msg, COLOUR_WARNING);
}
void print_error(loc_t loc, char* msg){
	print_msg(loc, "error", msg, COLOUR_ERROR);
}
