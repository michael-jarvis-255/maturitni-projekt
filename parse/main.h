#ifndef _PARSE_MAIN_H_
#define _PARSE_MAIN_H_
#include <stdio.h>
#include "../ast.h"
#include "../message.h"

ast_t parse_file(FILE* source);
ast_t parse_string(const char* source);

void push_new_scope(scope_t** current_scope);
scope_t* pop_scope(scope_t** current_scope);

// declarations
void parse_function_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* returntype, const char* name, ast_variable_ptr_list_t args, scope_t* scope, ast_stmt_t body);
void parse_typedef_decl(loc_t loc, scope_t* current_scope, const ast_datatype_t* type, ast_name_t name);
ast_id_t* parse_variable_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name);
ast_stmt_t parse_variable_assign_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name, ast_expr_t value);
void parse_global_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name);
void parse_global_assign_decl(loc_t loc, scope_t* current_scope, ast_datatype_t* type, ast_name_t name, ast_expr_t value);
ast_datatype_t* parse_anonymous_struct(loc_t loc, scope_t* current_scope, ast_variable_list_t members);

#endif
