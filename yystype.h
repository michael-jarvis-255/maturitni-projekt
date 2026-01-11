#include "ast.h"
typedef union YYSTYPE {
	char* str;
	ast_expr_t expr;
	ast_stmt_t stmt;
	ast_decl_t decl;
	ast_variable_list_t argdeflist;
	ast_name_t name;
	ast_datatype_t* type_ref;
	ast_variable_t* var_ref;
	ast_func_t* func_ref;
	context_t* context;
	ast_id_ptr_list_t id_ptr_list;
} YYSTYPE;
