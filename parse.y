%{
	#include <stdio.h>
	#include "ast.h"
	void yyerror(char const* err){
		fprintf(stderr, "%s\n", err);
	}
	int yylex ();

	typedef union YYSTYPE {
		char* str;
		ast_expr_t expr;
		ast_stmt_t stmt;
	} YYSTYPE;
%}

%token <expr> TK_INT
%token <str> TK_STRING
%token <str> TK_IDENTIFIER
%token TK_IF
%token TK_ELSE
%token TK_FOR
%token TK_WHILE
%token TK_BREAK
%token TK_CONTINUE

%token TK_LE
%token TK_GE
%token TK_EQ
%token TK_NE
%token TK_LAND
%token TK_LOR
%token TK_SHR
%token TK_SHL

%define parse.error detailed

%%

main: stmt			{ print_ast_stmt(&$1, 0); }

%nterm <expr> exp exp0 exp1 exp2 exp3 exp4 exp5 exp6 exp7 exp8 exp9;
exp0:
	TK_INT			{ $$ = $1; }
|	TK_IDENTIFIER	{ $$ = create_ast_expr_variable($1); }
|	'(' exp ')'		{ $$ = $2; }

exp1:
	exp1[l] '*' exp0[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_MUL, $l, $r); }
|	exp1[l] '/' exp0[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_DIV, $l, $r); }
|	exp1[l] '%' exp0[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_MOD, $l, $r); }
|	exp0

exp2:
	exp2[l] '+' exp1[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_ADD, $l, $r); }
|	exp2[l] '-' exp1[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_SUB, $l, $r); }
|	exp1

exp3:
	exp3[l] TK_SHL exp2[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_SHL, $l, $r); }
|	exp3[l] TK_SHR exp2[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_SHR, $l, $r); }
|	exp2

exp4:
	exp4[l]  '>'  exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_GT, $l, $r); }
|	exp4[l]  '<'  exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_LT, $l, $r); }
|	exp4[l] TK_GE exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_GE, $l, $r); }
|	exp4[l] TK_LE exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_LE, $l, $r); }
|	exp4[l] TK_NE exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_NE, $l, $r); }
|	exp4[l] TK_EQ exp3[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_EQ, $l, $r); }
|	exp3

exp5:
	exp5[l]  '&'  exp4[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_BAND, $l, $r); }
|	exp4

exp6:
	exp6[l]  '^'  exp5[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_BXOR, $l, $r); }
|	exp5

exp7:
	exp7[l]  '|'  exp6[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_BOR, $l, $r); }
|	exp6

exp8:
	exp8[l] TK_LAND exp7[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_LAND, $l, $r); }
|	exp7

exp9:
	exp9[l] TK_LOR exp8[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_LOR, $l, $r); }
|	exp8

exp: exp9


%nterm <stmt> block block_head stmt non_if_stmt single_assign_stmt single_declare_stmt if_stmt non_if_if_else_stmt if_else_stmt;
block:
	block_head '}'

block_head:
	'{'					{ $$ = create_ast_stmt_block(); }
|	block_head stmt		{ $$ = $1; ast_stmt_block_append(&$$, $stmt); }
|	block_head ';'

stmt:
	non_if_stmt
|	if_stmt
|	if_else_stmt

non_if_stmt:
	single_assign_stmt ';'
|	single_declare_stmt ';'
|	non_if_if_else_stmt
|	exp ';'				{ $$ = create_ast_stmt_expr($exp); }
|	block

single_assign_stmt:
	TK_IDENTIFIER[name] '=' exp[val]	{ $$ = create_ast_stmt_assign($name, $val); }

single_declare_stmt:
	TK_IDENTIFIER[type] TK_IDENTIFIER[name]					{ $$ = create_ast_stmt_declare($type, $name); }
|	TK_IDENTIFIER[type] TK_IDENTIFIER[name] '=' exp[val]	{ $$ = create_ast_stmt_declare_assign($type, $name, $val); }

// if-else constructs always bind as tightly as possible, so 'if (x) if (y) else z;' will parse as 'if (x) {if (y) else z;}'
if_stmt:
	TK_IF '(' exp[cond] ')' stmt[then]									{ $$ = create_ast_stmt_if($cond, $then);}
non_if_if_else_stmt:
	TK_IF '(' exp[cond] ')' non_if_stmt[then] TK_ELSE non_if_stmt[else]	{ $$ = create_ast_stmt_if_else($cond, $then, $else); }
if_else_stmt:
	TK_IF '(' exp[cond] ')' non_if_stmt[then] TK_ELSE if_stmt[else]		{ $$ = create_ast_stmt_if_else($cond, $then, $else); }
|	TK_IF '(' exp[cond] ')' non_if_stmt[then] TK_ELSE if_else_stmt[else]{ $$ = create_ast_stmt_if_else($cond, $then, $else); }


%%

int main(){
	return yyparse();
}
