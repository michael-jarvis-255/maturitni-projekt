#include "list.h"
#include "hashmap.h"
#include "ast.h"

// using structs for strong typing
typedef struct{ unsigned int idx; } llvm_reg_t;
typedef struct{ unsigned int idx; } llvm_label_t;
#define LLVM_INVALID_REG ((llvm_reg_t){.idx=-1})
#define LLVM_REG_EQ(a, b) (a.idx == b.idx)

typedef const ast_variable_t* ast_variable_ptr;
create_hashmap_type_header(ast_variable_ptr, llvm_reg_t, var2reg_map);

typedef enum {
	LLVM_TYPE_INTEGRAL,
	LLVM_TYPE_STRUCT,
	LLVM_TYPE_FLOAT,
} llvm_type_enum_t;

typedef struct llvm_type_t {
	llvm_type_enum_t type;
	union {
		unsigned int int_bitwidth;
	};
} llvm_type_t;

typedef enum {
	LLVM_VALUE_INT_CONST,
	LLVM_VALUE_REG,
	LLVM_VALUE_UNDEF,
	LLVM_VALUE_POISON, // TODO: where to use 'undef' and where 'poison'?
} llvm_value_enum_t;

typedef struct llvm_value_t {
	llvm_value_enum_t type;
	union {
		long int_const;
		llvm_reg_t reg;
	};
} llvm_value_t;

typedef struct llvm_typed_value_t {
	llvm_value_enum_t type;
	union {
		long int_const;
		struct {
			llvm_reg_t reg;
			ast_datatype_t* ast_type;
		} typed_reg;
	};
} llvm_typed_value_t;

typedef enum { // NOTE: these aren't all available LLVM IR instructions, just those used here
	LLVM_TERM_INST_NULL, // shouldn't appear in resulting code
	LLVM_TERM_INST_RET,
	LLVM_TERM_INST_JMP, // 'br' instruction without a condition
	LLVM_TERM_INST_BR,
} llvm_term_inst_enum_t;

typedef struct llvm_term_inst_t {
	llvm_term_inst_enum_t type;
	union {
		struct {
			// value
		} ret;
		struct {
			llvm_value_t cond;
			llvm_label_t iftrue, iffalse;
		} br;
		struct {
			llvm_label_t target;
		} jmp;
	};
} llvm_term_inst_t;

typedef enum {
	// binary operations
	LLVM_INST_ADD,
	LLVM_INST_SUB,
	LLVM_INST_MUL,
	LLVM_INST_UDIV,
	LLVM_INST_SDIV,
	LLVM_INST_UREM,
	LLVM_INST_SREM,
	LLVM_INST_SHL,
	LLVM_INST_LSHR,
	LLVM_INST_ASHR,
	LLVM_INST_AND,
	LLVM_INST_OR,
	LLVM_INST_XOR,

	// memory access
	LLVM_INST_LOAD,
	LLVM_INST_STORE,

	// other
	LLVM_INST_ICMP,
	LLVM_INST_PHI,
	LLVM_INST_CALL,
} llvm_inst_enum_t;

typedef enum {
	LLVM_ICMP_EQ,
	LLVM_ICMP_NE,
	LLVM_ICMP_UGT,
	LLVM_ICMP_UGE,
	LLVM_ICMP_ULT,
	LLVM_ICMP_ULE,
	LLVM_ICMP_SGT,
	LLVM_ICMP_SGE,
	LLVM_ICMP_SLT,
	LLVM_ICMP_SLE,
} llvm_icmp_enum_t;

#define LLVM_PHI_MAX 3
typedef struct llvm_inst_t {
	llvm_inst_enum_t type;
	union {
		struct {
			llvm_type_t type;
			llvm_value_t first, second;
		} binop;
		struct {
			// ptr value
		} load;
		struct {
			// value
			// ptr value
		} store;
		struct {
			llvm_icmp_enum_t cond;
			llvm_type_t type;
			llvm_value_t op1, op2;
		} icmp;
		struct {
			llvm_type_t type;
			unsigned int count;
			llvm_label_t labels[LLVM_PHI_MAX];
			llvm_value_t values[LLVM_PHI_MAX];
		} phi;
		struct {
			// func ptr
			// function args
		} call;
	};
} llvm_inst_t;
create_list_type_header(llvm_inst, false);

typedef struct llvm_basic_block_t {
	unsigned int regbase;
	llvm_inst_list_t instructions; // i-th instrcution assigns register (regbase + i)
	llvm_term_inst_t term_inst;
} llvm_basic_block_t;
create_list_type_header(llvm_basic_block, false);

typedef struct llvm_function_t {
	const char* name;
	llvm_basic_block_list_t blocks; // entry block is always block 0
	// return type
	// argument list
} llvm_function_t;


llvm_function_t llvm_emit_ast_func(ast_func_t func);
