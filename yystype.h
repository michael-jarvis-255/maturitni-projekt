#include "ast.h"
typedef union YYSTYPE {
	char* str;
	ast_expr_t expr;
	ast_stmt_t stmt;
	ast_name_t name;
	ast_datatype_t* type_ref;
	ast_variable_t* var_ref;
	ast_lvalue_t lvalue;
	ast_func_t* func_ref;
	char scope_start; // never used, only to surpress (expected) bison warnings
	scope_t* scope;
	ast_variable_list_t var_list;
	ast_variable_ptr_list_t var_ptr_list;
	ast_stmt_list_t stmt_list;
} YYSTYPE;
