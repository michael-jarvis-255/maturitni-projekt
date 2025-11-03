#ifndef _HASHMAP_H_
#define _HASHMAP_H_

typedef struct hashmap_entry_t {
	const char* key;
	void* value;
	struct hashmap_entry_t* next;
} hashmap_entry_t;

typedef struct {
	unsigned int size;
	unsigned int bcap;
	hashmap_entry_t** buckets;
} hashmap_t;

void hashmap_insert(hashmap_t* map, const char* key, void* value);
hashmap_t create_hashmap();
void* hashmap_get(const hashmap_t* map, const char* key);
void hashmap_remove(hashmap_t* map, const char* key);
void hashmap_insert(hashmap_t* map, const char* key, void* value);

#endif
