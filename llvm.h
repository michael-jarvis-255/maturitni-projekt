#ifndef _LLVM_H_
#define _LLVM_H_

#include "list.h"
#include "hashmap.h"
#include "ast.h"

// using structs for strong typing
typedef struct{ unsigned int idx; } llvm_reg_t;
typedef struct{ unsigned int idx; } llvm_label_t;
#define LLVM_INVALID_REG ((llvm_reg_t){.idx=-1})
#define LLVM_REG_EQ(a, b) (a.idx == b.idx)

typedef enum {
	LLVM_TYPE_INTEGRAL,
	LLVM_TYPE_STRUCT,
	LLVM_TYPE_FLOAT,
	LLVM_TYPE_POINTER,
} llvm_type_enum_t;

typedef struct llvm_type_t {
	llvm_type_enum_t type;
	union {
		unsigned int bitwidth;
		struct {
			unsigned int member_count;
			struct llvm_type_t* member_types;
		} structure;
	};
} llvm_type_t;

#define LLVM_TYPE_I1 ((llvm_type_t){.type=LLVM_TYPE_INTEGRAL,.bitwidth=1})

typedef enum {
	LLVM_VALUE_INT_CONST,
	LLVM_VALUE_DOUBLE_CONST,
	LLVM_VALUE_REG,
	LLVM_VALUE_UNDEF,
	LLVM_VALUE_POISON,
	LLVM_VALUE_NULL_PTR,
	LLVM_VALUE_GLOBAL,
} llvm_value_enum_t;

typedef struct llvm_value_t {
	llvm_value_enum_t type;
	union {
		bignum_t* int_const;
		double double_const;
		llvm_reg_t reg;
		char* global_name;
	};
} llvm_value_t;

typedef struct {
	llvm_type_t type;
	llvm_value_t val;
} llvm_arg_t;

create_list_type_header(llvm_arg, false);

typedef enum { // NOTE: these aren't all available LLVM IR instructions, just those used here
	LLVM_TERM_INST_NULL, // shouldn't appear in resulting code
	LLVM_TERM_INST_RET_VOID,
	LLVM_TERM_INST_RET,
	LLVM_TERM_INST_JMP, // 'br' instruction without a condition
	LLVM_TERM_INST_BR,
} llvm_term_inst_enum_t;

typedef struct llvm_term_inst_t {
	llvm_term_inst_enum_t type;
	union {
		struct {
			llvm_type_t type;
			llvm_value_t value;
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
	LLVM_INST_FADD,
	LLVM_INST_FSUB,
	LLVM_INST_FMUL,
	LLVM_INST_FDIV,
	LLVM_INST_FREM,

	// memory access
	LLVM_INST_LOAD,
	LLVM_INST_STORE,

	// conversion operators
	LLVM_INST_ZEXT,
	LLVM_INST_SEXT,
	LLVM_INST_TRUNC,
	LLVM_INST_PTR_TO_INT,
	LLVM_INST_INT_TO_PTR,
	LLVM_INST_UINT_TO_FLOAT,
	LLVM_INST_SINT_TO_FLOAT,
	LLVM_INST_FLOAT_TO_UINT,
	LLVM_INST_FLOAT_TO_SINT,
	LLVM_INST_FPEXT,
	LLVM_INST_FPTRUNC,

	// other
	LLVM_INST_ICMP,
	LLVM_INST_FCMP,
	LLVM_INST_PHI,
	LLVM_INST_CALL,
	LLVM_INST_VOID_CALL,
	LLVM_INST_ALLOCA,
	LLVM_INST_EXTRACT_VALUE,
	LLVM_INST_GET_ELEMENT_PTR,
	LLVM_INST_FNEG, // note: not a real llvm instruction
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

typedef enum {
	LLVM_FCMP_TRUE,
	LLVM_FCMP_FALSE,
	LLVM_FCMP_OEQ,
	LLVM_FCMP_UEQ,
	LLVM_FCMP_ONE,
	LLVM_FCMP_UNE,
	LLVM_FCMP_OGT,
	LLVM_FCMP_UGT,
	LLVM_FCMP_OGE,
	LLVM_FCMP_UGE,
	LLVM_FCMP_OLT,
	LLVM_FCMP_ULT,
	LLVM_FCMP_OLE,
	LLVM_FCMP_ULE,
} llvm_fcmp_enum_t;

#define LLVM_PHI_MAX 3
typedef struct llvm_inst_t {
	llvm_inst_enum_t type;
	union {
		struct {
			llvm_type_t type;
			llvm_value_t first, second;
		} binop;
		struct {
			llvm_type_t type;
			llvm_value_t ptr;
		} load;
		struct {
			llvm_type_t type;
			llvm_value_t value;
			llvm_value_t ptr;
		} store;
		struct {
			llvm_icmp_enum_t cond;
			llvm_type_t type;
			llvm_value_t op1, op2;
		} icmp;
		struct {
			llvm_fcmp_enum_t cond;
			llvm_type_t type;
			llvm_value_t op1, op2;
		} fcmp;
		struct {
			llvm_type_t type;
			unsigned int count;
			llvm_label_t labels[LLVM_PHI_MAX];
			llvm_value_t values[LLVM_PHI_MAX];
		} phi;
		struct {
			char* name;
			llvm_type_t rettype;
			llvm_arg_list_t args;
		} call;
		struct {
			llvm_type_t from, to;
			llvm_value_t value;
		} conversion;
		struct {
			llvm_type_t type;
		} alloca;
		struct {
			llvm_type_t aggregate_type;
			llvm_value_t value;
			unsigned int member_idx;
		} extract;
		struct {
			llvm_type_t aggregate_type;
			llvm_value_t ptr;
			unsigned int member_idx;
		} getelementptr;
		struct {
			llvm_type_t type;
			llvm_value_t value;
		} fneg;
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
	char* name;
	llvm_basic_block_list_t blocks; // entry block is always block 0
	bool has_return;
	llvm_type_t rettype;
	unsigned int arg_count;
	llvm_type_t* args;
} llvm_function_t;

typedef struct llvm_global_def_t {
	char* name;
	llvm_type_t type;
	llvm_value_t init_val;
} llvm_global_def_t;

typedef struct llvm_program_t {
	char* source_path;
	unsigned int function_count;
	unsigned int global_count;
	llvm_function_t* functions;
	llvm_global_def_t* globals;
} llvm_program_t;


void llvm_program_to_stream(const llvm_program_t program, FILE* stream);
char* llvm_program_to_string(const llvm_program_t program);

void free_llvm_function(const llvm_function_t func);
void free_llvm_global_def(llvm_global_def_t global);
void free_llvm_program(llvm_program_t program);

#endif
