#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include "parse/yyltype.h"

#define COLOUR_INFO "\033[36m"
#define COLOUR_WARNING "\033[33m"
#define COLOUR_ERROR "\033[31m"
#define STYLE_BOLD "\033[1m"
#define STYLE_RESET "\033[0m"
#define TAB_SIZE 4

typedef struct source_lines_t {
	unsigned int line_count;
	char** lines;
} source_lines_t;

source_lines_t create_source_lines(const char* source);
void free_source_lines_v(source_lines_t source_lines);

void print_info(loc_t loc, char* msg);
void print_warning(loc_t loc, char* msg);
void print_error(loc_t loc, char* msg);

void printf_info(loc_t loc, char* msg, ...) __attribute__ ((format (printf, 2, 3)));
void printf_warning(loc_t loc, char* msg, ...) __attribute__ ((format (printf, 2, 3)));
void printf_error(loc_t loc, char* msg, ...) __attribute__ ((format (printf, 2, 3)));

void internal_error(const char* file, int line) __attribute__ ((noreturn));
#define INTERNAL_ERROR() internal_error(__FILE__, __LINE__)

extern source_lines_t global_source_lines; // TODO: something better

#endif
