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

%define parse.error detailed

%%

main: exp

%nterm <expr> exp exp0 exp1;
exp0:
	TK_INT			{ $$ = $1; }
|	TK_IDENTIFIER	{ $$ = create_ast_expr_variable($1); }
|	'(' exp ')'		{ $$ = $2; }

exp1:
	exp1[l] '+' exp0[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_ADD, $l, $r); }
|	exp1[l] '-' exp0[r]	{ $$ = create_ast_expr_op(AST_EXPR_OP_SUB, $l, $r); }
|	exp0

exp: exp1

%%

int main(){
	return yyparse();
}
