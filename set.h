#ifndef _SET_H_
#define _SET_H_
#include <stdbool.h>

#define create_set_type_header(T)	\
typedef struct T##_set_entry_t {	\
	T value;	\
	struct T##_set_entry_t* next;	\
} T##_set_entry_t;	\
	\
typedef struct {	\
	unsigned int size;	\
	unsigned int bcap;	\
	T##_set_entry_t** buckets;	\
} T##_set_t;	\
	\
typedef struct {	\
	T##_set_entry_t* current;	\
	const T##_set_t* set;	\
	unsigned int bi;	\
} T##_set_iterator_t;	\
	\
T##_set_t create_##T##_set();	\
void T##_set_add(T##_set_t* set, T value);	\
void T##_set_remove(T##_set_t* set, T value);	\
bool T##_set_contains(const T##_set_t* set, T value);	\
T##_set_iterator_t T##_set_iter(const T##_set_t* set);	\
T##_set_iterator_t T##_set_iter_next(T##_set_iterator_t iter);	\
void T##_set_free(T##_set_t* set)

#define create_set_type_impl(T)	\
T##_set_t create_##T##_set(){	\
	T##_set_t set;	\
	set.size = 0;	\
	set.bcap = 16;	\
	set.buckets = calloc(set.bcap, sizeof(T##_set_entry_t**));	\
	return set;	\
}	\
	\
void T##_remove(T##_set_t* set, T val){	\
	unsigned long h = T##_hash(val) % set->bcap;	\
	T##_set_entry_t* entry = set->buckets[h];	\
	if (T##_cmp(entry->value, val) == 0){	\
		set->buckets[h] = 0;	\
		set->size--;	\
		return;	\
	}	\
	while (entry->next){	\
		if (T##_cmp(entry->next->value, val) == 0){	\
			void* to_free = entry->next;	\
			entry->next = entry->next->next;	\
			free(to_free);	\
			set->size--;	\
			return;	\
		}	\
	}	\
}	\
	\
static void T##_set_grow(T##_set_t* set){	\
	unsigned int old_bcap = set->bcap;	\
	T##_set_entry_t** old_buckets = set->buckets;	\
	set->size = 0;	\
	set->bcap *= 4;	\
	set->buckets = calloc(set->bcap, sizeof(T##_set_entry_t**));	\
	\
	for (unsigned int h=0; h<old_bcap; h++){	\
		T##_set_entry_t* head = old_buckets[h];	\
		while (head){	\
			T##_set_entry_t* entry = head;	\
			head = entry->next;	\
				\
			unsigned long newh = T##_hash(entry->value) % set->bcap;	\
			entry->next = set->buckets[newh];	\
			set->buckets[newh] = entry;	\
		}	\
	}	\
	free(old_buckets);	\
}	\
	\
void T##_set_add(T##_set_t* set, T value){	\
	if (set->size > set->bcap*2)	\
		T##_set_grow(set);	\
		\
	unsigned long h = T##_hash(value) % set->bcap;	\
	T##_set_entry_t* entry = set->buckets[h];	\
	if (entry == 0){	\
		entry = malloc(sizeof(T##_set_entry_t));	\
		set->buckets[h] = entry;	\
		goto end;	\
	}	\
	while (entry->next){	\
		if (T##_cmp(entry->next->value, value) == 0) return;	\
		entry = entry->next;	\
	}	\
	entry->next = malloc(sizeof(T##_set_entry_t));	\
	entry = entry->next;	\
end:	\
	entry->value = value;	\
	entry->next = 0;	\
	set->size++;	\
}	\
	\
T##_set_iterator_t T##_set_iter(const T##_set_t* set){	\
	for (unsigned int i=0; i<set->bcap; i++){	\
		if (set->buckets[i]){	\
			return (T##_set_iterator_t){	\
				.current=set->buckets[i],	\
				.set=set,	\
				.bi=i,	\
			};	\
		}	\
	}	\
	return (T##_set_iterator_t){	\
		.current=0,	\
		.set=set,	\
		.bi=set->bcap	\
	};	\
}	\
T##_set_iterator_t T##_set_iter_next(T##_set_iterator_t iter){	\
	if (!iter.current) return iter;	\
	if (iter.current->next){	\
		iter.current = iter.current->next;	\
		return iter;	\
	}	\
	for (iter.bi++; iter.bi < iter.set->bcap; iter.bi++){	\
		if (iter.set->buckets[iter.bi]){	\
			iter.current = iter.set->buckets[iter.bi];	\
			return iter;	\
		}	\
	}	\
	iter.current = 0;	\
	return iter;	\
}	\
void T##_set_free(T##_set_t* set){	\
	T##_set_entry_t* prev = 0;	\
	for (T##_set_iterator_t iter = T##_set_iter(set); iter.current; iter = T##_set_iter_next(iter)){	\
		if (prev) free(prev);	\
		prev = iter.current;	\
	}	\
	if (prev) free(prev);	\
	free(set->buckets);	\
	set->bcap = 0;	\
	set->size = 0;	\
}

#endif
