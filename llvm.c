#include "llvm.h"
#include "string.h"
#include <stdarg.h>

typedef char char_t;
create_list_type_header(char, false);
create_list_type_impl(char, false);

typedef enum {
	PRINT_TARGET_STRING,
	PRINT_TARGET_STREAM,
} print_target_enum_t;

typedef struct {
	print_target_enum_t target;
	union {
		FILE* stream;
		char_list_t string;
	};
} print_target_t;

static void tprint(print_target_t* t, const char* s){
	switch (t->target){
		case PRINT_TARGET_STREAM:
			fwrite(s, 1, strlen(s), t->stream);
			return;
		case PRINT_TARGET_STRING:
			char_list_extend(&t->string, strlen(s), s);
			return;
	}
}

__attribute__ ((format (printf, 2, 3))) static void tprintf(print_target_t* t, const char* format, ...){
	va_list args;
	va_start(args, format);
	switch (t->target){
		case PRINT_TARGET_STREAM:
			vfprintf(t->stream, format, args);
			break;
		case PRINT_TARGET_STRING:
			{
				char buf[512];
				vsnprintf(buf, sizeof(buf), format, args);
				tprint(t, buf);
				break;
			}
	}
	va_end(args);
}

static void llvm_inst_body_to_target(const llvm_inst_t, print_target_t* t){
	// TODO
}

static void llvm_block_body_to_target(const llvm_basic_block_t* block, print_target_t* t){
	for (unsigned int i = 0; i < block->instructions.len; i++){
		tprintf(t, "%%%u = ", block->regbase + 1);
		llvm_inst_body_to_target(block->instructions.data[i], t);
		tprint(t, "\n");
	}
}

static void llvm_func_to_target(const llvm_function_t* f, print_target_t* t){
	tprint(t, "define <type> <name>(<arg>, <arg>){\n");

	for (unsigned int i = 0; i < f->blocks.len; i++){
		tprintf(t, "l%u:\n", i);
		llvm_block_body_to_target(&f->blocks.data[i], t);
	}
	
	tprint(t, "}\n");
}

void llvm_func_to_stream(const llvm_function_t* f, FILE* stream){
	llvm_func_to_target(f, &(print_target_t){.target=PRINT_TARGET_STREAM, .stream=stream});
}

char* llvm_func_to_string(const llvm_function_t* f){
	char_list_t str = create_char_list();
	llvm_func_to_target(f, &(print_target_t){.target=PRINT_TARGET_STRING, .string=str});
	return str.data;
}
