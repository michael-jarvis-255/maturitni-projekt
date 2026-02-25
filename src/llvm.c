#include "llvm.h"
#include "string.h"
#include "ast2llvm.h"
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
			tprintf(t, "i%u", type.bitwidth);
			break;
		case LLVM_TYPE_FLOAT:
			switch (type.bitwidth){
				case 16: tprint(t, "half"); break;
				case 32: tprint(t, "float"); break;
				case 64: tprint(t, "double"); break;
				case 80: tprint(t, "x86_fp80"); break;
				case 128: tprint(t, "fp128"); break;
				default:
					INTERNAL_ERROR();
			}
			break;
		case LLVM_TYPE_STRUCT:
			tprint(t, "{ ");
			for (unsigned int i=0; i < type.structure.member_count; i++){
				llvm_type_to_target(type.structure.member_types[i], t);
				if (i+1 < type.structure.member_count)
					tprint(t, ", ");
			}
			tprint(t, " }");
			break;
		case LLVM_TYPE_POINTER:
			tprintf(t, "ptr");
			break;
	}
}

static void llvm_value_to_target(const llvm_value_t val, print_target_t* t){
	switch (val.type){
		case LLVM_VALUE_NULL_PTR: tprint(t, "null"); break;
		case LLVM_VALUE_UNDEF: tprint(t, "undef"); break;
		case LLVM_VALUE_POISON: tprint(t, "poison"); break;
		case LLVM_VALUE_REG: tprintf(t, "%%%u", val.reg.idx); break;
		case LLVM_VALUE_DOUBLE_CONST: tprintf(t, "%#016lx", *(unsigned long*)&val.double_const); break;
		case LLVM_VALUE_GLOBAL: tprintf(t, "@%s", val.global_name); break;
		case LLVM_VALUE_INT_CONST:
		{
			char* str = bignum_to_string(val.int_const);
			tprintf(t, "%s", str);
			free(str);
			break;
		}
	}
}

static void llvm_inst_body_to_target(const llvm_inst_t inst, print_target_t* t){
	switch (inst.type){
		// binary operations
		case LLVM_INST_ADD: tprint(t, "add "); goto binary_op;
		case LLVM_INST_SUB: tprint(t, "sub "); goto binary_op;
		case LLVM_INST_MUL: tprint(t, "mul "); goto binary_op;
		case LLVM_INST_UDIV: tprint(t, "udiv "); goto binary_op;
		case LLVM_INST_SDIV: tprint(t, "sdiv "); goto binary_op;
		case LLVM_INST_UREM: tprint(t, "urem "); goto binary_op;
		case LLVM_INST_SREM: tprint(t, "srem "); goto binary_op;
		case LLVM_INST_SHL: tprint(t, "shl "); goto binary_op;
		case LLVM_INST_LSHR: tprint(t, "lshr "); goto binary_op;
		case LLVM_INST_ASHR: tprint(t, "ashr "); goto binary_op;
		case LLVM_INST_AND: tprint(t, "and "); goto binary_op;
		case LLVM_INST_OR: tprint(t, "or "); goto binary_op;
		case LLVM_INST_XOR: tprint(t, "xor "); goto binary_op;
		case LLVM_INST_FADD: tprint(t, "fadd "); goto binary_op;
		case LLVM_INST_FSUB: tprint(t, "fsub "); goto binary_op;
		case LLVM_INST_FMUL: tprint(t, "fmul "); goto binary_op;
		case LLVM_INST_FDIV: tprint(t, "fdiv "); goto binary_op;
		case LLVM_INST_FREM: tprint(t, "frem "); goto binary_op;
		binary_op:
			llvm_type_to_target(inst.binop.type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.binop.first, t);
			tprint(t, ", ");
			llvm_value_to_target(inst.binop.second, t);
			return;
		
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

		// conversion operators
		case LLVM_INST_ZEXT: tprint(t, "zext "); goto conversion_op;
		case LLVM_INST_SEXT: tprint(t, "sext "); goto conversion_op;
		case LLVM_INST_TRUNC: tprint(t, "trunc "); goto conversion_op;
		case LLVM_INST_PTR_TO_INT: tprint(t, "ptrtoint "); goto conversion_op;
		case LLVM_INST_INT_TO_PTR: tprint(t, "inttoptr "); goto conversion_op;
		case LLVM_INST_UINT_TO_FLOAT: tprint(t, "uitofp "); goto conversion_op;
		case LLVM_INST_SINT_TO_FLOAT: tprint(t, "sitofp "); goto conversion_op;
		case LLVM_INST_FLOAT_TO_UINT: tprint(t, "fptoui "); goto conversion_op; // TODO: use saturating intrinsic instead?
		case LLVM_INST_FLOAT_TO_SINT: tprint(t, "fptosi "); goto conversion_op; // TODO: use saturating intrinsic instead?
		case LLVM_INST_FPEXT: tprint(t, "fpext "); goto conversion_op;
		case LLVM_INST_FPTRUNC: tprint(t, "fptrunc "); goto conversion_op;
		conversion_op:
			llvm_type_to_target(inst.conversion.from, t);
			tprint(t, " ");
			llvm_value_to_target(inst.conversion.value, t);
			tprint(t, " to ");
			llvm_type_to_target(inst.conversion.to, t);
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
		case LLVM_INST_FCMP:
			tprint(t, "fcmp ");
			switch (inst.fcmp.cond){
				case LLVM_FCMP_TRUE: tprint(t, "true "); break;
				case LLVM_FCMP_FALSE: tprint(t, "false "); break;
				case LLVM_FCMP_OEQ: tprint(t, "oeq "); break;
				case LLVM_FCMP_UEQ: tprint(t, "ueq "); break;
				case LLVM_FCMP_ONE: tprint(t, "one "); break;
				case LLVM_FCMP_UNE: tprint(t, "une "); break;
				case LLVM_FCMP_OGT: tprint(t, "ogt "); break;
				case LLVM_FCMP_UGT: tprint(t, "ugt "); break;
				case LLVM_FCMP_OGE: tprint(t, "oge "); break;
				case LLVM_FCMP_UGE: tprint(t, "uge "); break;
				case LLVM_FCMP_OLT: tprint(t, "olt "); break;
				case LLVM_FCMP_ULT: tprint(t, "ult "); break;
				case LLVM_FCMP_OLE: tprint(t, "ole "); break;
				case LLVM_FCMP_ULE: tprint(t, "ule "); break;
			}
			llvm_type_to_target(inst.fcmp.type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.fcmp.op1, t);
			tprint(t, ", ");
			llvm_value_to_target(inst.fcmp.op2, t);
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
		case LLVM_INST_VOID_CALL:
			tprint(t, "call void ");
			goto call_end;
		case LLVM_INST_CALL:
			tprint(t, "call ");
			llvm_type_to_target(inst.call.rettype, t);
		call_end:
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
		case LLVM_INST_ALLOCA:
			tprint(t, "alloca ");
			llvm_type_to_target(inst.alloca.type, t);
			return;
		case LLVM_INST_EXTRACT_VALUE:
			tprint(t, "extractvalue ");
			llvm_type_to_target(inst.extract.aggregate_type, t);
			tprint(t, " ");
			llvm_value_to_target(inst.extract.value, t);
			tprintf(t, ", %u", inst.extract.member_idx);
			return;
		case LLVM_INST_GET_ELEMENT_PTR:
			tprint(t, "getelementptr ");
			llvm_type_to_target(inst.getelementptr.aggregate_type, t);
			tprint(t, ", ptr ");
			llvm_value_to_target(inst.getelementptr.ptr, t);
			tprintf(t, ", i32 0, i32 %u", inst.getelementptr.member_idx);
			return;
		case LLVM_INST_FNEG:
			tprint(t, "fsub ");
			llvm_type_to_target(inst.fneg.type, t);
			tprint(t, " 0.0, ");
			llvm_value_to_target(inst.fneg.value, t);
			return;
	}
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
		if (block->instructions.data[i].type != LLVM_INST_STORE
		 && block->instructions.data[i].type != LLVM_INST_VOID_CALL)
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

static void free_llvm_type(llvm_type_t type){
	switch (type.type){
		case LLVM_TYPE_INTEGRAL:
		case LLVM_TYPE_FLOAT:
		case LLVM_TYPE_POINTER:
			break;
		case LLVM_TYPE_STRUCT:
			for (unsigned int i=0; i<type.structure.member_count; i++){
				free_llvm_type(type.structure.member_types[i]);
			}
			free(type.structure.member_types);
	}
}

static void free_llvm_value(llvm_value_t value){
	switch (value.type){
		case LLVM_VALUE_INT_CONST:
			free_bignum(value.int_const);
			break;
		case LLVM_VALUE_GLOBAL:
			free(value.global_name);
			break;
		case LLVM_VALUE_REG:
		case LLVM_VALUE_POISON:
		case LLVM_VALUE_UNDEF:
		case LLVM_VALUE_DOUBLE_CONST:
		case LLVM_VALUE_NULL_PTR:
			break;
	}
}

static void free_llvm_inst(llvm_inst_t inst){
	switch (inst.type){
		case LLVM_INST_ADD:
		case LLVM_INST_SUB:
		case LLVM_INST_MUL:
		case LLVM_INST_UDIV:
		case LLVM_INST_SDIV:
		case LLVM_INST_UREM:
		case LLVM_INST_SREM:
		case LLVM_INST_SHL:
		case LLVM_INST_LSHR:
		case LLVM_INST_ASHR:
		case LLVM_INST_AND:
		case LLVM_INST_OR:
		case LLVM_INST_XOR:
		case LLVM_INST_FADD:
		case LLVM_INST_FSUB:
		case LLVM_INST_FMUL:
		case LLVM_INST_FDIV:
		case LLVM_INST_FREM:
			free_llvm_type(inst.binop.type);
			free_llvm_value(inst.binop.first);
			free_llvm_value(inst.binop.second);
			break;
		case LLVM_INST_LOAD:
			free_llvm_type(inst.load.type);
			free_llvm_value(inst.load.ptr);
			break;
		case LLVM_INST_STORE:
			free_llvm_type(inst.store.type);
			free_llvm_value(inst.store.value);
			free_llvm_value(inst.store.ptr);
			break;
		case LLVM_INST_ICMP:
			free_llvm_type(inst.icmp.type);
			free_llvm_value(inst.icmp.op1);
			free_llvm_value(inst.icmp.op2);
			break;
		case LLVM_INST_FCMP:
			free_llvm_type(inst.fcmp.type);
			free_llvm_value(inst.fcmp.op1);
			free_llvm_value(inst.fcmp.op2);
			break;
		case LLVM_INST_PHI:
			free_llvm_type(inst.phi.type);
			for (unsigned int i=0; i < inst.phi.count; i++) free_llvm_value(inst.phi.values[i]);
			break;
		case LLVM_INST_SEXT:
		case LLVM_INST_ZEXT:
		case LLVM_INST_TRUNC:
		case LLVM_INST_PTR_TO_INT:
		case LLVM_INST_INT_TO_PTR:
		case LLVM_INST_UINT_TO_FLOAT:
		case LLVM_INST_SINT_TO_FLOAT:
		case LLVM_INST_FLOAT_TO_UINT:
		case LLVM_INST_FLOAT_TO_SINT:
		case LLVM_INST_FPEXT:
		case LLVM_INST_FPTRUNC:
			free_llvm_type(inst.conversion.from);
			free_llvm_type(inst.conversion.to);
			free_llvm_value(inst.conversion.value);
			break;
		case LLVM_INST_ALLOCA:
			free_llvm_type(inst.alloca.type);
			break;
		case LLVM_INST_EXTRACT_VALUE:
			free_llvm_type(inst.extract.aggregate_type);
			free_llvm_value(inst.extract.value);
			break;
		case LLVM_INST_GET_ELEMENT_PTR:
			free_llvm_type(inst.getelementptr.aggregate_type);
			free_llvm_value(inst.getelementptr.ptr);
			break;
		case LLVM_INST_CALL:
			free_llvm_type(inst.call.rettype);
			__attribute__ ((fallthrough));
		case LLVM_INST_VOID_CALL:
			free(inst.call.name);
			for (unsigned int i = 0; i < inst.call.args.len; i++)
				free_llvm_value(inst.call.args.data[i].val);
			shallow_free_llvm_arg_list(&inst.call.args);
			break;
		case LLVM_INST_FNEG:
			free_llvm_type(inst.fneg.type);
			free_llvm_value(inst.fneg.value);
			break;
	}
}

static void free_llvm_term_inst(llvm_term_inst_t inst){
	switch (inst.type){
		case LLVM_TERM_INST_RET:
			free_llvm_type(inst.ret.type);
			free_llvm_value(inst.ret.value);
			break;
		case LLVM_TERM_INST_BR:
			free_llvm_value(inst.br.cond);
			break;
		case LLVM_TERM_INST_JMP:
		case LLVM_TERM_INST_NULL:
		case LLVM_TERM_INST_RET_VOID:
			break;
	}
}

static void free_llvm_basic_block(llvm_basic_block_t block){
	for (unsigned int i = 0; i < block.instructions.len; i++){
		free_llvm_inst(block.instructions.data[i]);
	}
	shallow_free_llvm_inst_list(&block.instructions);
	free_llvm_term_inst(block.term_inst);
}

static void llvm_global_to_target(llvm_global_def_t global, print_target_t* t){
	tprintf(t, "@%s = external global ", global.name);
	llvm_type_to_target(global.type, t);
	tprint(t, "\n");
	// TODO: emit init value
}

static void llvm_program_to_target(const llvm_program_t program, print_target_t* t){
	tprintf(t, "source_filename = \"%s\"\n\n", program.source_path);
	for (unsigned int i=0; i<program.global_count; i++) llvm_global_to_target(program.globals[i], t);
	for (unsigned int i=0; i<program.function_count; i++) llvm_func_to_target(&program.functions[i], t);
}

void llvm_program_to_stream(const llvm_program_t program, FILE* stream){
	llvm_program_to_target(program, &(print_target_t){.target=PRINT_TARGET_STREAM, .stream=stream});
}

char* llvm_program_to_string(const llvm_program_t program){
	print_target_t t = {.target=PRINT_TARGET_STRING, .string=create_char_list()};
	llvm_program_to_target(program, &t);
	char_list_append(&t.string, '\0');
	return t.string.data;
}


void free_llvm_function(llvm_function_t func){
	free(func.name);
	for (unsigned int i = 0; i < func.blocks.len; i++){
		free_llvm_basic_block(func.blocks.data[i]);
	}
	shallow_free_llvm_basic_block_list(&func.blocks);
	free(func.args);
}

void free_llvm_global_def(llvm_global_def_t global){
	free(global.name);
	free_llvm_type(global.type);
	free_llvm_value(global.init_val);
}

void free_llvm_program(llvm_program_t program){
	free(program.source_path);
	for (unsigned int i=0; i<program.function_count; i++) free_llvm_function(program.functions[i]);
	for (unsigned int i=0; i<program.global_count; i++) free_llvm_global_def(program.globals[i]);
	free(program.functions);
	free(program.globals);
}
