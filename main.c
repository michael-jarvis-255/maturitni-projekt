#include "yystype.h"
#include "yyltype.h"
#include "build/parse.tab.h"
#include "ast.h"
#include "llvm.h"

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

	// emit all functions in top context
	for (context_iterator_t iter = context_iter(&top_level_context); iter.current; iter = context_iter_next(iter)){
		ast_id_t* id = iter.current->value;
		if (id->type == AST_ID_VAR){
			char buf[128];
			snprintf(buf, sizeof(buf), "variable '%s' declared here", id->var.name);
			print_info(id->var.declare_loc, buf);
		}
		
		if (id->type != AST_ID_FUNC) continue;

		printf("TODO: emit '%s'\n", iter.current->key);
		//llvm_emit_ast_func(id->func);

		print_warning(id->func.args.data[0].declare_loc, "test msg");
		print_error(id->func.declare_loc, "test msg");
	}
	
	ast_cleanup_context();
	fclose(code);
}
