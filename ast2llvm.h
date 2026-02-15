#include "llvm.h"

llvm_function_t ast2llvm_emit_func(ast_func_t func);
llvm_type_t ast_type_to_llvm_type(const ast_datatype_t* ast_type);
