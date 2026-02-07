#ifndef _BIGNUM_H_
#define _BIGNUM_H_
#include <stdint.h>
#include <stdbool.h>

typedef struct bignum_t bignum_t;

bignum_t* create_bignum();
void free_bignum(bignum_t* x);
bignum_t* bignum_copy(const bignum_t* bignum);
void bignum_set(bignum_t* dst, const bignum_t* src);
void bignum_set_uint(bignum_t* bignum, uint32_t value);
void bignum_from_string(bignum_t* dst, const char* string); // string must contain only characters 0-9, and a terminating null character
char* bignum_to_string(const bignum_t* x);

// all operations modify 'a'
void bignum_add(bignum_t* a, const bignum_t* b);
void bignum_sub(bignum_t* a, const bignum_t* b);
void bignum_mul(bignum_t* a, const bignum_t* b);
void bignum_div(bignum_t* a, const bignum_t* b);
void bignum_mod(bignum_t* a, const bignum_t* b);

int bignum_cmp(const bignum_t* a, const bignum_t* b);
int bignum_cmp_uint(const bignum_t* a, uint32_t x);

void bignum_xor(bignum_t* a, const bignum_t* b);
void bignum_and(bignum_t* a, const bignum_t* b);
void bignum_or(bignum_t* a, const bignum_t* b);

bool bignum_trunc(bignum_t* a, unsigned int bitwidth, bool is_signed); // truncates 'a' to a number that fits into an integer of size'bitwidth'

void bignum_add_uint(bignum_t* a, uint32_t b);
void bignum_sub_uint(bignum_t* a, uint32_t b);
void bignum_mul_uint(bignum_t* a, uint32_t b);
uint32_t bignum_divmod_uint(bignum_t* a, uint32_t b);

void bignum_shift_left_uint(bignum_t* x, uint32_t shift);

#endif
