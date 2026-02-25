%{
	#include <stdio.h>
	#include <string.h>
	#include "yystype.h"
	#include "yyltype.h"
	#include "ast.h"
	#include "message.h"
	#include "parse/main.h"
	void yyerror(__attribute__((unused)) scope_t** _, char const* err){
		fprintf(stderr, "%s\n", err);
	}
	int yylex ();

	static unsigned int err_recovery_scope_depth = 0;
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
%param {scope_t** current_scope}

%destructor { free($$); } <str>
%destructor { free_ast_expr_v($$); } <expr>
%destructor { free_ast_stmt_v($$); } <stmt>
%destructor { free($$.name); } <name>
%destructor { /* emtpy */ } <type_ref>
%destructor { /* emtpy */ } <var_ref>
%destructor { free_ast_lvalue_v($$); } <lvalue>
%destructor { free_ast_lvalue_v($$.lvalue); } <incomplete_lvalue>
%destructor { /* emtpy */ } <func_ref>
%destructor { free_scope($$); err_recovery_scope_depth++; } <scope>
%destructor { if (err_recovery_scope_depth) err_recovery_scope_depth--; else free_scope(pop_scope(current_scope)); } <scope_start>
%destructor { deep_free_ast_variable_list(&$$); } <var_list>
%destructor { shallow_free_ast_variable_ptr_list(&$$); } <var_ptr_list>
%destructor { deep_free_ast_stmt_list(&$$); } <stmt_list>

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
	TK_STRUCT '{' anonymous_struct_body[members] '}'	{ $$ = parse_anonymous_struct(@$, *current_scope, $members); }

%nterm <var_list> anonymous_struct_body;
anonymous_struct_body:
	%empty	 { $$ = create_ast_variable_list(); }
|	anonymous_struct_body[list] type name ';'	{ $$ = $list; ast_variable_list_append(&$$, (ast_variable_t){ .declare_loc=loc_add(@type, @name), .name=$name.name, .type_ref=$type }); }

%nterm <lvalue> lvalue;
lvalue:
	incomplete_lvalue { $$ = $1.lvalue; }

%nterm <incomplete_lvalue> incomplete_lvalue;
incomplete_lvalue:
	TK_VAR	{ $$.lvalue = create_ast_lvalue($1, @$); $$.err = false; }
|	incomplete_lvalue '.' name	{ $$ = $1; if (!$$.err) ($$.err = ast_lvalue_extend(&($$.lvalue), @$, false, $name)); free($name.name); }
|	incomplete_lvalue TK_ARROW name	{ $$ = $1; if (!$$.err) ($$.err = ast_lvalue_extend(&($$.lvalue), @$, false, $name)); free($name.name); }

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
	'{' begin_local_scope block_body end_local_scope[scope] '}'		{ $$ = create_ast_stmt_block(@$, $block_body, $scope); (void)$begin_local_scope; }

%nterm <stmt> scopeless_block;
scopeless_block:
	'{' block_body '}'		{ $$ = create_ast_stmt_block(@$, $block_body, 0); }

%nterm <stmt_list> block_body;
block_body:
	%empty	{ $$ = create_ast_stmt_list(); }
|	block_body stmt	{ $$ = $1; ast_stmt_list_append(&$$, $stmt); }
|	block_body var_assign_declaration[stmt] { $$ = $1; ast_stmt_list_append(&$$, $stmt); }
|	block_body var_declaration
|	block_body ';'

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
non_if_for_loop: // TODO: variables inside block can shadow 'init'
	TK_FOR '(' begin_local_scope basic_stmt[init]         ';' exp[cond] ';' basic_stmt[step] ')' non_if_stmt[body] end_local_scope[scope]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $scope); (void)$begin_local_scope; }
|	TK_FOR '(' begin_local_scope var_assign_declaration[init] exp[cond] ';' basic_stmt[step] ')' non_if_stmt[body] end_local_scope[scope]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $scope); (void)$begin_local_scope; }

%nterm <stmt> if_ending_for_loop;
if_ending_for_loop:
	TK_FOR '(' begin_local_scope basic_stmt[init]         ';' exp[cond] ';' basic_stmt[step] ')' if_ending_stmt[body] end_local_scope[scope]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $scope); (void)$begin_local_scope; }
|	TK_FOR '(' begin_local_scope var_assign_declaration[init] exp[cond] ';' basic_stmt[step] ')' if_ending_stmt[body] end_local_scope[scope]	{ $$ = create_ast_stmt_for(@$, $init, $cond, $step, $body, $scope); (void)$begin_local_scope; }

%nterm function_declaration;
function_declaration: // TODO: use block_body instead of scopeless_block ?
	type name '(' begin_local_scope function_args[args] ')' scopeless_block[body] end_local_scope[scope]	{ parse_function_decl(@$, *current_scope, $type, $name.name, $args, $scope, $body); (void)$begin_local_scope; }

%nterm <var_ptr_list> function_args function_args_non_emtpy;
function_args:
	%empty		{ $$ = create_ast_variable_ptr_list(); }
|	function_args_non_emtpy
function_args_non_emtpy:
	type name
		{
			$$ = create_ast_variable_ptr_list();
			ast_id_t* var_id = parse_variable_decl(@$, *current_scope, $type, $name);
			if (var_id) ast_variable_ptr_list_append(&$$, &var_id->var);
		}
|	function_args_non_emtpy[args] ',' type name
		{
			$$ = $1;
			ast_id_t* var_id = parse_variable_decl(@$, *current_scope, $type, $name);
			if (var_id) ast_variable_ptr_list_append(&$$, &var_id->var);
		}


%nterm var_declaration;
var_declaration:
	type name	';'				{ parse_variable_decl(@$, *current_scope, $type, $name); }

%nterm <stmt> var_assign_declaration;
var_assign_declaration:
	type name '=' exp[val] ';'	{ $$ = parse_variable_assign_decl(@$, @name, *current_scope, $type, $name, $val); }

%nterm global_declaration;
global_declaration:
	type name	';'				{ parse_global_decl(@$, *current_scope, $type, $name); }
|	type name '=' exp[val] ';'	{ parse_global_assign_decl(@$, *current_scope, $type, $name, $val); }

%nterm typedef_declaration;
typedef_declaration:
	TK_TYPEDEF type name ';'	{ parse_typedef_decl(@$, *current_scope, $type, $name); }

%nterm <scope_start> begin_local_scope;
begin_local_scope:
	%empty	{ push_new_scope(current_scope); $$=0; }

%nterm <scope> end_local_scope;
end_local_scope:
	%empty	{ $$ = pop_scope(current_scope); }

%%

static int yyreport_syntax_error(const yypcontext_t* ctx, __attribute__((unused)) scope_t** _){
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
