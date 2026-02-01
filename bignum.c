#include "bignum.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


#include <stdio.h> // TODO: remove
static void print_bignum(const bignum_t* x){
	char* str = bignum_to_string(x);
	printf("%s\n", str);
	free(str);
}


typedef unsigned long ulong;
typedef struct bignum_t {
	ulong* arr;
	unsigned int size;
	bool sign;
} bignum_t;

// TODO: replace constants like '64' or '32' with macro constants, or replace 'unsigned long' and 'unsigned int' with fixed width types

static inline ulong add(ulong a, ulong b, bool* carry){
	ulong ab = a+b;
	*carry = (ab < a) || (ab < b);
	return ab;
}
static inline ulong add_carry(ulong a, ulong b, bool c, bool* carry_out){
	ulong ab = a + b;
	*carry_out = (ab < a) || (ab < b);
	ulong abc = ab + c;
	*carry_out |= (abc < ab) || (abc < c);
	return abc;
}
static inline ulong sub(ulong a, ulong b, bool* carry){
	ulong c = a-b;
	*carry = b > a;
	return c;
}
static inline ulong sub_carry(ulong a, ulong b, bool c, bool* carry_out){ // return a - b - c
	ulong bc = add(b, c, carry_out);
	ulong abc = a - bc;
	*carry_out |= bc > a;
	return abc;
}
static inline unsigned int bignum_extract_uint(const bignum_t* x, unsigned int idx){
	ulong val = x->arr[idx/2];
	if (idx % 2){
		val >>= 32;
	}
	return (unsigned int)val;
}
static inline void bignum_insert_uint(const bignum_t* x, unsigned int idx, unsigned int value){
	ulong val = x->arr[idx/2];
	if (idx % 2){
		val = (val & UINT_MAX) | ((ulong)value << 32);
	}else{
		val = (val & ((ulong)UINT_MAX << 32)) | value;
	}
	x->arr[idx/2] = val;
}
static void bignum_grow(bignum_t* x, unsigned int size){
	if (size <= x->size) return;
	x->arr = reallocarray(x->arr, size, sizeof(ulong));
	for (unsigned int i = x->size; i < size; i++){
		x->arr[i] = 0;
	}
	x->size = size;
}

bignum_t* create_bignum(){
	bignum_t* num = malloc(sizeof(bignum_t));
	num->size = 1;
	num->arr = calloc(1, sizeof(ulong));
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
	num->arr = memcpy(malloc(sizeof(ulong)*bignum->size), bignum->arr, sizeof(ulong)*bignum->size);
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

void bignum_set_ulong(bignum_t* bignum, ulong value){
	for (unsigned int i = 0; i < bignum->size; i++){
		bignum->arr[i] = 0;
	}
	bignum->arr[0] = value;
	bignum->sign = false;
}

// these functions ignore the sign of a and b
static void bignum_unsigned_add(bignum_t* a, const bignum_t* b){
	bignum_grow(a, b->size);
	bool carry = false;
	unsigned int i = 0;
	for (; i < b->size; i++){
		a->arr[i] = add_carry(a->arr[i], b->arr[i], (ulong)carry, &carry);
	}
	while (carry){
		bignum_grow(a, i+1);
		a->arr[i] = add(a->arr[i], (ulong)carry, &carry);
		i++;
	}
}
static int bignum_unsigned_cmp(const bignum_t* a, const bignum_t* b){ // returns 1 if a > b, 0 if a == b and -1 if a < b
	unsigned int sz = (a->size > b->size) ? a->size : b->size;
	for (unsigned int j=sz; j > 0; j--){
		unsigned int i = j-1;
		ulong A = (i >= a->size) ? 0 : a->arr[i]; 
		ulong B = (i >= b->size) ? 0 : b->arr[i];

		if (A > B) return 1;
		if (A < B) return -1;
	}
	return 0;
}
static void bignum_unsigned_sub(bignum_t* a, const bignum_t* b){ // NOTE: always sets the sign of a
	if (bignum_unsigned_cmp(a, b) == -1){
		bignum_grow(a, b->size);
		bool carry = false;
		for (unsigned int i = 0; i < b->size; i++){
			a->arr[i] = sub_carry(b->arr[i], a->arr[i], carry, &carry);
		}
		// carry should be guaranteed to be 0 here, since a < b
		a->sign = true;
		return;
	}

	bool carry = false;
	for (unsigned int i = 0; i < a->size; i++){
		ulong B = (i < b->size) ? b->arr[i] : 0;
		a->arr[i] = sub_carry(a->arr[i], B, carry, &carry);
	}
	// carry should be guaranteed to be 0 here, since a >= b
	a->sign = false;
}
static void bignum_mul_uint(bignum_t* a, unsigned int b){
	ulong carry = 0;
	for (unsigned int i=0; i < 2*a->size; i++){
		ulong x = bignum_extract_uint(a, i);
		x *= (ulong)b;
		x += carry; // NOTE: overflow cannot occur since the largest value possible is UINT_MAX*UINT_MAX + UINT_MAX = UINT_MAX << 32

		bignum_insert_uint(a, i, (unsigned int)x);
		carry = x >> 32;
	}
	if (carry){
		bignum_grow(a, a->size+1);
		a->arr[a->size-1] = carry;
	}
}
static unsigned int bignum_used_size(const bignum_t* x){
	for (unsigned int j=x->size; j > 0; j--){
		unsigned int i = j-1;
		if (x->arr[i]){
			return j;
		}
	}
	return 0;
}
static void bignum_shift_left(bignum_t* x, unsigned int shift){
	unsigned int block_shift = shift / 64;
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

	shift = shift - 64*block_shift;
	if (!shift) return;

	for (unsigned int j = x->size; j > 0; j--){
		unsigned int i = j - 1;
		if (i == 0){
			x->arr[i] <<= shift;
		}else{
			x->arr[i] <<= shift;
			x->arr[i] |= (x->arr[i-1] >> (64-shift));
		}
	}
}

static unsigned int bignum_divmod_uint(bignum_t* a, unsigned int b){
	ulong carry = 0;
	for (unsigned int j = a->size*2; j > 0; j--){
		unsigned int i = j-1;
		ulong current = bignum_extract_uint(a, i);
		current += carry << 32;

		bignum_insert_uint(a, i, current / b);
		carry = current % b;
	}
	return carry;
}

void bignum_from_string(bignum_t* dst, const char* s){
	bignum_set_ulong(dst, 0);
	while (*s == '-'){
		dst->sign = !dst->sign;
		s++;
	}
	for (unsigned int i = 0; s[i]; i++){
		bignum_mul_uint(dst, 10);
		dst->arr[0] += s[i] - '0';
	}
}

char* bignum_to_string(const bignum_t* x){ // TODO: correctly handle when x=0
	char* str = malloc(20*x->size + 2); // 2^64 is 20 characters in decimal + sign + null
	bignum_t* num = bignum_copy(x);

	char* start = str;
	if (num->sign){
		str[0] = '-';
		start = str+1;
	}

	for (unsigned int i = 0; i <= 20*x->size; i++){
		start[i] = '0' + bignum_divmod_uint(num, 10);
	}

	// reverse string
	char* end = &start[20*x->size];
	while (*end == '0') end--;
	end[1] = 0;
	
	while (start < end){
		char tmp = *start;
		*start = *end;
		*end = tmp;
		
		start++;
		end--;
	}

	free_bignum(num);
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
void bignum_mul(bignum_t* a, const bignum_t* b){
	bignum_t* acc = create_bignum();
	bignum_t* tmp = create_bignum();
	for (unsigned int i=0; i < 2*b->size; i++){
		bignum_set(tmp, a);
		bignum_mul_uint(tmp, bignum_extract_uint(b, i));
		bignum_unsigned_add(acc, tmp);
		bignum_shift_left(a, 32);
	}
	// TODO: this feels hacky
	free(a->arr);
	a->arr = acc->arr;
	a->sign = a->sign != b->sign;
	free(acc);
	free_bignum(tmp);
}
void bignum_div(bignum_t* a, const bignum_t* b); // TODO

