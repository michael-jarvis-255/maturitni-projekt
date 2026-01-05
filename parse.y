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
%token <name> TK_NAME
%token <var_ref> TK_VAR
%token <type_ref> TK_TYPE
%token <func_ref> TK_FUNC
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
	declaration_list YYEOF

declaration_list:
	%empty
|	declaration_list declaration

%nterm <type_ref> type;
type:
	TK_TYPE
//|	type '*'		{ $$ = $1; $$.ptr_count++; }

%nterm <name> name;
name:
	TK_NAME
|	type		{ $$ = (ast_name_t){ .loc=@1, .name=strdup($1->name) }; }
|	TK_VAR		{ $$ = (ast_name_t){ .loc=@1, .name=strdup($1->name) }; }

%nterm <expr> exp exp0 exp1 exp2 exp3 exp4 exp5 exp6 exp7 exp8 exp9;
exp0: // TODO: unary oparations
	TK_INT		{ $$ = $1; }
|	TK_VAR		{ $$ = create_ast_expr_var_ref(@$, $1); }
|	function_call
|	'(' exp ')'	{ $$ = $2; }

exp1:
	exp1[l] '*' exp0[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_MUL, $l, $r); }
|	exp1[l] '/' exp0[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_DIV, $l, $r); }
|	exp1[l] '%' exp0[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_MOD, $l, $r); }
|	exp0

exp2:
	exp2[l] '+' exp1[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_ADD, $l, $r); }
|	exp2[l] '-' exp1[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_SUB, $l, $r); }
|	exp1

exp3:
	exp3[l] TK_SHL exp2[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_SHL, $l, $r); }
|	exp3[l] TK_SHR exp2[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_SHR, $l, $r); }
|	exp2

exp4:
	exp4[l]  '>'  exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_GT, $l, $r); }
|	exp4[l]  '<'  exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_LT, $l, $r); }
|	exp4[l] TK_GE exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_GE, $l, $r); }
|	exp4[l] TK_LE exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_LE, $l, $r); }
|	exp4[l] TK_NE exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_NE, $l, $r); }
|	exp4[l] TK_EQ exp3[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_EQ, $l, $r); }
|	exp3

exp5:
	exp5[l]  '&'  exp4[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_BAND, $l, $r); }
|	exp4

exp6:
	exp6[l]  '^'  exp5[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_BXOR, $l, $r); }
|	exp5

exp7:
	exp7[l]  '|'  exp6[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_BOR, $l, $r); }
|	exp6

exp8:
	exp8[l] TK_LAND exp7[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_LAND, $l, $r); }
|	exp7

exp9:
	exp9[l] TK_LOR exp8[r]	{ $$ = create_ast_expr_binop(@$, AST_EXPR_BINOP_LOR, $l, $r); }
|	exp8

exp: exp9

%nterm <expr> function_call;
function_call:
	TK_FUNC '(' ')'			{ $$ = create_ast_expr_func_call(@$, $1); }
|	function_call_head ')'

%nterm <expr> function_call_head;
function_call_head:
	TK_FUNC '(' exp			{ $$ = create_ast_expr_func_call(@$, $1); ast_expr_list_append(&($$.func_call.arglist), $exp); }
|	function_call_head ',' exp { $$ = $1; ast_expr_list_append(&($$.func_call.arglist), $exp); }


%nterm <stmt> block;
block:
	block_head '}'		{ $$ = $1; context_stack_pop($$.block.context); }

%nterm <stmt> block_head;
block_head:
	'{'					{ $$ = create_ast_stmt_block(@$, true); context_stack_push($$.block.context); }
|	block_head stmt		{ $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); } // TODO: block needs to also update it's loc
|	block_head declaration
	{
		$$ = $1;
		if ($declaration.type == AST_DECL_STMT)
			ast_stmt_list_append(&($$.block.stmtlist), $declaration.stmt);
	}
|	block_head ';'

%nterm <stmt> no_context_block;
no_context_block:
	no_context_block_head '}'

%nterm <stmt> no_context_block_head;
no_context_block_head:
	'{'					{ $$ = create_ast_stmt_block(@$, false); }
|	no_context_block_head stmt		{ $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); } // TODO: block needs to also update it's loc
|	no_context_block_head declaration
	{
		$$ = $1;
		if ($declaration.type == AST_DECL_STMT)
			ast_stmt_list_append(&($$.block.stmtlist), $declaration.stmt);
	}
|	no_context_block_head ';'

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
	TK_VAR[var] '=' exp[val]	{ $$ = create_ast_stmt_assign(@$, $var, $val); }

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
|	single_var_declaration

%nterm <decl> function_declaration;
function_declaration:
	type name '(' function_args_definition[args] ')'
	<context>{
		context_t ctx = create_context();
		$$ = convert_to_ptr(ctx);
		context_stack_push($$);

		for (unsigned int i=0; i<$args.len; i++){
			ast_variable_t v = $args.data[i];
			ast_id_t id = (ast_id_t){.type=AST_ID_VAR, .var=v};
			ast_id_t* idp = convert_to_ptr(id);
			current_context_insert(v.name, idp);
		}
	}[context]
	no_context_block[body]
	{
		context_stack_pop($context);
		$$ = create_ast_decl_function(@$, $type, $name.name, $args, $context, $body);
	}

%nterm <argdeflist> function_args_definition;
function_args_definition:
	%empty		{ $$ = create_ast_variable_list(); }
|	type name	{ $$ = create_ast_variable_list(); ast_variable_list_append(&$$, (ast_variable_t){.declare_loc=@$, .type_ref=$type, .name=$name.name}); }
|	function_args_definition[args] ',' type name	{ $$ = $1; ast_variable_list_append(&$$, (ast_variable_t){.declare_loc=@name, .type_ref=$type, .name=$name.name}); } // TODO: properly track declare loc
// TODO: 'int foo(,int x){...}' shouldn't be valid!

%nterm <decl> single_var_declaration;
single_var_declaration:
	type name	';'				{ $$ = create_ast_decl_var(@$, $type, $name); }
|	type name '=' exp[val] ';'	{ $$ = create_ast_decl_var_assign(@$, $type, $name, $val); }



%%
