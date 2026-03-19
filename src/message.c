#include "message.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

bool received_error;
source_lines_t global_source_lines;

source_lines_t create_source_lines(const char* source){
	source_lines_t source_lines = (source_lines_t){ .line_count=1, .lines=0 };

	for (const char* c = source; *c; c++)
		if (*c == '\n') source_lines.line_count++;
	source_lines.lines = calloc(source_lines.line_count, sizeof(const char*));

	unsigned int i = 0;
	for (const char* end = source; *end; end++){
		if (*end != '\n') continue;
		source_lines.lines[i] = strndup(source, end-source);
		source = end+1;
		i++;
	}
	return source_lines;
}

void free_source_lines_v(source_lines_t source_lines){
	for (unsigned int i=0; i < source_lines.line_count; i++){
		free(source_lines.lines[i]);
	}
	free(source_lines.lines);
}

static unsigned int standard_print(const char* msg, const unsigned int n, unsigned int col){ // col is column that print starts at (for expanding tabs)
	for (unsigned int i=0; i<n; i++){
		char c = msg[i];

		if (c >= 0x7f) continue;
		if (c >= 0x20) {
			fprintf(stderr, "%c", c);
			col++;
			continue;
		}
		if (c == '\t'){
			do {
				fprintf(stderr, " ");
				col++;
			} while (col % TAB_SIZE);
		}
		if (c == 0) break;
	}
	return col;
}

static void print_msg(loc_t loc, const char* msg_type, const char* msg, const char* colour){
	fprintf(stderr, "%sline %u: %s%s:%s %s\n", STYLE_BOLD, loc.first_line+1, colour, msg_type, STYLE_RESET, msg);
	unsigned int tilde_count = 0;
	unsigned int col = 0;
	if (loc.first_line == loc.last_line){
		fprintf(stderr, "%5u |    ", loc.first_line+1);
		col = standard_print(global_source_lines.lines[loc.first_line], loc.first_column, col);
		fprintf(stderr, "%s", colour);
		unsigned int col2 = standard_print(global_source_lines.lines[loc.first_line] + loc.first_column, loc.last_column - loc.first_column, col);
		fprintf(stderr, "%s", STYLE_RESET);
		standard_print(global_source_lines.lines[loc.first_line] + loc.last_column, -1, col2);
		fprintf(stderr, "\n");

		tilde_count = loc.last_column - loc.first_column - 1;
	}else{
		fprintf(stderr, "%5u |    ", loc.first_line+1);
		col = standard_print(global_source_lines.lines[loc.first_line], loc.first_column, col);
		fprintf(stderr, "%s", colour);
		standard_print(global_source_lines.lines[loc.first_line] + loc.first_column, -1, col);
		fprintf(stderr, "%s\n", STYLE_RESET);

		tilde_count = strlen(global_source_lines.lines[loc.first_line]) - loc.first_column - 1;
	}

	fprintf(stderr, "      |    %*s%s^", col, "", colour);
	for (unsigned int i=0; i<tilde_count; i++){
		fprintf(stderr, "~");
	}
	fprintf(stderr, "%s\n", STYLE_RESET);

	if (loc.first_line != loc.last_line){
		fprintf(stderr, "      |    %s...%s\n", colour, STYLE_RESET);
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

__attribute__ ((noreturn))
void internal_error(const char* file, int line){
	fprintf(stderr, "%s%sinternal error:%s ", COLOUR_ERROR, STYLE_BOLD, STYLE_RESET);
	fprintf(stderr, "%s file \"%s\", line %i:%s ", STYLE_BOLD, file, line, STYLE_RESET);
	fprintf(stderr, "this is a bug in the compiler, please report it on github: https://github.com/redsti-github/prog\n");
	exit(1);
}
