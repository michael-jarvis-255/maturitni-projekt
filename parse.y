%{
	#include <stdio.h>
	#include <string.h>
	#include "yystype.h"
	#include "yyltype.h"
	#include "ast.h"
	void yyerror(char const* err){
		fprintf(stderr, "%s\n", err);
	}
	int yylex ();
%}

%token <expr> TK_INT
%token <str> TK_STRING
%token <str> TK_IDENTIFIER
%token TK_IF "if"
%token TK_ELSE "else"
%token TK_FOR "for"
%token TK_WHILE "while"
%token TK_BREAK "break"
%token TK_CONTINUE "continue"
%token TK_RETURN "return"

%token TK_LE "<="
%token TK_GE ">="
%token TK_EQ "=="
%token TK_NE "!="
%token TK_LAND "&&"
%token TK_LOR "||"
%token TK_SHR ">>"
%token TK_SHL "<<"

%define parse.error detailed

%%

main:
	declaration_list YYEOF	{ print_ast_decl_list(&$1); }

%nterm <decllist> declaration_list;
declaration_list:
	%empty	{ $$ = create_ast_decl_list(); }
|	declaration_list declaration	{ $$ = $1; ast_decl_list_append(&$$, $declaration); printf("%i.%i - %i.%i\n", @2.first_line, @2.first_column, @2.last_line, @2.last_column); }

%nterm <identifier> identifier;
identifier:
	TK_IDENTIFIER	{ $$ = (ast_identifier_t){ .loc=@$, .name=strdup($1)}; }

%nterm <type> type;
type:
	TK_IDENTIFIER	{ $$ = (ast_type_t){ .loc=@$, .name=strdup($1)}; }
//|	type '*'		{ $$ = $1; $$.ptr_count++; }

%nterm <expr> exp exp0 exp1 exp2 exp3 exp4 exp5 exp6 exp7 exp8 exp9;
exp0:
	TK_INT			{ $$ = $1; }
|	identifier	{ $$ = create_ast_expr_variable(@$, $1); }
|	'(' exp ')'		{ $$ = $2; }

exp1:
	exp1[l] '*' exp0[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_MUL, $l, $r); }
|	exp1[l] '/' exp0[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_DIV, $l, $r); }
|	exp1[l] '%' exp0[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_MOD, $l, $r); }
|	exp0

exp2:
	exp2[l] '+' exp1[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_ADD, $l, $r); }
|	exp2[l] '-' exp1[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_SUB, $l, $r); }
|	exp1

exp3:
	exp3[l] TK_SHL exp2[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_SHL, $l, $r); }
|	exp3[l] TK_SHR exp2[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_SHR, $l, $r); }
|	exp2

exp4:
	exp4[l]  '>'  exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_GT, $l, $r); }
|	exp4[l]  '<'  exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_LT, $l, $r); }
|	exp4[l] TK_GE exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_GE, $l, $r); }
|	exp4[l] TK_LE exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_LE, $l, $r); }
|	exp4[l] TK_NE exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_NE, $l, $r); }
|	exp4[l] TK_EQ exp3[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_EQ, $l, $r); }
|	exp3

exp5:
	exp5[l]  '&'  exp4[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_BAND, $l, $r); }
|	exp4

exp6:
	exp6[l]  '^'  exp5[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_BXOR, $l, $r); }
|	exp5

exp7:
	exp7[l]  '|'  exp6[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_BOR, $l, $r); }
|	exp6

exp8:
	exp8[l] TK_LAND exp7[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_LAND, $l, $r); }
|	exp7

exp9:
	exp9[l] TK_LOR exp8[r]	{ $$ = create_ast_expr_op(@$, AST_EXPR_OP_LOR, $l, $r); }
|	exp8

exp: exp9


%nterm <stmt> block;
block:
	block_head '}'

%nterm <stmt> block_head;
block_head:
	'{'					{ $$ = create_ast_stmt_block(@$); }
|	block_head stmt		{ $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); } // TODO: block needs to also update it's loc
|	block_head ';'

%nterm <stmt> stmt;
stmt:
	non_if_stmt
|	if_ending_stmt

%nterm <stmt> if_ending_stmt;
if_ending_stmt:
	if_stmt
|	if_ending_if_else_stmt
|	if_ending_while_stmt
|	if_ending_for_loop

%nterm <stmt> non_if_stmt;
non_if_stmt:
	basic_stmt ';'
|	non_if_while_stmt
|	non_if_if_else_stmt
|	non_if_for_loop
|	block

%nterm <stmt> basic_stmt;
basic_stmt:
	single_assign_stmt
|	single_declare_stmt
|	return_stmt
|	break_stmt
|	continue_stmt
|	exp				{ $$ = create_ast_stmt_expr(@$, $exp); }

%nterm <stmt> return_stmt;
return_stmt:
	TK_RETURN exp		{ $$ = create_ast_stmt_return(@$, $exp); }

%nterm <stmt> break_stmt;
break_stmt:
	TK_BREAK			{ $$ = (ast_stmt_t){.type = AST_STMT_BREAK, .loc=@$}; }

%nterm <stmt> continue_stmt;
continue_stmt:
	TK_CONTINUE			{ $$ = (ast_stmt_t){.type = AST_STMT_CONTINUE, .loc=@$}; }

%nterm <stmt> single_assign_stmt;
single_assign_stmt:
	identifier[name] '=' exp[val]	{ $$ = create_ast_stmt_assign(@$, $name, $val); }

%nterm <stmt> single_declare_stmt;
single_declare_stmt:
	type identifier[name]				{ $$ = create_ast_stmt_declare(@$, $type, $name); }
|	type identifier[name] '=' exp[val]	{ $$ = create_ast_stmt_declare_assign(@$, $type, $name, $val); }

// if-else constructs always bind as tightly as possible, so 'if (x) if (y) else z;' will parse as 'if (x) {if (y) else z;}'
%nterm <stmt> if_stmt;
if_stmt:
	TK_IF '(' exp[cond] ')' stmt[then]									{ $$ = create_ast_stmt_if(@$, $cond, $then);}
%nterm <stmt> non_if_if_else_stmt;
non_if_if_else_stmt:
	TK_IF '(' exp[cond] ')' non_if_stmt[then] TK_ELSE non_if_stmt[else]	{ $$ = create_ast_stmt_if_else(@$, $cond, $then, $else); }
%nterm <stmt> if_ending_if_else_stmt;
if_ending_if_else_stmt:
	TK_IF '(' exp[cond] ')' non_if_stmt[then] TK_ELSE if_ending_stmt[else]		{ $$ = create_ast_stmt_if_else(@$, $cond, $then, $else); }

%nterm <stmt> non_if_while_stmt;
non_if_while_stmt:
	TK_WHILE '(' exp[cond] ')' non_if_stmt[body]	{ $$ = create_ast_stmt_while(@$, $cond, $body); }

%nterm <stmt> if_ending_while_stmt;
if_ending_while_stmt:
	TK_WHILE '(' exp[cond] ')' if_ending_stmt[body]	{ $$ = create_ast_stmt_while(@$, $cond, $body); }

%nterm <stmt> non_if_for_loop;
non_if_for_loop:
	TK_FOR '(' basic_stmt[init] ';' exp[cond] ';' basic_stmt[step] ')' non_if_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body); }

%nterm <stmt> if_ending_for_loop;
if_ending_for_loop:
	TK_FOR '(' basic_stmt[init] ';' exp[cond] ';' basic_stmt[step] ')' if_ending_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body); }


%nterm <decl> declaration;
declaration:
	function_declaration
|	single_global_declaration

%nterm <decl> function_declaration;
function_declaration:
	type identifier[name] '(' function_args_definition[args] ')' block[body]	{ $$ = create_ast_decl_function(@$, $type, $name, $args, $body); }

%nterm <argdeflist> function_args_definition;
function_args_definition:
	%empty		{ $$ = create_ast_argdef_list(); }
|	type identifier[name]	{ $$ = create_ast_argdef_list(); ast_argdef_list_append(&$$, (ast_argdef_t){.type=$type, .name=$name}); }
|	function_args_definition[args] ',' type identifier[name]	{ $$ = $1; ast_argdef_list_append(&$$, (ast_argdef_t){.type=$type, .name=$name}); }

%nterm <decl> single_global_declaration;
single_global_declaration:
	type identifier[name]	';'				{ $$ = create_ast_decl_global(@$, $type, $name); }
|	type identifier[name] '=' exp[val] ';'{ $$ = create_ast_decl_global_assign(@$, $type, $name, $val); }



%%

int main(){
	return yyparse();
}
