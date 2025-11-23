#include "ast.h"
typedef union YYSTYPE {
	char* str;
	ast_expr_t expr;
	ast_stmt_t stmt;
	ast_decl_t decl;
	ast_argdef_list_t argdeflist;
	ast_decl_list_t decllist;
	ast_name_t name;
	ast_datatype_t* type_ref;
	ast_variable_t* var_ref;
	ast_func_t* func_ref;
	hashmap_t* context;
} YYSTYPE;
