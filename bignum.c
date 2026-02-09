#include "bignum.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

typedef uint64_t ulong;
typedef uint32_t uint;
typedef struct bignum_t {
	uint* arr;
	uint size;
	bool sign;
} bignum_t;

static inline uint addc(uint a, uint b, bool* carry){
	ulong res = (ulong)a + (ulong)b + (ulong)*carry;
	*carry = res >> 32;
	return res;
}
static inline uint subc(uint a, uint b, bool* carry){
	ulong res = (ulong)a - (ulong)b - (ulong)*carry;
	*carry = (res >> 32) > 0;
	return res;
}
static void bignum_grow(bignum_t* x, unsigned int size){
	if (size <= x->size) return;
	x->arr = reallocarray(x->arr, size, sizeof(uint));
	for (unsigned int i = x->size; i < size; i++){
		x->arr[i] = 0;
	}
	x->size = size;
}
static inline unsigned int bignum_extract(const bignum_t* x, unsigned int idx){
	if (idx >= x->size) return 0;
	return x->arr[idx];
}
static inline void bignum_insert(bignum_t* x, unsigned int idx, unsigned int value){
	if (idx >= x->size) bignum_grow(x, idx+1);
	x->arr[idx] = value;
}

bignum_t* create_bignum(){
	bignum_t* num = malloc(sizeof(bignum_t));
	num->size = 1;
	num->arr = calloc(num->size, sizeof(uint));
	num->sign = false;
	return num;
}

void free_bignum(bignum_t* x){
	free(x->arr);
	free(x);
}

bignum_t* bignum_copy(const bignum_t* bignum){
	bignum_t* num = malloc(sizeof(bignum_t));
	num->size = bignum->size;
	num->arr = memcpy(malloc(sizeof(uint)*bignum->size), bignum->arr, sizeof(uint)*bignum->size);
	num->sign = bignum->sign;
	return num;
}

void bignum_set(bignum_t* dst, const bignum_t* src){
	bignum_grow(dst, src->size);
	for (unsigned int i = 0; i < src->size; i++){
		dst->arr[i] = src->arr[i];
	}
	for (unsigned int i = src->size; i < dst->size; i++){
		dst->arr[i] = 0;
	}
}

void bignum_set_uint(bignum_t* bignum, uint value){
	for (unsigned int i = 0; i < bignum->size; i++){
		bignum->arr[i] = 0;
	}
	bignum->arr[0] = value;
	bignum->sign = false;
}

static inline void bignum_move_and_free(bignum_t* dst, bignum_t* src){
	free(dst->arr);
	*dst = *src;
	free(src);
}
static unsigned int bignum_used_size(const bignum_t* x){
	for (unsigned int j = x->size; j > 0; j--){
		unsigned int i = j-1;
		if (x->arr[i]) return j;
	}
	return 0;
}

// these functions ignore the sign of a and b
static void bignum_unsigned_add_uint(bignum_t* a, uint x){
	bool carry = false;
	unsigned int i = 0;
	do {
		bignum_insert(a, i, addc(bignum_extract(a, i), i ? 0 : x, &carry));
	} while (carry);
}

static void bignum_unsigned_add(bignum_t* a, const bignum_t* b){
	bool carry = false;
	unsigned int i = 0;
	for (; i < b->size; i++){
		bignum_insert(a, i, addc(bignum_extract(a, i), bignum_extract(b, i), &carry));
	}
	if (carry){
		bignum_insert(a, i, addc(bignum_extract(a, i), 0, &carry));
	}
}
static int bignum_unsigned_cmp(const bignum_t* a, const bignum_t* b){ // returns 1 if a > b, 0 if a == b and -1 if a < b
	unsigned int sz = (a->size > b->size) ? a->size : b->size;
	for (unsigned int j = sz; j > 0; j--){
		unsigned int i = j-1;
		uint A = bignum_extract(a, i); 
		uint B = bignum_extract(b, i);
		if (A > B) return 1;
		if (A < B) return -1;
	}
	return 0;
}
static int bignum_unsigned_cmp_uint(const bignum_t* a, uint x){
	if (bignum_used_size(a) > 1) return 1;
	if (a->arr[0] == x) return 0;
	if (a->arr[0] > x) return 1;
	return -1;
}
static void bignum_unsigned_sub(bignum_t* a, const bignum_t* b){ // NOTE: always sets the sign of a
	if (bignum_unsigned_cmp(a, b) == -1){
		bool carry = false;
		for (unsigned int i = 0; i < b->size; i++)
			bignum_insert(a, i, subc(bignum_extract(b, i), bignum_extract(a, i), &carry));
		// carry should be guaranteed to be 0 here, since a < b
		a->sign = true;
		return;
	}

	bool carry = false;
	for (unsigned int i = 0; i < a->size; i++)
		bignum_insert(a, i, subc(bignum_extract(a, i), bignum_extract(b, i), &carry));
	// carry should be guaranteed to be 0 here, since a >= b
	a->sign = false;
}
static void bignum_unsigned_sub_uint(bignum_t* a, uint b){ // NOTE: always sets the sign of a
	if (bignum_unsigned_cmp_uint(a, b) == -1){ // b > a
		a->arr[0] = b - a->arr[0];
		a->sign = true;
		return;
	}

	bool carry = false;
	for (unsigned int i = 0; i==0 || carry; i++)
		bignum_insert(a, i, subc(bignum_extract(a, i), i ? 0 : b, &carry));
	a->sign = false;
}
void bignum_mul_uint(bignum_t* a, uint b){
	uint carry = 0;
	unsigned int i = 0;
	for (; i < a->size; i++){
		ulong x = bignum_extract(a, i);
		x *= (ulong)b;
		x += carry; // NOTE: overflow cannot occur since the largest value possible is UINT_MAX*UINT_MAX + UINT_MAX = UINT_MAX << 32

		bignum_insert(a, i, (uint)x);
		carry = x >> 32;
	}
	if (carry)
		bignum_insert(a, i, carry);
}
void bignum_shift_left_uint(bignum_t* x, uint shift){
	unsigned int block_shift = shift / 32;
	bignum_grow(x, bignum_used_size(x) + block_shift + 1);
	if (block_shift){
		for (unsigned int j = x->size; j > 0; j--){
			unsigned int i = j - 1;
			if (i < block_shift){
				x->arr[i] = 0;
			}else{
				x->arr[i] = x->arr[i - block_shift];
			}
		}
	}

	shift = shift - 32*block_shift;
	if (!shift) return;

	for (unsigned int j = x->size; j > 0; j--){
		unsigned int i = j - 1;
		if (i == 0){
			x->arr[i] <<= shift;
		}else{
			x->arr[i] <<= shift;
			x->arr[i] |= (x->arr[i-1] >> (32-shift));
		}
	}
}

unsigned int bignum_divmod_uint(bignum_t* a, uint b){
	uint carry = 0;
	for (unsigned int j = a->size; j > 0; j--){
		unsigned int i = j-1;
		ulong current = bignum_extract(a, i);
		current += (ulong)carry << 32;

		bignum_insert(a, i, current / b);
		carry = current % b;
	}
	return carry;
}

void bignum_from_string(bignum_t* dst, const char* s){
	bignum_set_uint(dst, 0);
	while (*s == '-'){
		dst->sign = !dst->sign;
		s++;
	}
	for (unsigned int i = 0; s[i]; i++){
		bignum_mul_uint(dst, 10);
		bignum_unsigned_add_uint(dst, s[i] - '0');
	}
}

char* bignum_to_string(const bignum_t* x){
	char* str = malloc(10*x->size + 2); // 2^32 is 10 characters in decimal + sign + null
	bignum_t* num = bignum_copy(x);

	char* start = str;
	if (num->sign){
		str[0] = '-';
		start = str+1;
	}

	for (unsigned int i = 0; i <= 10*x->size; i++){
		start[i] = '0' + bignum_divmod_uint(num, 10);
	}
	free_bignum(num);

	// reverse string
	char* end = &start[10*x->size];
	while (end >= start && *end == '0') end--;
	if (start > end){ // x == 0
		str[0] = '0';
		str[1] = 0;
		return str;
	}
	end[1] = 0;
	
	while (start < end){
		char tmp = *start;
		*start = *end;
		*end = tmp;
		
		start++;
		end--;
	}
	return str;
}

void bignum_add(bignum_t* a, const bignum_t* b){
	if (a->sign == b->sign){
		bignum_unsigned_add(a, b);
		return;
	}
	bool sign = a->sign;
	bignum_unsigned_sub(a, b);
	a->sign = a->sign != sign;
}
void bignum_sub(bignum_t* a, const bignum_t* b){
	if (a->sign != b->sign){
		bignum_unsigned_add(a, b);
		return;
	}
	bool sign = a->sign;
	bignum_unsigned_sub(a, b);
	a->sign = a->sign != sign;
}
void bignum_add_uint(bignum_t* a, uint b){
	if (a->sign == false){
		bignum_unsigned_add_uint(a, b);
		return;
	}
	bool sign = a->sign;
	bignum_unsigned_sub_uint(a, b);
	a->sign = a->sign != sign;
}
void bignum_sub_uint(bignum_t* a, uint b){
	if (a->sign == true){
		bignum_unsigned_add_uint(a, b);
		return;
	}
	bool sign = a->sign;
	bignum_unsigned_sub_uint(a, b);
	a->sign = a->sign != sign;
}

void bignum_mul(bignum_t* a, const bignum_t* b){
	bignum_t* acc = create_bignum();
	bignum_t* tmp = create_bignum();
	for (unsigned int i=0; i < b->size; i++){
		bignum_set(tmp, a);
		bignum_mul_uint(tmp, bignum_extract(b, i));
		bignum_unsigned_add(acc, tmp);
		bignum_shift_left_uint(a, 32);
	}
	acc->sign = a->sign != b->sign;
	bignum_move_and_free(a, acc);
	free_bignum(tmp);
}
static bignum_t* bignum_divmod(bignum_t* a, const bignum_t* b){ // return div, leave mod in a
	bignum_t* res = create_bignum();
	if (bignum_unsigned_cmp_uint(a, 0) == 0){
		return res; // TODO: throw error?
	}

	bool asign = a->sign;
	bignum_t* tmp = create_bignum();
	bignum_t* tmp2 = create_bignum();

	ulong btop = b->arr[bignum_used_size(b)-1];
	unsigned int btop_pos = (bignum_used_size(b) - 1);

	for (unsigned int j = a->size; j > 0; j--){
		unsigned int i = j - 1;

		bignum_set(tmp, b);
		bignum_shift_left_uint(tmp, 32*i);

		while (bignum_unsigned_cmp(a, tmp) > -1){
			ulong A = ((ulong)bignum_extract(a, btop_pos+i+1) << 32) + bignum_extract(a, btop_pos+i);
			ulong d = A / (btop + 1);
			if (d == 0) d++;
			
			bignum_set(tmp2, tmp);
			bignum_mul_uint(tmp2, d);
			bignum_unsigned_sub(a, tmp2);
			
			// TODO: add shifted ulong directly, instead of through tmp2
			bignum_set_uint(tmp2, d);
			bignum_shift_left_uint(tmp2, 32*i);
			bignum_unsigned_add(res, tmp2);
		}
	}

	while (bignum_unsigned_cmp(a, b) > -1){
		bignum_unsigned_sub(a, b);
		bignum_unsigned_add_uint(res, 1);
	}

	res->sign = asign != b->sign;
	free_bignum(tmp);
	free_bignum(tmp2);
	return res;
}
void bignum_div(bignum_t* a, const bignum_t* b){
	bignum_t* res = bignum_divmod(a, b);
	bignum_move_and_free(a, res);
}
// TODO: test everything below here
void bignum_mod(bignum_t* a, const bignum_t* b){
	bignum_t* res = bignum_divmod(a, b);
	free_bignum(res);
}

int bignum_cmp(const bignum_t* a, const bignum_t* b){ // returns 1 if a > b, 0 if a == b and -1 if a < b
	int unsigned_cmp = bignum_unsigned_cmp(a, b);
	if (unsigned_cmp == 0) return 0;
	if (a->sign != b->sign){
		if (a->sign) return -1; // a is negative
		return 1; // b is negative
	}
	return a->sign ? -unsigned_cmp : unsigned_cmp; // flip if negative
}
int bignum_cmp_uint(const bignum_t* a, uint x){
	if (bignum_used_size(a) > 1) return a->sign ? -1 : 1;
	if (a->arr[0] == x) return 0;
	if (a->arr[0] > x) return a->sign ? -1 : 1;
	return a->sign ? 1 : -1;
}

void bignum_xor(bignum_t* a, const bignum_t* b){
	unsigned int sz = (a->size > b->size) ? a->size : b->size;
	for (unsigned int j = sz; j > 0; j--){
		unsigned int i = j-1;
		bignum_insert(a, i, bignum_extract(a, i) ^ bignum_extract(b, i));
	}
}
void bignum_and(bignum_t* a, const bignum_t* b){
	unsigned int sz = (a->size > b->size) ? a->size : b->size;
	for (unsigned int j = sz; j > 0; j--){
		unsigned int i = j-1;
		bignum_insert(a, i, bignum_extract(a, i) & bignum_extract(b, i));
	}
}
void bignum_or(bignum_t* a, const bignum_t* b){
	unsigned int sz = (a->size > b->size) ? a->size : b->size;
	for (unsigned int j = sz; j > 0; j--){
		unsigned int i = j-1;
		bignum_insert(a, i, bignum_extract(a, i) | bignum_extract(b, i));
	}
}

bool bignum_trunc(bignum_t* a, unsigned int bitwidth, bool is_signed){
	bool changed = false;
	if (is_signed){
		bitwidth--;
		if (a->sign){
			bignum_t* tmp = create_bignum();
			bignum_set_uint(tmp, 1);
			bignum_shift_left_uint(tmp, bitwidth);
			if (bignum_unsigned_cmp(a, tmp) == 0){
				return false;
			}
			free_bignum(tmp);
		}
	}

	for (unsigned int i = 0; i < a->size; i++){
		if (32*(i+1) <= bitwidth) continue;
		if (32*i >= bitwidth){
			changed |= a->arr[i];
			a->arr[i] = 0;
			continue;
		}
		uint mask = UINT32_MAX >> (32*i + 32 - bitwidth);
		changed |= a->arr[i] & ~mask;
		a->arr[i] &= mask;
	}

	if (!is_signed && a->sign){ // 2's complement
		bignum_t* res = create_bignum();
		bignum_set_uint(res, 1);
		bignum_shift_left_uint(res, bitwidth);
		bignum_sub(res, a);
		bignum_move_and_free(a, res);
		changed = true;
	}
	return changed;
}

double bignum_to_double(const bignum_t* x){
	double res = 0;
	for (unsigned int i=0; i < x->size; i++){
		res += ldexp((double)x->arr[i], i*32);
	}
	return res;
}
