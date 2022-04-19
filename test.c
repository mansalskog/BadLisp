#include <stdlib.h>
#include <stdio.h>

#include "lisp.h"

/* Evaluate a string of lisp code and assert that it is true.
 */
void lisp_assert(const char *src) {
	const char *endptr;
	struct expr *expr = read_expr(src, &endptr);
	struct expr *result;
	if (*endptr) {
		fprintf(stderr, "Trailing chars %s!\n", endptr);
		exit(EXIT_FAILURE);
	}
	result = eval_expr(expr);
	if (result != globals.TRUE) {
		fprintf(stderr, "Lisp assertion failed: %s\n", src);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	if (argc > 1) {
		fprintf(stderr, "No args expected, got: %s ...", argv[0]);
		return EXIT_FAILURE;
	}

	printf("Running tests...\n");
	init_globals();

	/* basic arithmetic */
	lisp_assert("true");
	lisp_assert("(not false)");
	lisp_assert("(eq (+ 1 1) 2)");
	lisp_assert("(eq (* 1 2 3) (+ 1 2 3))");
	lisp_assert("(eq (- 10 1 1 1) 7)");
	lisp_assert("(< 3 4)");
	lisp_assert("(= 3 (abs -3))");
	lisp_assert("(< (abs (- (/ 22 7) pi)) 0.01)");

	/* equality */
	/* lisp_assert("(equal (quote test) (quote test))");*/
	lisp_assert("(equal (list 1 3 3 7) (list 1 3 3 7))");
	lisp_assert("(not (equal (list 1 3 3 7) (list 1 3 3 8)))");

	/* utility functions */
	lisp_assert("(eq (length (list 9 8 7 6 5)) 5)");
	lisp_assert("(equal (map (lambda (x) (* x x)) (list 1 2 3 4)) (list 1 4 9 16))");
	lisp_assert("(not (and true true false))");
	lisp_assert("(or true false true)");

	printf("All tests succeeded!\n");
	return 0;
}
