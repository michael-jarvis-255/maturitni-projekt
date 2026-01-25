#include "llvm.h"
#include "string.h"
#include <stdarg.h>

typedef char char_t;
create_list_type_header(char, false);
create_list_type_impl(char, false)
create_list_type_impl(llvm_arg, false)

typedef enum {
	PRINT_TARGET_STRING,
	PRINT_TARGET_STREAM,
} print_target_enum_t;

typedef struct {
	print_target_enum_t target;
	union {
		FILE* stream;
		char_list_t string;
	};
} print_target_t;

static void tprint(print_target_t* t, const char* s){
	switch (t->target){
		case PRINT_TARGET_STREAM:
			fwrite(s, 1, strlen(s), t->stream);
			return;
		case PRINT_TARGET_STRING:
			char_list_extend(&t->string, strlen(s), s);
			return;
	}
}

__attribute__ ((format (printf, 2, 3))) static void tprintf(print_target_t* t, const char* format, ...){
	va_list args;
	va_start(args, format);
	switch (t->target){
		case PRINT_TARGET_STREAM:
			vfprintf(t->stream, format, args);
			break;
		case PRINT_TARGET_STRING:
			{
				char buf[512];
				vsnprintf(buf, sizeof(buf), format, args);
				tprint(t, buf);
				break;
			}
	}
	va_end(args);
}

static void llvm_type_to_target(const llvm_type_t type, print_target_t* t){
	switch (type.type){
		case LLVM_TYPE_INTEGRAL:
			tprintf(t, "i%u", type.int_bitwidth);
			break;
		case LLVM_TYPE_FLOAT:
			tprintf(t, "f%u", type.int_bitwidth);
			break;
		case LLVM_TYPE_STRUCT:
			tprintf(t, "???");
			break;
		case LLVM_TYPE_POINTER:
			tprintf(t, "ptr");
			break;
	}
}

static void llvm_value_to_target(const llvm_value_t val, print_target_t* t){
	switch (val.type){
		case LLVM_VALUE_UNDEF:
			tprint(t, "undef");
			break;
		case LLVM_VALUE_POISON:
			tprint(t, "poison");
			break;
		case LLVM_VALUE_REG:
			tprintf(t, "%%%u", val.reg.idx);
			break;
		case LLVM_VALUE_INT_CONST:
			tprintf(t, "%lu", val.int_const);
			break;
	}
}

static void llvm_inst_body_to_target(const llvm_inst_t inst, print_target_t* t){
	switch (inst.type){
		// binary operations
		case LLVM_INST_ADD:
			tprint(t, "add ");
			goto binary_op;
		case LLVM_INST_SUB:
			tprint(t, "sub ");
			goto binary_op;
		case LLVM_INST_MUL:
			tprint(t, "mul ");
			goto binary_op;
		case LLVM_INST_UDIV:
			tprint(t, "udiv ");
			goto binary_op;
		case LLVM_INST_SDIV:
			tprint(t, "sdiv ");
			goto binary_op;
		case LLVM_INST_UREM:
			tprint(t, "urem ");
			goto binary_op;
		case LLVM_INST_SREM:
			tprint(t, "srem ");
			goto binary_op;
		case LLVM_INST_SHL:
			tprint(t, "shl ");
			goto binary_op;
		case LLVM_INST_LSHR:
			tprint(t, "lshr ");
			goto binary_op;
		case LLVM_INST_ASHR:
			tprint(t, "ashr ");
			goto binary_op;
		case LLVM_INST_AND:
			tprint(t, "and ");
			goto binary_op;
		case LLVM_INST_OR:
			tprint(t, "or ");
			goto binary_op;
		case LLVM_INST_XOR:
			tprint(t, "xor ");
			goto binary_op;
		
		// memory access
		case LLVM_INST_LOAD:
			tprint(t, "load ");
			llvm_type_to_target(inst.load.type, t);
			tprint(t, ", ptr ");
			llvm_value_to_target(inst.load.ptr, t);
			return;
		case LLVM_INST_STORE:
			tprint(t, "store ");
			llvm_type_to_target(inst.store.type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.store.value, t);
			tprint(t, ", ptr ");
			llvm_value_to_target(inst.store.ptr, t);
			return;
		
		// other
		case LLVM_INST_ICMP:
			tprint(t, "icmp ");
			switch (inst.icmp.cond){
				case LLVM_ICMP_EQ:	tprint(t, "eq " ); break;
				case LLVM_ICMP_NE:	tprint(t, "ne " ); break;
				case LLVM_ICMP_UGT:	tprint(t, "ugt "); break;
				case LLVM_ICMP_UGE:	tprint(t, "uge "); break;
				case LLVM_ICMP_ULT:	tprint(t, "ult "); break;
				case LLVM_ICMP_ULE:	tprint(t, "ule "); break;
				case LLVM_ICMP_SGT:	tprint(t, "sgt "); break;
				case LLVM_ICMP_SGE:	tprint(t, "sge "); break;
				case LLVM_ICMP_SLT:	tprint(t, "slt "); break;
				case LLVM_ICMP_SLE:	tprint(t, "sle "); break;
			}
			llvm_type_to_target(inst.icmp.type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.icmp.op1, t);
			tprint(t, ", ");
			llvm_value_to_target(inst.icmp.op2, t);
			return;
		case LLVM_INST_PHI:
			tprint(t, "phi ");
			llvm_type_to_target(inst.phi.type, t);
			for (unsigned int i=0; i < inst.phi.count; i++){
				tprint(t, " [");
				llvm_value_to_target(inst.phi.values[i], t);
				tprintf(t, ",%%l%u]", inst.phi.labels[i].idx);
				if (i+1 != inst.phi.count) tprint(t, ",");
			}
			return;
		case LLVM_INST_CALL: // TODO functions returning 'void'
			tprint(t, "call ");
			llvm_type_to_target(inst.call.rettype, t);
			tprintf(t, " @%s(", inst.call.name);
			for (unsigned int i=0; i < inst.call.args.len; i++){
				llvm_type_to_target(inst.call.args.data[i].type, t);
				tprint(t, " ");
				llvm_value_to_target(inst.call.args.data[i].val, t);
				if (i+1 < inst.call.args.len){
					tprint(t, ", ");
				}
			}
			tprint(t, ")");
			return;
		case LLVM_INST_ZEXT:
			tprint(t, "zext ");
			llvm_type_to_target(inst.ext.from, t);
			tprint(t, " ");
			llvm_value_to_target(inst.ext.operand, t);
			tprint(t, " to ");
			llvm_type_to_target(inst.ext.to, t);
			return;
		case LLVM_INST_ALLOCA:
			tprint(t, "alloca ");
			llvm_type_to_target(inst.alloca.type, t);
			return;
	}

binary_op:
	llvm_type_to_target(inst.binop.type, t);
	tprint(t, " ");
	llvm_value_to_target(inst.binop.first, t);
	tprint(t, ", ");
	llvm_value_to_target(inst.binop.second, t);
	return;
}

static void llvm_term_inst_to_target(const llvm_term_inst_t inst, print_target_t* t){
	switch (inst.type){
		case LLVM_TERM_INST_NULL:
			tprint(t, "unreachable\n");
			return;
		case LLVM_TERM_INST_RET_VOID:
			tprint(t, "ret void\n");
			return;
		case LLVM_TERM_INST_RET:
			tprint(t, "ret ");
			llvm_type_to_target(inst.ret.type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.ret.value, t);
			tprint(t, "\n");
			return;
		case LLVM_TERM_INST_JMP:
			tprintf(t, "br label %%l%u\n", inst.jmp.target.idx);
			return;
		case LLVM_TERM_INST_BR:
			tprint(t, "br i1 ");
			llvm_value_to_target(inst.br.cond, t);
			tprintf(t, ", label %%l%u, label %%l%u\n", inst.br.iftrue.idx, inst.br.iffalse.idx);
			return;
	}
}

static void llvm_block_body_to_target(const llvm_basic_block_t* block, print_target_t* t){
	for (unsigned int i = 0; i < block->instructions.len; i++){
		if (block->instructions.data[i].type != LLVM_INST_STORE)
			tprintf(t, "    %%%u = ", block->regbase + i);
		else
			tprint(t, "    ");
		llvm_inst_body_to_target(block->instructions.data[i], t);
		tprint(t, "\n");
	}
	tprint(t, "    "); llvm_term_inst_to_target(block->term_inst, t);
}

static void llvm_func_to_target(const llvm_function_t* f, print_target_t* t){
	tprint(t, "define ");
	if (f->has_return)
		llvm_type_to_target(f->rettype, t);
	else
		tprint(t, "void");
	tprintf(t, " @%s(", f->name);
	for (unsigned int i=0; i < f->arg_count; i++){
		llvm_type_to_target(f->args[i], t);
		tprintf(t, " %%%u", i);
		if (i+1 < f->arg_count)
			tprint(t, ", ");
	}
	tprint(t, "){\n");

	for (unsigned int i = 0; i < f->blocks.len; i++){
		tprintf(t, "l%u:\n", i);
		llvm_block_body_to_target(&f->blocks.data[i], t);
	}
	
	tprint(t, "}\n\n");
}

void llvm_func_to_stream(const llvm_function_t* f, FILE* stream){
	llvm_func_to_target(f, &(print_target_t){.target=PRINT_TARGET_STREAM, .stream=stream});
}

char* llvm_func_to_string(const llvm_function_t* f){
	print_target_t t = {.target=PRINT_TARGET_STRING, .string=create_char_list()};
	llvm_func_to_target(f, &t);
	char_list_append(&t.string, '\0');
	return t.string.data;
}
