MAKEFLAGS += -rR
GCCFLAGS = -Wall -Wextra -Wpedantic -g
ifneq ($(OS),Windows_NT)
	GCCFLAGS += -fsanitize=address,undefined
endif

a.out: build/lex.y.c build/parse.tab.c ast.c ast2llvm.c llvm.c bignum.c message.c ast/print.c main.c
	gcc $^ -o $@ -I . $(GCCFLAGS)

build/parse.tab.c build/parse.tab.h: parse.y ast.h yystype.h
	@mkdir build -p
	bison -Wcounterexamples --header=build/parse.tab.h --output=build/parse.tab.c parse.y

build/lex.y.c: lex.y build/parse.tab.h yystype.h
	flex -o $@ lex.y

cloc:
	cloc --exclude-dir=build .
