#ifndef _HASHMAP_H_
#define _HASHMAP_H_
#include <stdbool.h>
#include <stdlib.h>

// Tk = key type
// Tv = value type
// T = resulting hashmap type name
#define create_hashmap_type_header(Tk, Tv, T)	\
typedef struct T##_entry_t {	\
	Tk key;	\
	Tv value;	\
	struct T##_entry_t* next;	\
} T##_entry_t;	\
	\
typedef struct {	\
	unsigned int size;	\
	unsigned int bcap;	\
	T##_entry_t** buckets;	\
} T##_t;	\
	\
typedef struct {	\
	T##_entry_t* current;	\
	const T##_t* map;	\
	unsigned int bi;	\
} T##_iterator_t;	\
	\
void T##_insert(T##_t* map, Tk key, Tv value);	\
void T##_set(T##_t* map, Tk key, Tv value);	\
T##_t create_##T();	\
Tv T##_get(const T##_t* map, Tk key, Tv default_);	\
void T##_remove(T##_t* map, Tk key);	\
void T##_insert(T##_t* map, Tk key, Tv value);	\
T##_entry_t* T##_to_linked_list(T##_t* map);	\
T##_iterator_t T##_iter(const T##_t* map);	\
T##_iterator_t T##_iter_next(T##_iterator_t iter);	\
T##_t T##_copy(T##_t* map);	\
void T##_free(T##_t* map)

#define create_hashmap_type_impl(Tk, Tv, T)	\
T##_t create_##T(){	\
	T##_t map;	\
	map.size = 0;	\
	map.bcap = 16;	\
	map.buckets = calloc(map.bcap, sizeof(T##_entry_t*));	\
	return map;	\
}	\
	\
static T##_entry_t* T##_find_entry(const T##_t* map, Tk key){	\
	unsigned long h = Tk##_hash(key) % map->bcap;	\
	T##_entry_t* entry = map->buckets[h];	\
	while (entry){	\
		if (Tk##_cmp(entry->key, key) == 0) return entry;	\
		entry = entry->next;	\
	}	\
	return 0;	\
}	\
	\
Tv T##_get(const T##_t* map, Tk key, Tv default_){	\
	T##_entry_t* entry = T##_find_entry(map, key);	\
	if (entry) return entry->value;	\
	return default_;	\
}	\
	\
void T##_remove(T##_t* map, Tk key){	\
	unsigned long h = Tk##_hash(key) % map->bcap;	\
	T##_entry_t* entry = map->buckets[h];	\
	if (Tk##_cmp(entry->key, key) == 0){	\
		map->buckets[h] = 0;	\
		map->size--;	\
		return;	\
	}	\
	while (entry->next){	\
		if (Tk##_cmp(entry->next->key, key) == 0){	\
			void* to_free = entry->next;	\
			entry->next = entry->next->next;	\
			free(to_free);	\
			map->size--;	\
			return;	\
		}	\
	}	\
}	\
	\
static void T##_grow(T##_t* map){	\
	unsigned int old_bcap = map->bcap;	\
	T##_entry_t** old_buckets = map->buckets;	\
	map->size = 0;	\
	map->bcap *= 4;	\
	map->buckets = calloc(map->bcap, sizeof(T##_entry_t**));	\
	\
	for (unsigned int h=0; h<old_bcap; h++){	\
		T##_entry_t* head = old_buckets[h];	\
		while (head){	\
			T##_entry_t* entry = head;	\
			head = entry->next;	\
				\
			unsigned long newh = Tk##_hash(entry->key) % map->bcap;	\
			entry->next = map->buckets[newh];	\
			map->buckets[newh] = entry;	\
		}	\
	}	\
	free(old_buckets);	\
}	\
	\
/* NOTE: inserting a key that already exists must create a second entry with duplicate key. */	\
/* It is undefined which is returned with T##_get(). Both must be iterated over when iterating. */	\
void T##_insert(T##_t* map, Tk key, Tv value){	\
	if (map->size > map->bcap*2)	\
		T##_grow(map);	\
		\
	unsigned long h = Tk##_hash(key) % map->bcap;	\
	T##_entry_t* entry = malloc(sizeof(T##_entry_t));	\
	entry->key = key;	\
	entry->value = value;	\
	entry->next = map->buckets[h];	\
	map->buckets[h] = entry;	\
	map->size++;	\
}	\
	\
void T##_set(T##_t* map, Tk key, Tv value){	\
	T##_entry_t* entry = T##_find_entry(map, key);	\
	if (entry == 0){	\
		T##_insert(map, key, value);	\
		return;	\
	}	\
	entry->value = value;	\
}	\
	\
T##_entry_t* T##_to_linked_list(T##_t* map){	\
	T##_entry_t* first = 0;	\
	T##_entry_t* last = 0;	\
	for (unsigned int i=0; i < map->bcap; i++){	\
		if (map->buckets[i]){	\
			if (!first){	\
				first = map->buckets[i];	\
				last = first;	\
			}else{	\
				last->next = map->buckets[i];	\
			}	\
			map->buckets[i] = 0;	\
			while (last->next) last = last->next;	\
		}	\
	}	\
	map->size = 0;	\
	map->bcap = 0;	\
	free(map->buckets);	\
	map->buckets = 0;	\
		\
	return first;	\
}	\
	\
T##_iterator_t T##_iter(const T##_t* map){	\
	for (unsigned int i=0; i<map->bcap; i++){	\
		if (map->buckets[i]){	\
			return (T##_iterator_t){	\
				.current=map->buckets[i],	\
				.map=map,	\
				.bi=i,	\
			};	\
		}	\
	}	\
	return (T##_iterator_t){	\
		.current=0,	\
		.map=map,	\
		.bi=-1,	\
	};	\
}	\
T##_iterator_t T##_iter_next(T##_iterator_t iter){	\
	if (!iter.current) return iter;	\
	if (iter.current->next){	\
		iter.current = iter.current->next;	\
		return iter;	\
	}	\
	for (iter.bi++; iter.bi < iter.map->bcap; iter.bi++){	\
		if (iter.map->buckets[iter.bi]){	\
			iter.current = iter.map->buckets[iter.bi];	\
			return iter;	\
		}	\
	}	\
	iter.current = 0;	\
	return iter;	\
}	\
T##_t T##_copy(T##_t* map){	\
	T##_t copy = create_##T();	\
	copy.buckets = realloc(copy.buckets, map->bcap * sizeof(T##_entry_t**));	\
	copy.bcap = map->bcap;	\
	for (T##_iterator_t iter = T##_iter(map); iter.current; iter = T##_iter_next(iter)){	\
		T##_insert(&copy, iter.current->key, iter.current->value);	\
	}	\
	return copy;	\
}	\
void T##_free(T##_t* map){	\
	T##_entry_t* prev = 0;	\
	for (T##_iterator_t iter = T##_iter(map); iter.current; iter = T##_iter_next(iter)){	\
		if (prev) free(prev);	\
		prev = iter.current;	\
	}	\
	if (prev) free(prev);	\
	free(map->buckets);	\
	map->bcap = 0;	\
	map->size = 0;	\
}


#endif
