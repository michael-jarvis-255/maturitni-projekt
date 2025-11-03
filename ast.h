#ifndef _AST_H_
#define _AST_H_
#include "yyltype.h"
#include "list.h"

typedef struct ast_identifier_t {
	loc_t loc;
	const char* name;
} ast_identifier_t;

typedef struct ast_type_t {
	loc_t loc;
	unsigned int ptr_count; // 0 if type is not a ptr, 1 if it is `name*`, 2 if `name**` and so on
	const char* name;
} ast_type_t;

typedef enum {
	AST_EXPR_CONST,
	AST_EXPR_VARIABLE,
	AST_EXPR_FUNC_CALL,
	AST_EXPR_OP,
} ast_expr_enum_t;

typedef enum {
	// numeric ops
	AST_EXPR_OP_ADD,
	AST_EXPR_OP_SUB,
	AST_EXPR_OP_MUL,
	AST_EXPR_OP_DIV,
	AST_EXPR_OP_MOD,
	AST_EXPR_OP_NEG, // unary

	// comparisons
	AST_EXPR_OP_LT,
	AST_EXPR_OP_GT,
	AST_EXPR_OP_LE,
	AST_EXPR_OP_GE,
	AST_EXPR_OP_EQ,
	AST_EXPR_OP_NE,

	// bitwise
	AST_EXPR_OP_SHL,
	AST_EXPR_OP_SHR,
	AST_EXPR_OP_BXOR,
	AST_EXPR_OP_BAND,
	AST_EXPR_OP_BOR,
	AST_EXPR_OP_BNOT, // unary

	// logical
	AST_EXPR_OP_LAND,
	AST_EXPR_OP_LOR,
	AST_EXPR_OP_LNOT, // unary
} ast_expr_op_enum_t;

typedef struct ast_expr_t {
	ast_expr_enum_t type;
	loc_t loc;
	union {
		struct {
			unsigned long value;
		} constant;
		ast_identifier_t variable;
		struct {
			ast_expr_op_enum_t op;
			struct ast_expr_t* left;
			struct ast_expr_t* right;
		} op;
		struct {
			ast_identifier_t funcname;
			unsigned int cap;
			unsigned int len;
			struct ast_expr_t* arglist;
		} call;
	};
} ast_expr_t;

typedef enum {
	AST_STMT_IF,
	AST_STMT_IF_ELSE,
	AST_STMT_EXPR,
	AST_STMT_BLOCK,
	AST_STMT_DECLARE,
	AST_STMT_ASSIGN,
	AST_STMT_BREAK,
	AST_STMT_CONTINUE,
	AST_STMT_RETURN,
	AST_STMT_FOR,
	AST_STMT_WHILE,
} ast_stmt_enum_t;

struct ast_stmt_t;
typedef struct ast_stmt_t ast_stmt_t;
create_list_type_header(ast_stmt)

typedef struct ast_stmt_t {
	ast_stmt_enum_t type;
	loc_t loc;
	union {
		struct {
			struct ast_stmt_t* iftrue;
			ast_expr_t cond;
		} if_;
		struct {
			struct ast_stmt_t* iftrue;
			struct ast_stmt_t* iffalse;
			ast_expr_t cond;
		} if_else;
		ast_expr_t expr;
		struct {
			ast_stmt_list_t stmtlist;
		} block;
		struct {
			ast_type_t type;
			ast_identifier_t name;
			ast_expr_t* val;
		} declare;
		struct {
			ast_identifier_t name;
			ast_expr_t val;
		} assign;
		struct {
			ast_expr_t val;
		} return_;
		struct {
			ast_expr_t cond;
			struct ast_stmt_t* body;
		} while_;
		struct {
			ast_expr_t cond;
			struct ast_stmt_t* init;
			struct ast_stmt_t* step;
			struct ast_stmt_t* body;
		} for_;
	};
} ast_stmt_t;

typedef struct ast_argdef_t {
	ast_type_t type;
	ast_identifier_t name;
} ast_argdef_t;
create_list_type_header(ast_argdef);

typedef enum {
	AST_DECL_FUNCTION,
	AST_DECL_GLOBAL,
	AST_DECL_TYPE,
} ast_decl_enum_t;

typedef struct ast_decl_t {
	ast_decl_enum_t type;
	loc_t loc;
	union {
		struct {
			ast_type_t returntype;
			ast_identifier_t name;
			ast_argdef_list_t args;
			ast_stmt_t* body;
		} function;
		struct {
			ast_type_t type;
			ast_identifier_t name;
			ast_expr_t* init;
		} global;
		struct {
			ast_type_t type;
			ast_identifier_t name;
		} typedecl;
	};
} ast_decl_t;
create_list_type_header(ast_decl)

ast_expr_t create_ast_expr_const(loc_t loc, unsigned long value);
ast_expr_t create_ast_expr_variable(loc_t loc, ast_identifier_t id);
ast_expr_t create_ast_expr_op(loc_t loc, ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right);

void print_ast_expr(const ast_expr_t* exp);

ast_stmt_t create_ast_stmt_block(loc_t loc);
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_identifier_t name, ast_expr_t value);
ast_stmt_t create_ast_stmt_if(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue);
ast_stmt_t create_ast_stmt_if_else(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse);
ast_stmt_t create_ast_stmt_declare(loc_t loc, ast_type_t type, ast_identifier_t name);
ast_stmt_t create_ast_stmt_declare_assign(loc_t loc, ast_type_t type, ast_identifier_t name, ast_expr_t value);
ast_stmt_t create_ast_stmt_return(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_while(loc_t loc, ast_expr_t cond, ast_stmt_t body);
ast_stmt_t create_ast_stmt_for(loc_t loc, ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body);

void print_ast_stmt(const ast_stmt_t* stmt, int depth);

ast_decl_t create_ast_decl_function(loc_t loc, ast_type_t returntype, ast_identifier_t name, ast_argdef_list_t args, ast_stmt_t body);
ast_decl_t create_ast_decl_global(loc_t loc, ast_type_t type, ast_identifier_t name);
ast_decl_t create_ast_decl_global_assign(loc_t loc, ast_type_t type, ast_identifier_t name, ast_expr_t value);

void print_ast_decl_list(const ast_decl_list_t* decllist);
void print_ast_decl(const ast_decl_t* decl);

#endif
