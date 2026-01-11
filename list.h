#ifndef _LIST_H_
#define _LIST_H_
#include <stdlib.h>
#include <stdio.h>

#define _MACRO_IF_0(body)
#define _MACRO_IF_1(body) body
#define _MACRO_IF_false(body)
#define _MACRO_IF_true(body) body
#define MACRO_IF(cond, body) _MACRO_IF_##cond(body)

// T should be the base type (without _t suffix)
#define create_list_type_header(T, deep_free) \
typedef struct T##_list_t { \
	unsigned int cap;	\
	unsigned int len;	\
	T##_t* data;	\
} T##_list_t;	\
T##_list_t create_##T##_list();	\
void T##_list_append(T##_list_t* list, T##_t elem);	\
void T##_list_pop(T##_list_t* list);	\
MACRO_IF(deep_free, void deep_free_##T##_list(T##_list_t* list);) \
void shallow_free_##T##_list(T##_list_t* list)	\

#define create_list_type_impl(T, deep_free) \
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
}	\
void T##_list_pop(T##_list_t* list){	\
	if (list->len == 0){	\
		printf("ERROR: cannot pop from empty list\n");	\
		exit(1);	\
	}	\
	list->len--;	\
}	\
void shallow_free_##T##_list(T##_list_t* list){	\
	free(list->data);	\
	list->data = 0;	\
	list->cap = 0;	\
	list->len = 0;	\
}	\
MACRO_IF(deep_free, \
	void deep_free_##T##_list(T##_list_t* list){	\
		for (unsigned int i=0; i<list->len; i++){	\
			free_##T##_v(list->data[i]);	\
		}	\
		free(list->data);	\
		list->data = 0;	\
		list->cap = 0;	\
		list->len = 0;	\
	}	\
)

#endif
