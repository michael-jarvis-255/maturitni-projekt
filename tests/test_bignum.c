#include "../src/lib/bignum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int main(int argc, const char** argv){
	if (argc < 3){
		printf("Usage: %s <operation> <numbers>\n", argv[0]);
		return 1;
	}
	const char* op = argv[1];
	const unsigned int n = argc - 2;

	bignum_t** nums = malloc(sizeof(bignum_t*)*n);
	for (unsigned int i=0; i<n; i++){
		nums[i] = create_bignum();
		bignum_from_string(nums[i], argv[i+2]);

		//char* res = bignum_to_string(nums[i]);
		//puts(res);
		//free(res);
	}

	bool err = false;
	if (strcmp(op, "+") == 0){
		bignum_add(nums[0], nums[1]);
	} else if (strcmp(op, "-") == 0){
		bignum_sub(nums[0], nums[1]);
	} else if (strcmp(op, "*") == 0) {
		bignum_mul(nums[0], nums[1]);
	} else if (strcmp(op, "/") == 0) {
		bignum_div(nums[0], nums[1], &err);
	} else if (strcmp(op, "%") == 0) {
		bignum_mod(nums[0], nums[1], &err);
	} else if (strcmp(op, "cmp") == 0) {
		int res = bignum_cmp(nums[0], nums[1]);
		printf("%i\n", res);
		goto end;
	} else if (strcmp(op, "xor") == 0) {
		bignum_xor(nums[0], nums[1]);
	} else if (strcmp(op, "and") == 0) {
		bignum_and(nums[0], nums[1]);
	} else if (strcmp(op, "or") == 0) {
		bignum_or(nums[0], nums[1]);
	} else if (strcmp(op, "double") == 0) {
		double res = bignum_to_double(nums[0]);
		printf("%.20e\n", res);
		goto end;
	} else {
		fprintf(stderr, "unknown operation '%s'", op);
		return 1;
	}
	if (err) {
		puts("err");
		goto end;
	}

	char* res = bignum_to_string(nums[0]);
	puts(res);
	free(res);

end:
	for (unsigned int i=0; i<n; i++){
		free_bignum(nums[i]);
	}
	free(nums);
	return 0;
}
