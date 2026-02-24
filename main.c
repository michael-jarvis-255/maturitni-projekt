#include "yystype.h"
#include "yyltype.h"
#include "build/parse.tab.h"
#include "ast.h"
#include "ast2llvm.h"
#include "parse/main.h"
#include "ast/print.h"
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

typedef enum {
	OPT_0,
	OPT_1,
	OPT_2,
	OPT_3
} opt_level_t;

typedef enum {
	OUTPUT_LLVM,
	OUTPUT_ASM,
	OUTPUT_OBJ
} output_format_t;

typedef struct {
	const char* input_path;
	const char* output_path;
	output_format_t output_format;
	opt_level_t opt_level;
} command_line_options_t;

#define STRING_SWITCH_START(x) {const char* __string_switch = x; do { if (0) {
#define STRING_CASE(x) } else if (strcmp(__string_switch, x) == 0) {
#define STRING_DEFAULT() } else {
#define STRING_SWITCH_END() }} while (0);}

command_line_options_t parse_arguments(const unsigned int argc, const char** argv){
	command_line_options_t opts = (command_line_options_t){
		.input_path = 0,
		.output_path = 0,
		.opt_level = OPT_2,
		.output_format = OUTPUT_OBJ
	};
	for (unsigned int i = 1; i < argc; i++){
		STRING_SWITCH_START(argv[i])
			STRING_CASE("-o")
				i++;
				if (i == argc){
					printf("expected filename after '-o'\n");
					exit(1);
				}
				opts.output_path = argv[i];
			STRING_CASE("-O0") opts.opt_level = OPT_0;
			STRING_CASE("-O1") opts.opt_level = OPT_1;
			STRING_CASE("-O2") opts.opt_level = OPT_2;
			STRING_CASE("-O3") opts.opt_level = OPT_3;
			STRING_CASE("-filetype=llvm") opts.output_format = OUTPUT_LLVM;
			STRING_CASE("-filetype=asm") opts.output_format = OUTPUT_ASM;
			STRING_CASE("-filetype=obj") opts.output_format = OUTPUT_OBJ;
			STRING_DEFAULT()
				if (opts.input_path){
					printf("please provide only one input file (got '%s' and '%s')\n", opts.input_path, argv[i]);
					exit(1);
				}
				opts.input_path = argv[i];
		STRING_SWITCH_END();
	}
	return opts;
}

char* create_tmp_path(const char* suffix){
	char* tmpfile = malloc(strlen("/tmp/XXXXXX")+strlen(suffix)+1);
	strcpy(tmpfile, "/tmp/XXXXXX");
	strcat(tmpfile, suffix);
	int fd = mkstemps(tmpfile, strlen(suffix));
	close(fd);
	return tmpfile;
}

const char* opt_level_flag(opt_level_t lvl){
	switch (lvl){
		case OPT_0: return "-O0";
		case OPT_1: return "-O1";
		case OPT_2: return "-O2";
		case OPT_3: return "-O3";
	}
	return "";
}

void system_concat(unsigned int n, ...){
	va_list args1;
	va_list args2;
	va_start(args1, n);
	va_copy(args2, args1);

	unsigned int length = n; // spaces between + null terminator
	for (unsigned int i=0; i<n; i++)
		length += strlen(va_arg(args1, const char*));

	char* const buf = malloc(length);
	buf[0] = 0;
	for (unsigned int i=0; i<n; i++){
		strcat(buf, va_arg(args2, const char*));
		if (i+1 < n) strcat(buf, " ");
	}

	int res = system(buf);
	free(buf);
	
	if (res) exit(1);

	va_end(args2);
	va_end(args1);
}

int main(int argc, const char** argv){
	if (argc < 2){
		printf("Usage: %s [options] file\n", argv[0]);
		return 1;
	}

	command_line_options_t options = parse_arguments(argc, argv);
	if (!options.input_path) {
		printf("no input files provided\n");
		return 1;
	}

	FILE* infile = fopen(options.input_path, "r");
	if (infile == 0){
		printf("Failed to open file '%s'\n", options.input_path);
		return 1;
	}

	// parse into ast
	ast_t ast = parse_file(infile, options.input_path);
	fclose(infile);

	if (received_error){
		printf("compilation stopped due to error(s)\n");
		free_ast_v(ast);
		return 1;
	}

	// convert to llvm
	llvm_program_t llvm = ast2llvm(ast);
	free_ast_v(ast);

	// emit llvm to file
	char* ir1_fp = create_tmp_path(".ll");
	FILE* ir1_file = fopen(ir1_fp, "w");
	llvm_program_to_stream(llvm, ir1_file);
	free_llvm_program(llvm);
	fclose(ir1_file);
	
	if (received_error){
		printf("compilation stopped due to error(s)\n");
		unlink(ir1_fp);
		return 1;
	}

	// optimise llvm ir
	char* opt_ir_fp = create_tmp_path(".ll");
	system_concat(6, "opt", "-o", opt_ir_fp, "-S", opt_level_flag(options.opt_level), ir1_fp);
	unlink(ir1_fp);
	free(ir1_fp);

	if (options.output_format == OUTPUT_LLVM){
		if (options.output_path){
			system_concat(3, "mv", opt_ir_fp, options.output_path);
		}else{
			puts("no output path specified"); // TODO: print to stdout
		}
		unlink(opt_ir_fp);
		free(opt_ir_fp);
		return 0;
	}

	if (options.output_format == OUTPUT_ASM){
		if (!options.output_path){
			puts("no output path specified"); // TODO: print to stdout
		}else{
			system_concat(6, "llc", "-o", options.output_path, "-filetype=asm", opt_level_flag(options.opt_level), opt_ir_fp);
		}
		unlink(opt_ir_fp);
		free(opt_ir_fp);
		return 0;
	}

	// create object file
	char* obj_fp = create_tmp_path(".o");
	system_concat(6, "llc", "-o", obj_fp, "-filetype=obj", opt_level_flag(options.opt_level), opt_ir_fp);
	unlink(opt_ir_fp);
	free(opt_ir_fp);

	// link with C runtime and dynamic linker
	if (!options.output_path){
		puts("no output path specified");
		unlink(obj_fp);
		free(obj_fp);
		return 1;
	}
	system_concat(4, "ld -lc -L /lib/x86_64-linux-gnu -dynamic-linker /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/crt1.o", obj_fp, "-o", options.output_path);
	unlink(obj_fp);
	free(obj_fp);

	return 0;
}
