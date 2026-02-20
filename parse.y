%{
	#include <stdio.h>
	#include <string.h>
	#include "yystype.h"
	#include "yyltype.h"
	#include "ast.h"
	#include "message.h"
	void yyerror(char const* err){
		fprintf(stderr, "%s\n", err);
	}
	int yylex ();
%}

%token <expr> TK_NUMBER "number"
%token <str> TK_STRING "string"
%token <name> TK_NAME "name"
%token <var_ref> TK_VAR "variable name"
%token <type_ref> TK_TYPE "type name"
%token <func_ref> TK_FUNC "function name"
%token TK_IF "'if'"
%token TK_ELSE "'else'"
%token TK_FOR "'for'"
%token TK_WHILE "'while'"
%token TK_BREAK "'break'"
%token TK_CONTINUE "'continue'"
%token TK_RETURN "'return'"
%token TK_TYPEDEF "'typedef'"
%token TK_STRUCT "'struct'"

%token TK_LE "<="
%token TK_GE ">="
%token TK_EQ "=="
%token TK_NE "!="
%token TK_LAND "&&"
%token TK_LOR "||"
%token TK_SHR ">>"
%token TK_SHL "<<"
%token TK_ARROW "->"

%define parse.error custom

%%

main:
	declaration_list YYEOF

declaration_list:
	%empty
|	declaration_list function_declaration
|	declaration_list typedef_declaration
|	declaration_list global_declaration

%nterm <type_ref> type;
type:
	TK_TYPE
|	type '*'		{ $$ = get_ast_pointer_type($1); }
|	anonymous_struct

%nterm <type_ref> anonymous_struct;
anonymous_struct:
	anonymous_struct_head '}'	{ $$ = ast_anon_struct_finalise($1); }

%nterm <type_ref> anonymous_struct_head;
anonymous_struct_head:
	TK_STRUCT '{'	{ $$ = create_ast_anon_struct_head(@$); }
|	anonymous_struct_head type name ';'		{ $$ = $1; ast_anon_struct_head_append($$, @name, $type, $name); } // TODO: loc should be @type + @name, not just @name

%nterm <lvalue> lvalue;
lvalue:
	TK_VAR	{ $$ = create_ast_lvalue($1); }
|	lvalue '.' name	{ $$ = ast_lvalue_extend($1, false, $name); }
|	lvalue TK_ARROW name	{ $$ = ast_lvalue_extend($1, true, $name); }

%nterm <name> name;
name:
	TK_NAME
|	TK_TYPE		{ $$ = (ast_name_t){ .loc=@1, .name=strdup($1->name) }; }
|	TK_VAR		{ $$ = (ast_name_t){ .loc=@1, .name=strdup($1->name) }; }

%nterm <expr> exp exp0 exp1 exp2 exp3 exp4 exp5 exp6 exp7 exp8 exp9;
exp0:
	TK_NUMBER	{ $$ = $1; }
|	lvalue		{ $$ = create_ast_expr_lvalue(@$, $1); }
|	function_call
|	'(' exp ')'	{ $$ = $2; }
|	'-' exp0[op]	{ $$ = create_ast_expr_unop(@$, AST_EXPR_UNOP_NEG, $op); }
|	'~' exp0[op]	{ $$ = create_ast_expr_unop(@$, AST_EXPR_UNOP_BNOT, $op); }
|	'!' exp0[op]	{ $$ = create_ast_expr_unop(@$, AST_EXPR_UNOP_LNOT, $op); }
|	'*' exp0[op]	{ $$ = create_ast_expr_unop(@$, AST_EXPR_UNOP_DEREF, $op); }
|	'&' lvalue		{ $$ = create_ast_expr_ref(@$, $lvalue); }
|	'(' type ')' exp0[op]	{ $$ = create_ast_expr_cast(@$, $op, $type); }

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
|	block_head var_declaration
|	block_head var_assign_declaration[stmt] { $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); }
|	block_head ';'

%nterm <stmt> no_context_block;
no_context_block:
	no_context_block_head '}'

%nterm <stmt> no_context_block_head;
no_context_block_head:
	'{'					{ $$ = create_ast_stmt_block(@$, false); }
|	no_context_block_head stmt		{ $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); } // TODO: block needs to also update it's loc
|	no_context_block_head var_declaration
|	no_context_block_head var_assign_declaration[stmt] { $$ = $1; ast_stmt_list_append(&($$.block.stmtlist), $stmt); }
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
	lvalue '=' exp[val]	{ $$ = create_ast_stmt_assign(@$, $lvalue, $val); }

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
	for_header[ctx] basic_stmt[init] ';' exp[cond] ';' basic_stmt[step] ')' non_if_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $ctx); } // TODO: variables inside block can shadow 'init'
|	for_header[ctx] var_assign_declaration[init] exp[cond] ';' basic_stmt[step] ')' non_if_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $ctx); }

%nterm <stmt> if_ending_for_loop;
if_ending_for_loop:
	for_header[ctx] basic_stmt[init] ';' exp[cond] ';' basic_stmt[step] ')' if_ending_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $ctx); }
|	for_header[ctx] var_assign_declaration[init] exp[cond] ';' basic_stmt[step] ')' if_ending_stmt[body]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $ctx); }


%nterm <context> for_header;
for_header:
	TK_FOR '('
		{
			context_t ctx = create_context();
			$$ = convert_to_ptr(ctx);
			context_stack_push($$);
		}

%nterm function_declaration;
function_declaration:
	type name '(' function_args_definition[args] ')'
	<context>{
		context_t ctx = create_context();
		$$ = convert_to_ptr(ctx);
		context_stack_push($$);
	}[context]
	<id_ptr_list>{
		$$ = create_ast_id_ptr_list();
		for (unsigned int i=0; i<$args.len; i++){
			ast_variable_t v = $args.data[i];
			ast_id_t id = (ast_id_t){.type=AST_ID_VAR, .var=v};
			ast_id_t* idp = convert_to_ptr(id);
			current_context_insert(v.name, idp);
			ast_id_ptr_list_append(&$$, idp);
		}
		shallow_free_ast_variable_list(&$args);
	}[id_ptr_list]
	no_context_block[body]
	{
		context_stack_pop($context);
		ast_declare_function(@$, $type, $name.name, $id_ptr_list, $context, $body);
	}

%nterm <argdeflist> function_args_definition;
function_args_definition:
	%empty		{ $$ = create_ast_variable_list(); }
|	type name	{ $$ = create_ast_variable_list(); ast_variable_list_append(&$$, (ast_variable_t){.declare_loc=@$, .type_ref=$type, .name=$name.name}); }
|	function_args_definition[args] ',' type name	{ $$ = $1; ast_variable_list_append(&$$, (ast_variable_t){.declare_loc=@name, .type_ref=$type, .name=$name.name}); } // TODO: properly track declare loc
// TODO: 'int foo(,int x){...}' shouldn't be valid!

%nterm var_declaration;
var_declaration:
	type name	';'				{ ast_declare_variable(@$, $type, $name); }

%nterm <stmt> var_assign_declaration;
var_assign_declaration:
	type name '=' exp[val] ';'	{ $$ = ast_declare_variable_assign(@$, $type, $name, $val); }

%nterm global_declaration;
global_declaration:
	type name	';'				{ ast_declare_global(@$, $type, $name); }
|	type name '=' exp[val] ';'	{ ast_declare_global_assign(@$, $type, $name, $val); }

%nterm typedef_declaration;
typedef_declaration:
	TK_TYPEDEF type name ';'	{ ast_declare_typedef(@$, $type, $name); }

%%

#define COLOUR_INFO "\033[36m"
#define COLOUR_WARNING "\033[33m"
#define COLOUR_ERROR "\033[31m"
#define STYLE_BOLD "\033[1m"
#define STYLE_RESET "\033[0m"
static int yyreport_syntax_error(const yypcontext_t* ctx){
	unsigned int buflen = 0;
	char buf[1024];
	buf[0] = 0;

	#define MAX_EXPECTED 10
	yysymbol_kind_t expected[MAX_EXPECTED];
	int n = yypcontext_expected_tokens(ctx, expected, MAX_EXPECTED);
	for (int i=0; i<n; i++){
		buflen += snprintf(buf + buflen, sizeof(buf)-buflen, i == 0 ? ", expected %s%s%s%s" : " or %s%s%s%s", COLOUR_INFO, STYLE_BOLD, yysymbol_name(expected[i]), STYLE_RESET);
		if (buflen > sizeof(buf)) {
			buf[0] = 0;
			break;
		}
	}
	
	loc_t loc = *yypcontext_location(ctx);
	printf_error(loc, "%ssyntax error:%s unexpected %s%s%s%s%s", STYLE_BOLD, STYLE_RESET, COLOUR_ERROR, STYLE_BOLD, yysymbol_name(yypcontext_token(ctx)), STYLE_RESET, buf);
	return 0;
}
