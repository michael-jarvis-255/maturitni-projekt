MAKEFLAGS += -rR

a.out: build/lex.yy.c build/parse.tab.c ast.c hashmap.c
	gcc $^ -o $@ -I .

build/parse.tab.c build/parse.tab.h: parse.y ast.h yystype.h
	@mkdir build -p
	bison -Wcounterexamples --header=build/parse.tab.h --output=build/parse.tab.c parse.y

build/lex.yy.c: lex.yy build/parse.tab.h yystype.h
	flex -o $@ lex.yy

