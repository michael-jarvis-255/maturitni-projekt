MAKEFLAGS += -rR
GCCFLAGS = -Wall -Wextra -Wpedantic -Wno-stringop-overread -g -O3 $(shell llvm-config --cflags)

compiler: build/src/lex.y.c build/src/parse.tab.c src/ast/main.c src/ast2llvm.c src/llvm.c src/lib/bignum.c src/message.c src/parse/main.c src/ast/scope.c src/main.c
	gcc $^ -o $@ -I build/h/ -I src/ $(GCCFLAGS)

build/src/parse.tab.c build/h/parse.tab.h: src/parse/parse.y src/parse/yystype.h src/parse/yyltype.h src/ast/main.h src/message.h src/parse/main.h
	@mkdir build/src -p
	@mkdir build/h -p
	bison -Wcounterexamples --header=build/h/parse.tab.h --output=build/src/parse.tab.c src/parse/parse.y

build/src/lex.y.c: src/parse/lex.y build/h/parse.tab.h src/parse/yystype.h
	@mkdir build/src -p
	flex -o $@ src/parse/lex.y
