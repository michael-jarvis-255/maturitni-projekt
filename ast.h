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

typedef struct ast_stmt_t {
	ast_stmt_enum_t type;
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
			unsigned int cap;
			unsigned int len;
			struct ast_stmt_t* stmtlist;
		} block;
		struct {
			const char* type;
			const char* name;
			ast_expr_t* val;
		} declare;
		struct {
			const char* name;
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


ast_expr_t create_ast_expr_const(unsigned long value);
ast_expr_t create_ast_expr_variable(const char* id);
ast_expr_t create_ast_expr_op(ast_expr_op_enum_t op, ast_expr_t left, ast_expr_t right);

void print_ast_expr(const ast_expr_t* exp);

ast_stmt_t create_ast_stmt_block();
ast_stmt_t create_ast_stmt_expr(ast_expr_t expr);
ast_stmt_t create_ast_stmt_assign(const char* name, ast_expr_t value);
ast_stmt_t create_ast_stmt_if(ast_expr_t cond, ast_stmt_t iftrue);
ast_stmt_t create_ast_stmt_if_else(ast_expr_t cond, ast_stmt_t iftrue, ast_stmt_t iffalse);
ast_stmt_t create_ast_stmt_declare(const char* type, const char* name);
ast_stmt_t create_ast_stmt_declare_assign(const char* type, const char* name, ast_expr_t value);
ast_stmt_t create_ast_stmt_return(ast_expr_t expr);
ast_stmt_t create_ast_stmt_while(ast_expr_t cond, ast_stmt_t body);
ast_stmt_t create_ast_stmt_for(ast_stmt_t init, ast_expr_t cond, ast_stmt_t step, ast_stmt_t body);

void ast_stmt_block_append(ast_stmt_t* block, ast_stmt_t stmt);
void print_ast_stmt(const ast_stmt_t* stmt, int depth);

#endif
