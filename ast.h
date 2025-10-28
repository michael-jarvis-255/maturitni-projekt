#ifndef _AST_H_
#define _AST_H_

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
	union {
		struct {
			unsigned long value;
		} constant;
		
		struct {
			const char* id;
		} variable;
		
		struct {
			ast_expr_op_enum_t op;
			struct ast_expr_t* left;
			struct ast_expr_t* right;
		} op;
		
		struct {
			const char* id;
			unsigned int cap;
			unsigned int len;
			struct ast_expr_t* arglist;
		} call;
	};
} ast_expr_t;


ast_expr_t create_ast_expr_const(unsigned long value);
ast_expr_t create_ast_expr_variable(const char* id);
ast_expr_t create_ast_expr_op(ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right);

void print_ast_expr(ast_expr_t* exp);

#endif
