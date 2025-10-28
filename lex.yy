%{
	#include "ast.h"
	//extern YYSTYPE yylval;
	typedef union YYSTYPE {
		char* str;
		ast_expr_t expr;
	} YYSTYPE;
	#include "parse.tab.h"
%}

%option noyywrap
%x comment

DIGIT [0-9]
ID_START [a-zA-Z_]
ID_CHAR [0-9a-zA-Z]

%%

\/\/.*$
\/\*	BEGIN(comment);
<comment>[^*]*
<comment>\*\/	BEGIN(INITIAL);
<comment>\*

{DIGIT}+				yylval.expr = create_ast_expr_const(atoll(yytext)); return TK_INT;
{ID_START}{ID_CHAR}+	yylval.str = yytext; return TK_IDENTIFIER;
\"[^"]*\"				yylval.str = yytext; return TK_STRING;

" \t\r\n"
<<EOF>>		return YYEOF;
.			return yytext[0];
