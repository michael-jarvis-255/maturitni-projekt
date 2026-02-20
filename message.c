#include "message.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

extern char** source_lines; // TODO: something better than this
extern bool received_error; // TODO: something better than this

static unsigned int standard_print(const char* msg, const unsigned int n, unsigned int col){ // col is column that print starts at (for expanding tabs)
	for (unsigned int i=0; i<n; i++){
		char c = msg[i];
		
		if (c >= 0x7f) continue;
		if (c >= 0x20) {
			printf("%c", c);
			col++;
			continue;
		}
		if (c == '\t'){
			do {
				printf(" ");
				col++;
			} while (col % TAB_SIZE);
		}
		if (c == 0) break;
	}
	return col;
}

static void print_msg(loc_t loc, const char* msg_type, const char* msg, const char* colour){
	printf("%sline %u: %s%s:%s %s\n", STYLE_BOLD, loc.first_line+1, colour, msg_type, STYLE_RESET, msg);
	unsigned int tilde_count = 0;
	unsigned int col = 0;
	if (loc.first_line == loc.last_line){
		printf("%5u |    ", loc.first_line+1);
		col = standard_print(source_lines[loc.first_line], loc.first_column, col);
		printf("%s", colour);
		unsigned int col2 = standard_print(source_lines[loc.first_line] + loc.first_column, loc.last_column - loc.first_column, col);
		printf("%s", STYLE_RESET);
		standard_print(source_lines[loc.first_line] + loc.last_column, -1, col2);
		printf("\n");
		
		tilde_count = loc.last_column - loc.first_column - 1;
	}else{
		printf("%5u |    ", loc.first_line+1);
		col = standard_print(source_lines[loc.first_line], loc.first_column, col);
		printf("%s", colour);
		standard_print(source_lines[loc.first_line] + loc.first_column, -1, col);
		printf("%s\n", STYLE_RESET);

		tilde_count = strlen(source_lines[loc.first_line]) - loc.first_column - 1;
	}

	printf("      |    %*s%s^", col, "", colour);
	for (unsigned int i=0; i<tilde_count; i++){
		printf("~");
	}
	printf("%s\n", STYLE_RESET);

	if (loc.first_line != loc.last_line){
		printf("      |    %s...%s\n", colour, STYLE_RESET);
	}
}

void print_info(loc_t loc, char* msg){
	print_msg(loc, "info", msg, COLOUR_INFO);
}
void print_warning(loc_t loc, char* msg){
	print_msg(loc, "warning", msg, COLOUR_WARNING);
}
void print_error(loc_t loc, char* msg){
	received_error = true;
	print_msg(loc, "error", msg, COLOUR_ERROR);
}

static void vprintf_msg(loc_t loc, const char* msg_type, const char* colour, const char* msg, va_list args){
	char buf[512];
	vsnprintf(buf, sizeof(buf), msg, args);
	print_msg(loc, msg_type, buf, colour);
}

void printf_info(loc_t loc, char* msg, ...){
	va_list args;
	va_start(args, msg);
	vprintf_msg(loc, "info", COLOUR_INFO, msg, args);
	va_end(args);
}
void printf_warning(loc_t loc, char* msg, ...){
	va_list args;
	va_start(args, msg);
	vprintf_msg(loc, "warning", COLOUR_WARNING, msg, args);
	va_end(args);
}
void printf_error(loc_t loc, char* msg, ...){
	received_error = true;
	va_list args;
	va_start(args, msg);
	vprintf_msg(loc, "error", COLOUR_ERROR, msg, args);
	va_end(args);
}
