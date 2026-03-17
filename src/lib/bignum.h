#ifndef _BIGNUM_H_
#define _BIGNUM_H_
#include <stdint.h>
#include <stdbool.h>

typedef struct bignum_t bignum_t;

// creation and destruction
bignum_t* create_bignum();
bignum_t* bignum_copy(const bignum_t* bignum);
void free_bignum(bignum_t* x);

// set
void bignum_set(bignum_t* dst, const bignum_t* src);
void bignum_set_uint(bignum_t* bignum, uint32_t value);
void bignum_set_int(bignum_t* bignum, int32_t value);

// arithmetic
void bignum_negate(bignum_t* a);
void bignum_abs(bignum_t* a);
void bignum_add(bignum_t* a, const bignum_t* b);
void bignum_sub(bignum_t* a, const bignum_t* b);
void bignum_mul(bignum_t* a, const bignum_t* b);
void bignum_div(bignum_t* a, const bignum_t* b, bool* err);
void bignum_mod(bignum_t* a, const bignum_t* b, bool* err);
bignum_t* bignum_divmod(bignum_t* a, const bignum_t* b, bool* err);

// arithmetic with uints
void bignum_add_uint(bignum_t* a, uint32_t b);
void bignum_sub_uint(bignum_t* a, uint32_t b);
void bignum_mul_uint(bignum_t* a, uint32_t b);
uint32_t bignum_divmod_uint(bignum_t* a, uint32_t b);

// comparisons
int bignum_cmp(const bignum_t* a, const bignum_t* b); // returns 1 if a > b, 0 if a == b and -1 if a < b
int bignum_cmp_uint(const bignum_t* a, uint32_t x);
bool bignum_is_nonzero(const bignum_t* x);

// bitwise ops
void bignum_xor(bignum_t* a, const bignum_t* b);
void bignum_and(bignum_t* a, const bignum_t* b);
void bignum_or(bignum_t* a, const bignum_t* b);
void bignum_shift_left_uint(bignum_t* x, uint32_t shift);
void bignum_shift_right_uint(bignum_t* x, uint32_t shift);

// conversions
void bignum_from_string(bignum_t* dst, const char* string); // string must contain only characters 0-9, and a terminating null character
char* bignum_to_string(const bignum_t* x);
double bignum_to_double(bignum_t* x);
bool bignum_trunc_unsigned(bignum_t* a, unsigned int bitwidth);
bool bignum_trunc_signed(bignum_t* a, unsigned int bitwidth);
uint32_t bignum_to_uint(const bignum_t* a);

#endif
