#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

static unsigned long str_hash(const char* str){
	unsigned long h = 1337;
	while (str[0]){
		h = (h*6364136223846793005) + str[0];
		str++;
	}
	return h;
}

hashmap_t create_hashmap(){
	hashmap_t map;
	map.size = 0;
	map.bcap = 16;
	map.buckets = calloc(map.bcap, sizeof(hashmap_entry_t*));
	return map;
}

static hashmap_entry_t* hashmap_find_entry(const hashmap_t* map, const char* key){
	unsigned long h = str_hash(key) % map->bcap;
	hashmap_entry_t* entry = map->buckets[h];
	while (entry){
		if (strcmp(entry->key, key) == 0) return entry;
		entry = entry->next;
	}
	return 0;
}

void* hashmap_get(const hashmap_t* map, const char* key){
	hashmap_entry_t* entry = hashmap_find_entry(map, key);
	if (entry) return entry->value;
	return 0;
}

void hashmap_remove(hashmap_t* map, const char* key){
	unsigned long h = str_hash(key) % map->bcap;
	hashmap_entry_t* entry = map->buckets[h];
	if (strcmp(entry->key, key) == 0){
		map->buckets[h] = 0;
		map->size--;
		return;
	}
	while (entry->next){
		if (strcmp(entry->next->key, key) == 0){
			void* to_free = entry->next;
			entry->next = entry->next->next;
			free(to_free);
			map->size--;
			return;
		}
	}
}

static void hashmap_grow(hashmap_t* map){
	unsigned int old_bcap = map->bcap;
	hashmap_entry_t** old_buckets = map->buckets;
	map->size = 0;
	map->bcap *= 4;
	map->buckets = calloc(map->bcap, sizeof(hashmap_entry_t*));

	for (unsigned int h=0; h<old_bcap; h++){
		hashmap_entry_t* head = old_buckets[h];
		while (head){
			hashmap_entry_t* entry = head;
			head = entry->next;

			unsigned long newh = str_hash(entry->key) % map->bcap;
			entry->next = map->buckets[newh];
			map->buckets[newh] = entry;
		}
	}
	free(old_buckets);
}

void hashmap_insert(hashmap_t* map, const char* key, void* value){
	if (map->size > map->bcap*2)
		hashmap_grow(map);

	
	unsigned long h = str_hash(key) % map->bcap;
	hashmap_entry_t* entry = malloc(sizeof(hashmap_entry_t));
	entry->key = key;
	entry->value = value;
	entry->next = map->buckets[h];
	map->buckets[h] = entry;
	map->size++;
}

hashmap_entry_t* hashmap_to_linked_list(hashmap_t* map){
	hashmap_entry_t* first = 0;
	hashmap_entry_t* last = 0;
	for (unsigned int i=0; i < map->bcap; i++){
		if (map->buckets[i]){
			if (!first){
				first = map->buckets[i];
				last = first;
			}else{
				last->next = map->buckets[i];
			}
			map->buckets[i] = 0;
			while (last->next) last = last->next;
		}
	}
	map->size = 0;
	map->bcap = 0;
	free(map->buckets);
	map->buckets = 0;
	
	return first;
}
