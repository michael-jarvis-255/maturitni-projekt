#include "main.h"
#include "scope.h"
#include "../lib/hash.h"
#include "../message.h"
#include <stdlib.h>

create_hashmap_type_impl(str, ast_id_t*, str2id_hashmap)

typedef struct scope_t {
	struct scope_t* parent;
	str2id_hashmap_t hashmap;
} scope_t;


scope_t* create_scope(scope_t* parent){
	scope_t* scope = malloc(sizeof(scope_t));
	scope->parent = parent;
	scope->hashmap = create_str2id_hashmap();
	return scope;
}

ast_id_t* scope_get_local(const scope_t* scope, const char* name){
	return str2id_hashmap_get(&scope->hashmap, name, 0);
}

ast_id_t* scope_get(const scope_t* scope, const char* name){
	while (scope){
		ast_id_t* value = scope_get_local(scope, name);
		if (value) return value;
		scope = scope->parent;
	}
	return 0;
}

static inline loc_t loc_from_ast_id(ast_id_t* id){
	switch (id->type){
		case AST_ID_VAR: return id->var.declare_loc;
		case AST_ID_FUNC: return id->func.declare_loc;
		case AST_ID_TYPE: return id->type_.declare_loc;
		case AST_ID_GLOBAL: return id->global.var.declare_loc;
	}
	INTERNAL_ERROR();
}

void scope_insert(scope_t* scope, const char* name, ast_id_t* value){
	ast_id_t* exists = scope_get(scope, name);
	if (exists){ // TODO: is this the correct place to catch this error?
		printf_error(loc_from_ast_id(value), "name '%s' is already declared", name);
		if (!loc_eq(loc_from_ast_id(exists), (loc_t){0})){ // if not built-in type
			print_info(loc_from_ast_id(exists), "declared here");
		}
		free_ast_id(value);
		return;
	}
	str2id_hashmap_insert(&scope->hashmap, name, value);
}

void scope_force_insert(scope_t* scope, const char* name, ast_id_t* value){
	str2id_hashmap_insert(&scope->hashmap, name, value);
}

void free_scope(scope_t* scope){
	for (str2id_hashmap_iterator_t iter = str2id_hashmap_iter(&scope->hashmap); iter.current; iter = str2id_hashmap_iter_next(iter)){
		free_ast_id(iter.current->value);
	}
	free_str2id_hashmap(&scope->hashmap);
	free(scope);
}

scope_t* scope_get_parent(const scope_t* scope){
	return scope->parent;
}

void scope_remove(scope_t* scope, const char* name){
	str2id_hashmap_remove(&scope->hashmap, name);
}

scope_iterator_t scope_iter(const scope_t* scope){ return str2id_hashmap_iter(&scope->hashmap); }
scope_iterator_t scope_iter_next(scope_iterator_t iter){ return str2id_hashmap_iter_next(iter); }
