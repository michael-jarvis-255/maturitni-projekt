%{
	#include "ast.h"
	//extern YYSTYPE yylval;
	typedef union YYSTYPE {
		char* str;
		ast_expr_t expr;
		ast_stmt_t stmt;
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

if			return TK_IF;
else		return TK_ELSE;
for			return TK_FOR;
while		return TK_WHILE;
break		return TK_BREAK;
continue	return TK_CONTINUE;

\<\=	return TK_LE;
\>\=	return TK_GE;
\=\=	return TK_EQ;
\!\=	return TK_NE;
\&\&	return TK_LAND;
\|\|	return TK_LOR;
\>\>	return TK_SHR;
\<\<	return TK_SHL;

{DIGIT}+				yylval.expr = create_ast_expr_const(atoll(yytext)); return TK_INT;
{ID_START}{ID_CHAR}*	{ 
							yylval.str = malloc(yyleng);
							strncpy(yylval.str, yytext, yyleng);
							return TK_IDENTIFIER;
						}
\"[^"]*\"				{ 
							yylval.str = malloc(yyleng);
							strncpy(yylval.str, yytext, yyleng);
							return TK_STRING;
						}

[[:space:]]
<<EOF>>		return YYEOF;
.			return yytext[0];
