#ifndef _AST_H_
#define _AST_H_
#include "yyltype.h"
#include "list.h"
#include "hashmap.h"
#include "bignum.h"
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
	AST_DATATYPE_POINTER,
	AST_DATATYPE_VOID,
} ast_datatype_enum_t;

struct ast_datatype_t;

typedef struct ast_variable_t {
	loc_t declare_loc;
	char* name;
	struct ast_datatype_t* type_ref;
} ast_variable_t;

create_list_type_header(ast_variable, false);

typedef struct ast_datatype_t {
	loc_t declare_loc;
	char* name;
	ast_datatype_enum_t kind;

	union {
		struct {
			unsigned int bitwidth;
			bool signed_;
		} integral;
		struct {
			unsigned int bitwidth;
		} floating;
		struct {
			struct ast_datatype_t* base;
		} pointer;
		struct {
			ast_variable_list_t members;
		} structure;
	};
	struct ast_datatype_t* ptr_type;
} ast_datatype_t;

typedef struct ast_lvalue_member_access_t {
	bool deref; // '.' if false, '->' if true
	unsigned int member_idx;
} ast_lvalue_member_access_t;

create_list_type_header(ast_lvalue_member_access, false);

typedef struct {
	loc_t loc; // TODO
	ast_variable_t* base_var;
	ast_datatype_t* type; // the type after all member accesses
	ast_lvalue_member_access_list_t member_access;
} ast_lvalue_t;

typedef enum {
	AST_EXPR_INT_CONST,
	AST_EXPR_DOUBLE_CONST,
	AST_EXPR_LVALUE,
	AST_EXPR_FUNC_CALL,
	AST_EXPR_UNOP,
	AST_EXPR_BINOP,
	AST_EXPR_REF,
	AST_EXPR_CAST,
} ast_expr_enum_t;

typedef enum {
	AST_EXPR_UNOP_NEG,
	AST_EXPR_UNOP_BNOT,
	AST_EXPR_UNOP_LNOT,
	AST_EXPR_UNOP_DEREF,
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
		bignum_t* int_constant;
		double double_constant;
		ast_lvalue_t lvalue;
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
			const struct ast_func_t* func_ref;
			ast_expr_list_t arglist;
		} func_call;
		struct {
			ast_lvalue_t lvalue;
		} ref;
		struct {
			struct ast_expr_t* expr;
			const ast_datatype_t* type_ref;
		} cast;
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
			ast_lvalue_t lvalue;
			ast_expr_t val;
		} assign;
		struct {
			ast_expr_t* val;
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

typedef struct ast_global_t {
	ast_variable_t var;
	ast_expr_t* init; // TODO: use this
} ast_global_t;

typedef enum {
	AST_ID_TYPE,
	AST_ID_VAR,
	AST_ID_FUNC,
	AST_ID_GLOBAL,
} ast_id_enum_t;

typedef struct ast_id_t {
	ast_id_enum_t type;
	union {
		ast_datatype_t type_;
		ast_variable_t var;
		ast_func_t func;
		ast_global_t global;
	};
} ast_id_t;

typedef context_t* context_ptr_t;
create_list_type_header(context_ptr, true);
typedef context_ptr_list_t context_stack_t;

void free_ast_id_v(ast_id_t id);
void free_ast_id(ast_id_t* id);

ast_datatype_t* create_ast_anon_struct_head(loc_t loc);
void ast_anon_struct_head_append(ast_datatype_t* strct, loc_t loc, ast_datatype_t* elem_type, ast_name_t elem_name);
ast_datatype_t* ast_anon_struct_finalise(ast_datatype_t* strct);

ast_lvalue_t create_ast_lvalue(ast_variable_t* var);
ast_lvalue_t ast_lvalue_extend(ast_lvalue_t lvalue, bool deref, ast_name_t member_name);

ast_expr_t create_ast_expr_int_const(loc_t loc, bignum_t* value);
ast_expr_t create_ast_expr_double_const(loc_t loc, double value);
ast_expr_t create_ast_expr_lvalue(loc_t loc, ast_lvalue_t lvalue);
ast_expr_t create_ast_expr_func_call(loc_t loc, ast_func_t* func_ref);
ast_expr_t create_ast_expr_binop(loc_t loc, ast_expr_binop_enum_t op, ast_expr_t left, ast_expr_t right);
ast_expr_t create_ast_expr_unop(loc_t loc, ast_expr_unop_enum_t op, ast_expr_t opernad);
ast_expr_t create_ast_expr_ref(loc_t loc, ast_lvalue_t lvalue);
ast_expr_t create_ast_expr_cast(loc_t loc, ast_expr_t expr, const ast_datatype_t* type);
void free_ast_expr_v(ast_expr_t exp);
void free_ast_expr(ast_expr_t* exp);
void print_ast_expr(const ast_expr_t* exp);

ast_stmt_t create_ast_stmt_block(loc_t loc, bool with_context);
ast_stmt_t create_ast_stmt_expr(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_assign(loc_t loc, ast_lvalue_t lvalue, ast_expr_t value);
ast_stmt_t create_ast_stmt_if(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue);
ast_stmt_t create_ast_stmt_if_else(loc_t loc, ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse);
ast_stmt_t create_ast_stmt_return(loc_t loc, ast_expr_t expr);
ast_stmt_t create_ast_stmt_while(loc_t loc, ast_expr_t cond, ast_stmt_t body);
ast_stmt_t create_ast_stmt_for(loc_t loc, ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body);
void free_ast_stmt_v(ast_stmt_t stmt);
void free_ast_stmt(ast_stmt_t* stmt);
void print_ast_stmt(const ast_stmt_t* stmt, int depth);

void ast_declare_function(loc_t loc, ast_datatype_t* returntype, const char* name, ast_id_ptr_list_t args, context_t* context, ast_stmt_t body);
void ast_declare_typedef(loc_t loc, const ast_datatype_t* type, ast_name_t name);
void ast_declare_variable(loc_t loc, ast_datatype_t* type, ast_name_t name);
ast_stmt_t ast_declare_variable_assign(loc_t loc, ast_datatype_t* type, ast_name_t name, ast_expr_t value);

void ast_declare_global(loc_t loc, ast_datatype_t* type, ast_name_t name);
void ast_declare_global_assign(loc_t loc, ast_datatype_t* type, ast_name_t name, ast_expr_t value);


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

extern bool received_error;
extern context_t top_level_context;

bool ast_datatype_eq(const ast_datatype_t* a, const ast_datatype_t* b);
ast_datatype_t* get_ast_pointer_type(ast_datatype_t* base);
void free_ast_datatype(ast_datatype_t* type);

#endif
