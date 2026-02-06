#include "../bignum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int main(int argc, char** argv){
	if (argc != 4){
		printf("Usage: %s <number> <operation> <number>\n", argv[0]);
		return 1;
	}
	if (strlen(argv[2]) != 1){
		fprintf(stderr, "operation must be single character\n");
		return 1;
	}


	bignum_t* a = create_bignum();
	bignum_t* b = create_bignum();

	bignum_from_string(a, argv[1]);
	bignum_from_string(b, argv[3]);

	switch (argv[2][0]){
		case '+':
			bignum_add(a, b);
			break;
		case '-':
			bignum_sub(a, b);
			break;
		case '*':
			bignum_mul(a, b);
			break;
		case '/':
			bignum_div(a, b);
			break;
		default:
			fprintf(stderr, "unknown operation '%s'", argv[2]);
			return 1;
	}

	char* res = bignum_to_string(a);
	puts(res);
	free(res);

	free_bignum(a);
	free_bignum(b);
}
