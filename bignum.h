#ifndef _BIGNUM_H_
#define _BIGNUM_H_
#include <stdint.h>

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

#endif
