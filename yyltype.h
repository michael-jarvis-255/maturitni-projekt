#ifndef _YYLTYPE_H_
#define _YYLTYPE_H_
typedef struct loc_t {
	unsigned int first_line;
	unsigned int first_column;
	unsigned int last_line;
	unsigned int last_column;	
} loc_t;
#define YYLTYPE loc_t
#endif
