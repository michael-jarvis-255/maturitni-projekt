#include "ast.h"
typedef union YYSTYPE {
	char* str;
	ast_expr_t expr;
	ast_stmt_t stmt;
	ast_decl_t decl;
	ast_argdef_list_t argdeflist;
	ast_decl_list_t decllist;
} YYSTYPE;
