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
		yylloc.last_line++; yylloc.last_column = 0;
	}
	static void string_update_yylloc(const char* yytext, unsigned int yyleng){
		update_yylloc(0);
		for (unsigned int i=0; i<yyleng; i++){
			if (yytext[i] == '\n') newline_update_yylloc();
			else yylloc.last_column++;
		}
	}
%}

%option nounput
%option noinput
%option noyywrap
%x comment
%x string

DIGIT [0-9]
ID_START [a-zA-Z_]
ID_CHAR [0-9a-zA-Z_]

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
typedef		update_yylloc(yyleng); return TK_TYPEDEF;
struct		update_yylloc(yyleng); return TK_STRUCT;

\<\=	update_yylloc(yyleng); return TK_LE;
\>\=	update_yylloc(yyleng); return TK_GE;
\=\=	update_yylloc(yyleng); return TK_EQ;
\!\=	update_yylloc(yyleng); return TK_NE;
\&\&	update_yylloc(yyleng); return TK_LAND;
\|\|	update_yylloc(yyleng); return TK_LOR;
\>\>	update_yylloc(yyleng); return TK_SHR;
\<\<	update_yylloc(yyleng); return TK_SHL;
\-\>	update_yylloc(yyleng); return TK_ARROW;

{DIGIT}+				update_yylloc(yyleng); bignum_t* num = create_bignum(); bignum_from_string(num, yytext); yylval.expr = create_ast_expr_const(yylloc, num); return TK_INT;
{ID_START}{ID_CHAR}*	{ 
							update_yylloc(yyleng);
							char* name = malloc(yyleng+1);
							strncpy(name, yytext, yyleng);
							name[yyleng] = 0;

							ast_id_t* id = context_stack_get(name);
							if (id == 0){
								yylval.name.name = name;
								yylval.name.loc = yylloc;
								return TK_NAME;
							}
							
							switch (id->type){
								case AST_ID_TYPE:
									yylval.type_ref = &id->type_;
									free(name);
									return TK_TYPE;
								case AST_ID_VAR:
									yylval.var_ref = &id->var;
									free(name);
									return TK_VAR;
								case AST_ID_FUNC:
									yylval.func_ref = &id->func;
									free(name);
									return TK_FUNC;
								default:
									yylval.name.name = name;
									yylval.name.loc = yylloc;
									return TK_NAME;
							}
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
