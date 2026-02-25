#include "main.h"
#include "../message.h"
#include "../yystype.h"
#include "../build/parse.tab.h"
#include <string.h>

void yy_scan_string(const char *str); // TODO: include from header?

static void ast_scope_insert_type(scope_t* scope, const char* name, ast_datatype_t type){ // TODO: move elsewhere
	ast_id_t* id = malloc(sizeof(ast_id_t));
	id->type = AST_ID_TYPE;
	id->type_ = type;
	id->type_.name = strdup(name);
	scope_insert(scope, name, id);
}

ast_t parse_file(FILE* source_file, const char* source_path){ // TODO: support for stdin
	if (fseek(source_file, 0, SEEK_END)){
		printf("Failed to seek file.\n"); // TODO: better error message
		exit(1);
	}
	size_t sz = ftell(source_file);
	char* source = malloc(sz+1);

	if (fseek(source_file, 0, SEEK_SET)){
		printf("Failed to seek file.\n"); // TODO: better error message
		exit(1);
	}

	size_t bytes_read = 0;
	while (bytes_read < sz){
		size_t res = fread(source + bytes_read, 1, sz - bytes_read, source_file);
		if (res == 0){
			printf("Failed to read file.\n"); // TODO: better error message
			exit(1);
		}
		bytes_read += res;
	}
	source[sz] = 0;

	ast_t ast = parse_string(source, source_path);
	free(source);
	return ast;
}

ast_t parse_string(const char* source, const char* source_path){
	source_lines_t source_lines = create_source_lines(source);
	scope_t* global_scope = create_scope(0);
	scope_t* current_scope = global_scope;

	// fill global scope with built-in types
	ast_scope_insert_type(global_scope, "i64", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=64, .integral.signed_=true,  .ptr_type=0});
	ast_scope_insert_type(global_scope, "i32", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=32, .integral.signed_=true,  .ptr_type=0});
	ast_scope_insert_type(global_scope, "i16", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=16, .integral.signed_=true,  .ptr_type=0});
	ast_scope_insert_type(global_scope, "i8",  (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=8,  .integral.signed_=true,  .ptr_type=0});
	ast_scope_insert_type(global_scope, "u64", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=64, .integral.signed_=false, .ptr_type=0});
	ast_scope_insert_type(global_scope, "u32", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=32, .integral.signed_=false, .ptr_type=0});
	ast_scope_insert_type(global_scope, "u16", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=16, .integral.signed_=false, .ptr_type=0});
	ast_scope_insert_type(global_scope, "u8",  (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=8,  .integral.signed_=false, .ptr_type=0});
	ast_scope_insert_type(global_scope, "bool", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_INTEGRAL, .integral.bitwidth=1, .integral.signed_=false, .ptr_type=0});
	ast_scope_insert_type(global_scope, "void", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_VOID, .ptr_type=0 });
	ast_scope_insert_type(global_scope, "f64", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_FLOAT, .floating.bitwidth=64, .ptr_type=0 });
	ast_scope_insert_type(global_scope, "f32", (ast_datatype_t){ .declare_loc=(loc_t){0}, .kind=AST_DATATYPE_FLOAT, .floating.bitwidth=32, .ptr_type=0 });

	yy_scan_string(source); // TODO: use scan_bytes instead? or yy_scan_buffer?
	global_source_lines = source_lines;
	yyparse(&current_scope);
	return (ast_t){ .source_path=strdup(source_path), .source_lines=source_lines, .global_scope=global_scope };
}

void push_new_scope(scope_t** current_scope){
	scope_t* scope = create_scope(*current_scope);
	*current_scope = scope;
}

scope_t* pop_scope(scope_t** current_scope){
	scope_t* scope = *current_scope;
	if (scope == 0){
		printf("internal error\n"); // TODO: better internal error messages
		exit(1);
	}
	*current_scope = scope_get_parent(scope);
	return scope;
}


void parse_function_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* returntype, const char* name, ast_variable_ptr_list_t args, scope_t* local_scope, ast_stmt_t body){
	ast_id_t* func_id = malloc(sizeof(ast_id_t));
	func_id->type = AST_ID_FUNC;
	func_id->func.declare_loc = loc;
	func_id->func.return_type_ref = returntype;
	func_id->func.name = name;
	func_id->func.args = args;
	func_id->func.local_scope = local_scope;
	func_id->func.body = convert_to_ptr(body);
	scope_insert(current_scope, name, func_id);
}

void parse_typedef_decl(loc_t loc, scope_t* current_scope, const ast_datatype_t* type, ast_name_t name){
	ast_id_t* type_id = malloc(sizeof(ast_id_t));
	type_id->type = AST_ID_TYPE;
	type_id->type_ = ast_datatype_duplicate(type, loc, name.name);
	scope_insert(current_scope, name.name, type_id);
}

ast_id_t* parse_variable_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name){
	if (type->kind == AST_DATATYPE_VOID){
		printf_error(loc, "cannot declare variable '%s' with void type ('%s')", name.name, type->name);
		return 0;
	}
	ast_id_t* var_id = malloc(sizeof(ast_id_t));
	var_id->type = AST_ID_VAR;
	var_id->var.type_ref = type;
	var_id->var.declare_loc = loc;
	var_id->var.name = name.name;
	scope_insert(current_scope, name.name, var_id);
	return var_id;
}

ast_stmt_t parse_variable_assign_decl(loc_t loc, loc_t name_loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name, ast_expr_t value){
	ast_id_t* var_id = parse_variable_decl(loc, current_scope, type, name);
	if (!var_id) return (ast_stmt_t){0}; // TODO: is this safe?
	return create_ast_stmt_assign(loc, create_ast_lvalue(&var_id->var, name_loc), value);
}

void parse_global_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name){
	if (type->kind == AST_DATATYPE_VOID){
		printf_error(loc, "cannot declare global variable '%s' with void type ('%s')", name.name, type->name);
		return;
	}
	ast_id_t* var_id = malloc(sizeof(ast_id_t));
	var_id->type = AST_ID_GLOBAL;
	var_id->global.var.type_ref = type;
	var_id->global.var.declare_loc = loc;
	var_id->global.var.name = name.name;
	var_id->global.init = 0;
	scope_insert(current_scope, name.name, var_id);
}

void parse_global_assign_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name, ast_expr_t value){
	parse_global_decl(loc, current_scope, type, name);
}

ast_datatype_t* parse_anonymous_struct(loc_t loc, scope_t* current_scope, ast_variable_list_t members){
	ast_id_t* id = malloc(sizeof(ast_id_t));
	id->type = AST_ID_TYPE;
	id->type_ = (ast_datatype_t){ .declare_loc=loc, .name=strdup("<anonymous struct>"), .kind=AST_DATATYPE_STRUCTURED, .structure.members=members, .ptr_type=0};

	// we are being cheeky and inserting even when "<anonymous struct>" might already be in the scope
	// we are placing it into the scope so that it can be free()'d
	// TODO: find a more elegant solution
	scope_force_insert(current_scope, "<anonymous struct>", id);
	return &id->type_;
}
