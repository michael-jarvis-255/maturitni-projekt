#ifndef _SCOPE_H_
#define _SCOPE_H_
#include "../lib/hashmap.h"

struct ast_id_t; typedef struct ast_id_t ast_id_t;
struct scope_t; typedef struct scope_t scope_t;
typedef const char* str;
create_hashmap_type_header(str, ast_id_t*, str2id_hashmap);

scope_t* create_scope(scope_t* parent);
ast_id_t* scope_get(const scope_t* scope, const char* name);
void scope_insert(scope_t* scope, const char* name, ast_id_t* value);
void scope_force_insert(scope_t* scope, const char* name, ast_id_t* value);
void free_scope(scope_t* scope);
scope_t* scope_get_parent(const scope_t* scope);

typedef str2id_hashmap_iterator_t scope_iterator_t;
scope_iterator_t scope_iter(const scope_t* scope);
scope_iterator_t scope_iter_next(scope_iterator_t iter);

#endif
