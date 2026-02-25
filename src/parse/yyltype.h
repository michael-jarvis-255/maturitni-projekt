#ifndef _YYLTYPE_H_
#define _YYLTYPE_H_
#include <stdbool.h>
typedef struct loc_t {
	unsigned int first_line;
	unsigned int first_column;
	unsigned int last_line;
	unsigned int last_column;	
} loc_t;
#define YYLTYPE loc_t

static inline bool loc_eq(loc_t a, loc_t b){
	return (a.first_line == b.first_line)
		&& (a.first_column == b.first_column)
		&& (a.last_line == b.last_line)
		&& (a.last_column == b.last_column);
}

static inline loc_t loc_add(loc_t a, loc_t b){
	return (loc_t){
		.first_line   = a.first_line   < b.first_line   ? a.first_line   : b.first_line,
		.first_column = a.first_column < b.first_column ? a.first_column : b.first_column,
		.last_line    = a.last_line    > b.last_line    ? a.last_line    : b.last_line,
		.last_column  = a.last_column  > b.last_column  ? a.last_column  : b.last_column
	};
}
#endif
