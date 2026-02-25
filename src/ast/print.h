#ifndef _AST_PRINT_H_
#define _AST_PRINT_H_
#include "../ast/main.h"

const char* ast_expr_binop_string(ast_expr_binop_enum_t);
const char* ast_expr_unop_string(ast_expr_unop_enum_t);

void print_ast_lvalue(ast_lvalue_t lvalue);
void print_ast_id(const ast_id_t* id, int depth);
void print_ast_expr(const ast_expr_t* exp);
void print_ast_stmt(const ast_stmt_t* stmt, int depth);
void print_ast_scope(const scope_t* scope, int depth);
void print_ast(const ast_t ast);

#endif
