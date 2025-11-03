%{
	#include "yystype.h"
	#include "ast.h"
	#include "parse.tab.h"

	static void update_yylloc(unsigned int yyleng){
		yylloc.first_line = yylloc.last_line;
		yylloc.first_column = yylloc.last_column;
		yylloc.last_column += yyleng;
	}
	static void newline_update_yylloc(){
		yylloc.last_line++; yylloc.last_column = 1;
	}
	static void string_update_yylloc(const char* yytext, unsigned int yyleng){
		update_yylloc(0);
		for (unsigned int i=0; i<yyleng; i++){
			if (yytext[i] == '\n') newline_update_yylloc();
			else yylloc.last_column++;
		}
	}
%}

%option noyywrap
%x comment
%x string

DIGIT [0-9]
ID_START [a-zA-Z_]
ID_CHAR [0-9a-zA-Z]

%%

\/\/.*$		update_yylloc(yyleng);
\/\*	BEGIN(comment);	update_yylloc(yyleng);
<comment>[^*\n]*	update_yylloc(yyleng);
<comment>\*\/	BEGIN(INITIAL);	update_yylloc(yyleng);
<comment>\*		update_yylloc(yyleng);

if			update_yylloc(yyleng); return TK_IF;
else		update_yylloc(yyleng); return TK_ELSE;
for			update_yylloc(yyleng); return TK_FOR;
while		update_yylloc(yyleng); return TK_WHILE;
break		update_yylloc(yyleng); return TK_BREAK;
continue	update_yylloc(yyleng); return TK_CONTINUE;
return		update_yylloc(yyleng); return TK_RETURN;

\<\=	update_yylloc(yyleng); return TK_LE;
\>\=	update_yylloc(yyleng); return TK_GE;
\=\=	update_yylloc(yyleng); return TK_EQ;
\!\=	update_yylloc(yyleng); return TK_NE;
\&\&	update_yylloc(yyleng); return TK_LAND;
\|\|	update_yylloc(yyleng); return TK_LOR;
\>\>	update_yylloc(yyleng); return TK_SHR;
\<\<	update_yylloc(yyleng); return TK_SHL;

{DIGIT}+				update_yylloc(yyleng); yylval.expr = create_ast_expr_const(yylloc, atoll(yytext)); return TK_INT;
{ID_START}{ID_CHAR}*	{ 
							update_yylloc(yyleng);
							yylval.str = malloc(yyleng);
							strncpy(yylval.str, yytext, yyleng);
							return TK_IDENTIFIER;
						}
\"[^"]*\"				{
							string_update_yylloc(yytext, yyleng);
							yylval.str = malloc(yyleng);
							strncpy(yylval.str, yytext, yyleng);
							return TK_STRING;
						}

<comment,INITIAL>"\n"			newline_update_yylloc();
[[:space:]]		update_yylloc(yyleng);
<<EOF>>		return YYEOF;
.			update_yylloc(yyleng); return yytext[0];
