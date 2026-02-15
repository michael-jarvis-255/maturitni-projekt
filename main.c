#include "yystype.h"
#include "yyltype.h"
#include "build/parse.tab.h"
#include "ast.h"
#include "ast2llvm.h"
#include <unistd.h>
#include <string.h>

extern FILE *yyin;

char* process_file(const char* cmd, char* infile, const char* suffix){
	char* tmpfile = malloc(strlen("/tmp/XXXXXX")+strlen(suffix)+1);
	strcpy(tmpfile, "/tmp/XXXXXX");
	strcat(tmpfile, suffix);

	int fd = mkstemps(tmpfile, strlen(suffix));
	char buf[strlen(cmd) + 1 + strlen(infile) + 1 + strlen("-o") + 1 + strlen(tmpfile) + 1];
	strcpy(buf, cmd);
	strcat(buf, " ");
	strcat(buf, infile);
	strcat(buf, " -o ");
	strcat(buf, tmpfile);

	int res = system(buf);
	close(fd);
	//unlink(infile);
	free(infile);
	
	if (res){
		//printf("error: command '%s' failed with code %i\n", buf, res);
		exit(1);
	}

	return tmpfile;
}

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

	// emit all functions to llvm ir
	char* fp = strdup("/tmp/XXXXXX.ll");
	FILE* outfile = fdopen(mkstemps(fp, 3), "w");
	for (context_iterator_t iter = context_iter(&top_level_context); iter.current; iter = context_iter_next(iter)){
		ast_id_t* id = iter.current->value;
		switch (id->type){
			case AST_ID_FUNC:
			{
				llvm_function_t func = ast2llvm_emit_func(id->func);		
				llvm_func_to_stream(&func, outfile);
				free_llvm_function(func);
				break;
			}
			case AST_ID_VAR:
				llvm_global_to_stream(&id->var, outfile);
				break;
			case AST_ID_TYPE:
				break;
		}
	}
	fclose(outfile);
	fclose(code);
	ast_cleanup_context();
	
	if (received_error){
		printf("compilation stopped due to error(s)\n");
		unlink(fp);
		return 1;
	}

	// use llvm to compile to object
	fp = process_file("llvm-as", fp, ".bc");
	printf("%s\n", fp);
	fp = process_file("opt -O3", fp, ".bc");
	printf("%s\n", fp);
	fp = process_file("llc -O3 -filetype=obj", fp, ".o");
	fp = process_file("ld -lc -L /lib/x86_64-linux-gnu -dynamic-linker /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/crt1.o", fp, ".o");
	printf("generated %s\n", fp);
	free(fp);
}
