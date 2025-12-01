

typedef enum { // NOTE: these aren't all available LLVM IR instructions, just those used here
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
			// condition
			// label iftrue
			// label iffalse
		} br;
		struct {
			// label
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

typedef struct llvm_inst_t {
	llvm_inst_enum_t type;
	union {
		struct {
			// type (?)
			// value1
			// value2
		} binop;
		struct {
			// type
			// ptr value
		} load;
		struct {
			// value
			// ptr value
		} store;
		struct {
			// cond (eq, ne, ugt, ...)
			// type (?)
			// value1
			// value2
		} icmp;
		struct {
			// type
			// list of [value, label] pairs
		} phi;
		struct {
			// type
			// func ptr
			// function args
		} call;
	};
} llvm_inst_t;


typedef struct llvm_basic_block_t {
	// list of [%register = instruction] pairs
	llvm_term_inst_t term_inst;
} llvm_basic_block_t;


