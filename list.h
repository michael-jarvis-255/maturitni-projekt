#ifndef _LIST_H_
#define _LIST_H_
#include <stdlib.h>

// T should be the base type (without _t suffix)
#define create_list_type_header(T) \
typedef struct T##_list_t { \
	unsigned int cap;	\
	unsigned int len;	\
	T##_t* data;	\
} T##_list_t;	\
T##_list_t create_##T##_list();	\
void T##_list_append(T##_list_t* list, T##_t elem);	\

#define create_list_type_impl(T) \
T##_list_t create_##T##_list(){	\
	return (T##_list_t){	\
		.len = 0,\
		.cap = 4,\
		.data = malloc(4*sizeof(T##_t))	\
	};	\
}	\
	\
void T##_list_append(T##_list_t* list, T##_t elem){ \
	if (list->cap == list->len){		\
		list->cap *= 2;	\
		list->data = reallocarray(list->data, list->cap, sizeof(T##_t));	\
	}	\
	list->data[list->len] = elem;	\
	list->len++;	\
}

#endif
