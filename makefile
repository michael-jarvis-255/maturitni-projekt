MAKEFLAGS += -rR

a.out: build/lex.yy.c build/parse.tab.c ast.c
	gcc $^ -o $@ -I .

build/parse.tab.c build/parse.tab.h: parse.y
	bison --header=build/parse.tab.h --output=build/parse.tab.c $^

build/lex.yy.c: lex.yy build/parse.tab.h
	flex -o $@ lex.yy

