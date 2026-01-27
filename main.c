#include "yystype.h"
#include "yyltype.h"
#include "build/parse.tab.h"
#include "ast.h"
#include "ast2llvm.h"

extern FILE *yyin;

int main(int argc, char** argv){
	if (argc != 2){
		printf("Usage: %s <file>\n", argv[0]);
		exit(1);
	}

	FILE* code = fopen(argv[1], "r");
	if (code == 0){
		printf("Failed to open file '%s'\n", argv[1]);
		exit(1);
	}

	ast_init_context(code);
	if (fseek(code, 0, SEEK_SET)){
		printf("Failed to seek file.\n");
		exit(1);
	}
	yyin = code;
	yyparse();

	// if an error was generated (with print_error), stop compilation
	if (received_error){
		printf("compilation stopped due to error(s)\n");
		return 1;
	}

	// emit all functions in top context
	FILE* outfile = fopen("out.ll", "w");
	for (context_iterator_t iter = context_iter(&top_level_context); iter.current; iter = context_iter_next(iter)){
		ast_id_t* id = iter.current->value;
		if (id->type != AST_ID_FUNC) continue;

		llvm_function_t func = ast2llvm_emit_func(id->func);
		
		printf("-- EMIT '%s' --\n", iter.current->key);
		char* llvm_code = llvm_func_to_string(&func);
		llvm_func_to_stream(&func, outfile);
		puts(llvm_code);
		free(llvm_code);
		puts("----");

		free_llvm_function(func);
	}
	fclose(outfile);
	if (received_error){
		printf("compilation stopped due to error(s)\n");
		return 1;
	}
	
	ast_cleanup_context();
	fclose(code);
}
