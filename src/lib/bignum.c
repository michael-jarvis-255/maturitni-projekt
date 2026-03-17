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
	bool sign_ext;
} bignum_t;

// static helper functions
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
		x->arr[i] = x->sign_ext * UINT32_MAX;
	}
	x->size = size;
}
static inline uint bignum_extract(const bignum_t* x, unsigned int idx){
	if (idx >= x->size) return x->sign_ext * UINT32_MAX;
	return x->arr[idx];
}
static inline void bignum_insert(bignum_t* x, unsigned int idx, uint value){
	if (idx >= x->size) bignum_grow(x, idx+1);
	x->arr[idx] = value;
}
static inline void bignum_move_and_free(bignum_t* dst, bignum_t* src){
	free(dst->arr);
	*dst = *src;
	free(src);
}
static unsigned int bignum_used_size(const bignum_t* x){
	const uint ext = x->sign_ext * UINT32_MAX;
	for (unsigned int j = x->size; j > 0; j--){
		unsigned int i = j-1;
		if (x->arr[i] != ext) return j;
	}
	return 0;
}


// creation and destruction
bignum_t* create_bignum(){
	bignum_t* num = malloc(sizeof(bignum_t));
	num->size = 1;
	num->arr = calloc(num->size, sizeof(uint));
	num->sign_ext = 0;
	return num;
}
bignum_t* bignum_copy(const bignum_t* original){
	bignum_t* num = malloc(sizeof(bignum_t));
	num->size = original->size;
	num->sign_ext = original->sign_ext;

	num->arr = malloc(sizeof(uint)*original->size);
	num->arr = memcpy(num->arr, original->arr, sizeof(uint)*original->size);
	return num;
}
void free_bignum(bignum_t* num){
	free(num->arr);
	free(num);
}


// set
void bignum_set(bignum_t* dst, const bignum_t* src){
	dst->sign_ext = src->sign_ext;
	bignum_grow(dst, src->size);
	for (unsigned int i = 0; i < src->size; i++){
		dst->arr[i] = src->arr[i];
	}
	for (unsigned int i = src->size; i < dst->size; i++){
		dst->arr[i] = src->sign_ext * UINT32_MAX;
	}
}
void bignum_set_uint(bignum_t* bignum, uint value){
	for (unsigned int i = 0; i < bignum->size; i++){
		bignum->arr[i] = 0;
	}
	bignum->arr[0] = value;
	bignum->sign_ext = 0;
}
void bignum_set_int(bignum_t* bignum, int32_t value){
	bignum->sign_ext = value < 0;
	for (unsigned int i = 0; i < bignum->size; i++){
		bignum->arr[i] = bignum->sign_ext * UINT32_MAX;
	}
	bignum->arr[0] = value;
}


// bitwise ops
void bignum_xor(bignum_t* a, const bignum_t* b){
	for (unsigned int i=0; i < b->size; i++){
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		bignum_insert(a, i, ai ^ bi);
	}
	for (unsigned int i = b->size; i < a->size; i++){
		uint ai = bignum_extract(a, i);
		bignum_insert(a, i, ai ^ (b->sign_ext * UINT32_MAX));
	}
	a->sign_ext = a->sign_ext ^ b->sign_ext;
}
void bignum_and(bignum_t* a, const bignum_t* b){
	for (unsigned int i=0; i < b->size; i++){
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		bignum_insert(a, i, ai & bi);
	}
	for (unsigned int i = b->size; i < a->size; i++){
		uint ai = bignum_extract(a, i);
		bignum_insert(a, i, ai & (b->sign_ext * UINT32_MAX));
	}
	a->sign_ext = a->sign_ext && b->sign_ext;
}
void bignum_or(bignum_t* a, const bignum_t* b){
	for (unsigned int i=0; i < b->size; i++){
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		bignum_insert(a, i, ai | bi);
	}
	for (unsigned int i = b->size; i < a->size; i++){
		uint ai = bignum_extract(a, i);
		bignum_insert(a, i, ai | (b->sign_ext * UINT32_MAX));
	}
	a->sign_ext = a->sign_ext || b->sign_ext;
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
void bignum_shift_right_uint(bignum_t* x, uint shift){
	unsigned int block_shift = shift / 32;
	if (block_shift){
		for (unsigned int i = 0; i < x->size; i++){
			x->arr[i] = bignum_extract(x, i + block_shift);
		}
	}

	shift = shift - 32*block_shift;
	if (!shift) return;

	for (unsigned int i = 0; i < x->size; i++){
		x->arr[i] >>= shift;
		x->arr[i] |= bignum_extract(x, i+1) << (32-shift);
	}
}


// comparisons
int bignum_cmp(const bignum_t* a, const bignum_t* b){
	if (a->sign_ext != b->sign_ext){
		return a->sign_ext ? -1 : 1;
	}
	const unsigned int sz = a->size > b->size ? a->size : b->size;
	for (unsigned int j = sz; j > 0; j--){
		unsigned int i = j - 1;
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		if (ai == bi) continue;
		return (ai > bi) ? 1 : -1;
	}
	return 0;
}
int bignum_cmp_uint(const bignum_t* a, uint32_t b){
	if (a->sign_ext) return -1;
	for (unsigned int i = 1; i < a->size; i++){
		if (a->arr[i]) return 1;
	}
	if (a->arr[0] > b) return 1;
	if (a->arr[0] < b) return -1;
	return 0;
}
bool bignum_is_nonzero(const bignum_t* x){
	if (x->sign_ext) return true;
	for (unsigned int i = 0; i < x->size; i++){
		if (x->arr[i]) return true;
	}
	return false;
}


// arithmetic with uints
static void bignum_add_uint_at(bignum_t* a, uint x, uint i){ // adds x << 32*i
	bool carry = 0;
	bignum_insert(a, i, addc(bignum_extract(a, i), x, &carry));
	while (carry){
		i++;
		if ((a->sign_ext) && (carry == 1) && (i == a->size)){
			a->sign_ext = 0;
			return;
		}

		bignum_insert(a, i, addc(bignum_extract(a, i), 0, &carry));
	}
}
static void bignum_sub_uint_at(bignum_t* a, uint x, uint i){ // subtracts x << 32*i
	bool carry = 0;
	bignum_insert(a, i, subc(bignum_extract(a, i), x, &carry));
	while (carry){
		i++;
		if ((!a->sign_ext) && (carry == 1) && (i == a->size)){
			a->sign_ext = 1;
			return;
		}
		bignum_insert(a, i, subc(bignum_extract(a, i), 0, &carry));
	}
}
void bignum_add_uint(bignum_t* a, uint b){
	bignum_add_uint_at(a, b, 0);
}
void bignum_sub_uint(bignum_t* a, uint b){
	bignum_sub_uint_at(a, b, 0);
}
void bignum_mul_uint(bignum_t* a, uint b){
	bool sign = a->sign_ext;
	if (sign) bignum_negate(a);

	uint carry = 0;
	for (unsigned int i = 0; (i < a->size) || carry; i++){
		ulong x = bignum_extract(a, i);
		x *= (ulong)b;
		x += carry; // NOTE: overflow cannot occur since the largest value possible is UINT_MAX*UINT_MAX + UINT_MAX = UINT_MAX << 32

		bignum_insert(a, i, (uint)x);
		carry = x >> 32;
	}
	if (sign) bignum_negate(a);
}
uint bignum_divmod_uint(bignum_t* a, uint b){
	bool sign = a->sign_ext;
	if (sign) bignum_negate(a);

	uint carry = 0;
	for (unsigned int j = a->size; j > 0; j--){
		unsigned int i = j-1;
		ulong current = bignum_extract(a, i);
		current += (ulong)carry << 32;

		bignum_insert(a, i, current / b);
		carry = current % b;
	}

	if (sign) bignum_negate(a);
	return carry;
}


// arithmetic
void bignum_negate(bignum_t* a){
	for (unsigned int i=0; i < a->size; i++){
		a->arr[i] = ~a->arr[i];
	}
	a->sign_ext = !a->sign_ext;
	bignum_add_uint(a, 1);
}
void bignum_abs(bignum_t* a){
	if (a->sign_ext) bignum_negate(a);
}
void bignum_add(bignum_t* a, const bignum_t* b){
	const unsigned int sz = a->size > b->size ? a->size : b->size;
	bool carry = 0;
	unsigned int i=0;
	for (; i < sz; i++){
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		bignum_insert(a, i, addc(ai, bi, &carry));
	}
	if ((carry == 0) && (a->sign_ext == 0) && (b->sign_ext == 0)) {a->sign_ext = 0; return;}
	if ((carry == 0) && (a->sign_ext == 1) && (b->sign_ext == 0)) {a->sign_ext = 1; return;}
	if ((carry == 0) && (a->sign_ext == 0) && (b->sign_ext == 1)) {a->sign_ext = 1; return;}
	if ((carry == 0) && (a->sign_ext == 1) && (b->sign_ext == 1)) {bignum_insert(a, i, UINT32_MAX - 1); return;}

	if ((carry == 1) && (a->sign_ext == 0) && (b->sign_ext == 0)) {bignum_insert(a, i, 1); return;}
	if ((carry == 1) && (a->sign_ext == 1) && (b->sign_ext == 0)) {a->sign_ext = 0; return;}
	if ((carry == 1) && (a->sign_ext == 0) && (b->sign_ext == 1)) {a->sign_ext = 0; return;}
	if ((carry == 1) && (a->sign_ext == 1) && (b->sign_ext == 1)) {a->sign_ext = 1; return;}
}
void bignum_sub(bignum_t* a, const bignum_t* b){
	const unsigned int sz = a->size > b->size ? a->size : b->size;
	bool carry = 0;
	unsigned int i = 0;
	for (; i < sz; i++){
		uint ai = bignum_extract(a, i);
		uint bi = bignum_extract(b, i);
		bignum_insert(a, i, subc(ai, bi, &carry));
	}
	if ((carry == 0) && (a->sign_ext == 0) && (b->sign_ext == 0)) {a->sign_ext = 0; return;}
	if ((carry == 0) && (a->sign_ext == 1) && (b->sign_ext == 0)) {a->sign_ext = 1; return;}
	if ((carry == 0) && (a->sign_ext == 0) && (b->sign_ext == 1)) {bignum_insert(a, i, 1); return;}
	if ((carry == 0) && (a->sign_ext == 1) && (b->sign_ext == 1)) {a->sign_ext = 0; return;}

	if ((carry == 1) && (a->sign_ext == 0) && (b->sign_ext == 0)) {a->sign_ext = 1; return;}
	if ((carry == 1) && (a->sign_ext == 1) && (b->sign_ext == 0)) {bignum_insert(a, i, UINT32_MAX - 1); return;}
	if ((carry == 1) && (a->sign_ext == 0) && (b->sign_ext == 1)) {a->sign_ext = 0; return;}
	if ((carry == 1) && (a->sign_ext == 1) && (b->sign_ext == 1)) {a->sign_ext = 1; return;}
}
void bignum_mul(bignum_t* a, const bignum_t* b){
	bool sign = a->sign_ext;
	if (sign) bignum_negate(a);

	bignum_t* acc = create_bignum();
	bignum_t* tmp = create_bignum();

	for (unsigned int i=0; i < a->size; i++){
		bignum_set(tmp, b);
		bignum_mul_uint(tmp, bignum_extract(a, i));
		bignum_shift_left_uint(tmp, i*32);
		bignum_add(acc, tmp);
	}
	bignum_move_and_free(a, acc);
	free_bignum(tmp);

	if (sign) bignum_negate(a);
}
bignum_t* bignum_divmod(bignum_t* a, const bignum_t* b, bool* err){ // return div, leave mod in a
	bignum_t* res = create_bignum();
	if (!bignum_is_nonzero(b)){
		*err = true;
		return res;
	}
	*err = false;

	bignum_t* b2 = bignum_copy(b);
	bignum_abs(b2);

	bool asign = a->sign_ext;
	if (asign) bignum_negate(a);

	bignum_t* tmp = create_bignum();
	bignum_t* tmp2 = create_bignum();

	ulong btop = b2->arr[bignum_used_size(b2)-1];
	unsigned int btop_pos = (bignum_used_size(b2) - 1);

	for (unsigned int j = a->size; j > 0; j--){
		unsigned int i = j - 1;

		bignum_set(tmp, b2);
		bignum_shift_left_uint(tmp, 32*i);

		while (bignum_cmp(a, tmp) > -1){ // a > tmp
			ulong A = ((ulong)bignum_extract(a, btop_pos+i+1) << 32) + bignum_extract(a, btop_pos+i);
			ulong d = A / (btop + 1);
			if (d == 0) d++;

			bignum_set(tmp2, tmp);
			bignum_mul_uint(tmp2, d);
			bignum_sub(a, tmp2);

			bignum_add_uint_at(res, d, i);
		}
	}

	while (bignum_cmp(a, b2) > -1){ // a > b
		bignum_sub(a, b2);
		bignum_add_uint(res, 1);
	}

	if (asign && bignum_is_nonzero(a)) { // modulo result needs to be adjusted
		bignum_sub(a, b2);
		bignum_negate(a);
	}

	if (asign != b->sign_ext) bignum_negate(res);

	free_bignum(tmp);
	free_bignum(tmp2);
	free_bignum(b2);
	return res;
}
void bignum_div(bignum_t* a, const bignum_t* b, bool* err){
	bignum_t* res = bignum_divmod(a, b, err);
	bignum_move_and_free(a, res);
}
void bignum_mod(bignum_t* a, const bignum_t* b, bool* err){
	bignum_t* res = bignum_divmod(a, b, err);
	free_bignum(res);
}


// conversions
void bignum_from_string(bignum_t* dst, const char* s){
	bignum_set_uint(dst, 0);
	bool sign = false;
	while (*s == '-'){
		sign = !sign;
		s++;
	}
	for (unsigned int i = 0; s[i]; i++){
		bignum_mul_uint(dst, 10);
		bignum_add_uint(dst, s[i] - '0');
	}
	if (sign){
		bignum_negate(dst);
	}
}
char* bignum_to_string(const bignum_t* x){
	char* str = malloc(10*x->size + 2); // 2^32 is 10 characters in decimal + sign + null
	bignum_t* num = bignum_copy(x);

	char* start = str;
	if (num->sign_ext){
		str[0] = '-';
		start = str+1;
		bignum_negate(num);
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
double bignum_to_double(bignum_t* x){
	bool sign = x->sign_ext;
	if (sign) bignum_negate(x);

	double res = 0;
	for (unsigned int i=0; i < x->size; i++){
		res += ldexp((double)x->arr[i], i*32);
	}
	if (sign){
		bignum_negate(x);
		res = -res;
	}
	return res;
}
bool bignum_trunc_unsigned(bignum_t* a, unsigned int bitwidth){
	bool changed = a->sign_ext;

	bignum_grow(a, (bitwidth/32) + 1);
	a->sign_ext = 0;
	for (unsigned int i=0; i < a->size; i++){
		if (32*(i+1) <= bitwidth) continue;
		if (32*i >= bitwidth){ // the entire block is above bitwidth
			changed |= a->arr[i];
			a->arr[i] = 0;
			continue;
		}
		uint mask = UINT32_MAX >> (32*i + 32 - bitwidth);
		changed |= a->arr[i] & ~mask;
		a->arr[i] &= mask;
	}
	return changed;
}
bool bignum_trunc_signed(bignum_t* a, unsigned int bitwidth){
	if (bitwidth == 0){
		bool changed = bignum_is_nonzero(a);
		bignum_set_uint(a, 0);
		return changed;
	}
	bignum_grow(a, (bitwidth/32) + 1);

	bool changed = false;
	bool new_sign = (bignum_extract(a, (bitwidth-1) / 32) >> ((bitwidth-1) % 32)) & 1;
	for (unsigned int i = 0; i < a->size; i++){
		if (32*(i+1) <= bitwidth) continue;
		if (32*i >= bitwidth){ // the entire block is above bitwidth
			changed |= a->arr[i] != (new_sign*UINT32_MAX);
			a->arr[i] = new_sign*UINT32_MAX;
			continue;
		}
		uint mask = UINT32_MAX >> (32*i + 32 - bitwidth); // the bits that we keep
		changed |= (a->arr[i] & ~mask) != ((new_sign*UINT32_MAX) & ~mask);
		a->arr[i] = (a->arr[i] & mask) | ((new_sign*UINT32_MAX) & ~mask);
	}
	a->sign_ext = new_sign;
	return changed;
}
uint32_t bignum_to_uint(const bignum_t* a){
	return a->arr[0];
}
