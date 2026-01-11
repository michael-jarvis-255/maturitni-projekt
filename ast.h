#ifndef _AST_H_
#define _AST_H_
#include "yyltype.h"
#include "list.h"
#include "hashmap.h"
#include <stdbool.h>
#define convert_to_ptr(x) memcpy(malloc(sizeof(x)), &x, sizeof(x))

typedef const char* str;
struct ast_id_t;
typedef struct ast_id_t ast_id_t;
create_hashmap_type_header(str, ast_id_t*, context);

typedef struct ast_name_t {
	loc_t loc;
	char* name;
} ast_name_t;

typedef enum {
	AST_DATATYPE_INTEGRAL,
	AST_DATATYPE_FLOAT,
	AST_DATATYPE_STRUCTURED,
} ast_datatype_enum_t;

typedef struct ast_datatype_t {
	loc_t declare_loc;
	char* name;
	ast_datatype_enum_t kind;
	unsigned int ptr_count; // 0 if type is not a ptr, 1 if it is `name*`, 2 if `name**` and so on
	unsigned int bitwidth;
	bool signed_;
} ast_datatype_t;

typedef struct ast_variable_t {
	loc_t declare_loc;
	char* name;
	ast_datatype_t* type_ref;
} ast_variable_t;

typedef enum {
	AST_EXPR_CONST,
	AST_EXPR_VAR_REF,
	AST_EXPR_FUNC_CALL,
	AST_EXPR_UNOP,
	AST_EXPR_BINOP,
} ast_expr_enum_t;

typedef enum {
	AST_EXPR_UNOP_NEG,
	AST_EXPR_UNOP_BNOT,
	AST_EXPR_UNOP_LNOT,
} ast_expr_unop_enum_t;

typedef enum {
	// numeric ops
	AST_EXPR_BINOP_ADD,
	AST_EXPR_BINOP_SUB,
	AST_EXPR_BINOP_MUL,
	AST_EXPR_BINOP_DIV,
	AST_EXPR_BINOP_MOD,

	// comparisons
	AST_EXPR_BINOP_LT,
	AST_EXPR_BINOP_GT,
	AST_EXPR_BINOP_LE,
	AST_EXPR_BINOP_GE,
	AST_EXPR_BINOP_EQ,
	AST_EXPR_BINOP_NE,

	// bitwise
	AST_EXPR_BINOP_SHL,
	AST_EXPR_BINOP_SHR,
	AST_EXPR_BINOP_BXOR,
	AST_EXPR_BINOP_BAND,
	AST_EXPR_BINOP_BOR,

	// logical
	AST_EXPR_BINOP_LAND,
	AST_EXPR_BINOP_LOR,
} ast_expr_binop_enum_t;

struct ast_func_t;
typedef struct ast_expr_t ast_expr_t;
create_list_type_header(ast_expr, true);
typedef struct ast_expr_t {
	ast_expr_enum_t type;
	loc_t loc;
	union {
		struct {
			unsigned long value;
		} constant;
		ast_variable_t* var_ref;
		struct {
			ast_expr_unop_enum_t op;
			struct ast_expr_t* operand;
		} unop;
		struct {
			ast_expr_binop_enum_t op;
			struct ast_expr_t* left;
			struct ast_expr_t* right;
		} binop;
		struct {
			struct ast_func_t* func_ref;
			ast_expr_list_t arglist;
		} func_call;
	};
} ast_expr_t;

typedef enum {
	AST_STMT_IF,
	AST_STMT_IF_ELSE,
	AST_STMT_EXPR,
	AST_STMT_BLOCK,
	AST_STMT_ASSIGN,
	AST_STMT_BREAK,
	AST_STMT_CONTINUE,
	AST_STMT_RETURN,
	AST_STMT_FOR,
	AST_STMT_WHILE,
} ast_stmt_enum_t;

struct ast_stmt_t;
typedef struct ast_stmt_t ast_stmt_t;
create_list_type_header(ast_stmt, true);

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
			context_t* context;
		} block;
		struct {
			ast_variable_t* var_ref;
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

typedef enum {
	AST_DECL_DUMMY,
	AST_DECL_STMT,
} ast_decl_enum_t;

typedef struct ast_decl_t {
	ast_decl_enum_t type;
	union {
		ast_stmt_t stmt;
	};
} ast_decl_t;

create_list_type_header(ast_variable, false);
typedef ast_id_t* ast_id_ptr_t;
create_list_type_header(ast_id_ptr, false);

typedef struct ast_func_t {
	loc_t declare_loc;
	ast_datatype_t* return_type_ref;
	const char* name;
	ast_id_ptr_list_t args; // TODO: would be nicer to use ast_variable_ptr_list_t, maybe refactor argdeflist from yystype.h too?
	ast_stmt_t* body;
	context_t* context;
} ast_func_t;

typedef enum {
	AST_ID_TYPE,
	AST_ID_VAR,
	AST_ID_FUNC,
} ast_id_enum_t;

typedef struct ast_id_t {
	ast_id_enum_t type;
	union {
		ast_datatype_t type_;
		ast_variable_t var;
		ast_func_t func;
	};
} ast_id_t;

typedef context_t* context_ptr_t;
create_list_type_header(context_ptr, true);
typedef context_ptr_list_t context_stack_t;

void free_ast_id_v(ast_id_t id);
void free_ast_id(ast_id_t* id);

ast_expr_t create_ast_expr_const(loc_t loc, unsigned long value);
ast_expr_t create_ast_expr_var_ref(loc_t loc, ast_variable_t* var);
ast_expr_t create_ast_expr_func_call(loc_t loc, ast_func_t* func_ref);
ast_expr_t create_ast_expr_binop(loc_t loc, ast_expr_binop_enum_t op, ast_expr_t left, ast_expr_t right);
ast_expr_t create_ast_expr_unop(loc_t loc, ast_expr_unop_enum_t op, ast_expr_t opernad);
void free_ast_expr_v(ast_expr_t exp);
void free_ast_expr(ast_expr_t* exp);
void print_ast_expr(const ast_expr_t* exp);

ast_stmt_t create_ast_stmt_block(loc_t loc, bool with_context);
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_variable_t* name, ast_expr_t value);
ast_stmt_t create_ast_stmt_if(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue);
ast_stmt_t create_ast_stmt_if_else(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse);
ast_stmt_t create_ast_stmt_return(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_while(loc_t loc, ast_expr_t cond, ast_stmt_t body);
ast_stmt_t create_ast_stmt_for(loc_t loc, ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body);
void free_ast_stmt_v(ast_stmt_t stmt);
void free_ast_stmt(ast_stmt_t* stmt);
void print_ast_stmt(const ast_stmt_t* stmt, int depth);

ast_decl_t create_ast_decl_function(loc_t loc, ast_datatype_t* returntype, const char* name, ast_id_ptr_list_t args, context_t* context, ast_stmt_t body);
ast_decl_t create_ast_decl_var(loc_t loc, ast_datatype_t* type, ast_name_t name);
ast_decl_t create_ast_decl_var_assign(loc_t loc, ast_datatype_t* type, ast_name_t name, ast_expr_t value);

void ast_init_context(FILE* source);
void current_context_insert(const char* name, ast_id_t* value);
ast_id_t* context_stack_get(const char* name);
context_t create_context();
void context_stack_push(context_t* context);
void context_stack_pop(context_t* context);
void free_context_v(context_t* context);
void free_context(context_t* context);
void ast_cleanup_context();
void print_ast_context(const context_t* context, int depth);

void print_ast();

const char* ast_expr_binop_string(ast_expr_binop_enum_t op);
const char* ast_expr_unop_string(ast_expr_unop_enum_t op);

void print_info(loc_t loc, char* msg);
void print_warning(loc_t loc, char* msg);
void print_error(loc_t loc, char* msg);

void printf_info(loc_t loc, char* msg, ...) 	__attribute__ ((format (printf, 2, 3)));
void printf_warning(loc_t loc, char* msg, ...) 	__attribute__ ((format (printf, 2, 3)));
void printf_error(loc_t loc, char* msg, ...) 	__attribute__ ((format (printf, 2, 3)));

extern context_t top_level_context;

bool ast_datatype_eq(const ast_datatype_t* a, const ast_datatype_t* b);

#endif
