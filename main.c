#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "lisp.h"

#define REPL_MAXLEN 100

int main(int argc, char **argv)
{
#ifndef USE_READLINE
	char repl_buf[REPL_MAXLEN];
#endif
	if (argc > 1) {
		printf("No args expected, got: %s ...", argv[0]);
		return 1;
	}

	init_globals();
	while (1) {
		const char *endptr = NULL;
		struct expr *e;
		struct expr *r;
#ifdef USE_READLINE
		char *repl_line = readline("> ");
		e = read_expr(repl_line, &endptr);
#else
		printf("> ");
		fgets(repl_buf, REPL_MAXLEN, stdin);
		e = read_expr(repl_buf, &endptr);
#endif
		if (globals.debug) {
			fprintf(stderr, "Parsed expression: ");
			print_expr(e, stderr);
			putc('\n', stderr);
		}
		if (*skip_spaces(endptr)) {
			fprintf(stderr, "Trailing text \"%s\"!\n", endptr);
		} else {
			r = eval_expr(e);
			if (globals.error == ERR_NONE) {
				if (globals.debug) {
					print_dbg_expr(r, stdout);
				} else {
					print_expr(r, stdout);
				}
				putchar('\n');
#ifdef USE_READLINE
				add_history(repl_line);
				free(repl_line);
#endif

			} else {
				/* error message has already been printed */
				globals.error = ERR_NONE;
			}
		}
	}
	return 0;
}
