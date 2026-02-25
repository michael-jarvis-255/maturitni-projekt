#include <string.h>

static unsigned long str_hash(const char* str){
	unsigned long h = 1337;
	while (str[0]){
		h = (h*6364136223846793005) + str[0];
		str++;
	}
	return h;
}

static inline int str_cmp(const char* a, const char* b){
	return strcmp(a, b);
}
