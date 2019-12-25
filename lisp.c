#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>

#ifdef USE_READLINE
#include <readline/readline.h>
#endif

#define SYMBOL_MAXLEN 30
const char *NON_SYMBOL_CHARS = ")\".";

enum type {
	T_SYMBOL,
	T_NUMBER,
	T_STRING,
	T_PAIR,
	T_BUILTIN,
	T_LAMBDA
};

const char *TYPE_NAMES[] = {
	"symbol",
	"number",
	"string",
	"pair",
	"builtin",
	"lambda"
};

struct pair {
	struct expr *car;
	struct expr *cdr;
};

typedef struct expr *(*func_t)(struct expr *args);

struct builtin {
	func_t func;
	int spec_form;
	/* the name is only used for info messages */
	const char *name;
};

struct lambda {
	struct expr *params;
	struct expr *body;
};

struct expr {
	enum type type;
	union {
		const char *symbol;
		double number;
		char *string;
		struct pair pair;
		struct builtin builtin;
		struct lambda lambda;
	} data;
	unsigned int refs;
};

struct variable {
	const char *symbol;
	struct expr *value;
	struct variable *left;
	struct variable *right;
};

void init_globals(void);
void free_expr(struct expr *e);
void free_unused(void);

const char *save_symbol(const char *symbol);
struct expr *make_symbol(const char *symbol);
struct expr *make_pair(struct expr *car, struct expr *cdr);
struct expr *make_string(const char *string, size_t len);
struct expr *make_number(double number);

struct expr *get_variable(const char *symbol);
void set_variable(const char *symbol, struct expr *value);
void create_builtin(const char *symbol, func_t func, int sf);

struct expr *eval_each(struct expr *list);
struct expr *eval_expr(struct expr *e);

void print_expr(struct expr *e, FILE *f);
void print_dbg_expr(struct expr *e, FILE *f);

const char *skip_spaces(const char *text);
struct expr *read_list(const char *text, const char **endptr);
struct expr *read_symbol(const char *text, const char **endptr);
struct expr *read_string(const char *text, const char **endptr);
struct expr *read_number(const char *text, const char **endptr);
struct expr *read_expr(const char *text, const char **endptr);

unsigned int list_length(struct expr *list);
struct expr *list_index(struct expr *list, unsigned int idx);
int check_arg_count(struct expr *list, unsigned int l);

struct expr *bi_define(struct expr *args);
struct expr *bi_quote(struct expr *args);
struct expr *bi_cons(struct expr *args);
struct expr *bi_car(struct expr *args);
struct expr *bi_cdr(struct expr *args);
struct expr *bi_eq(struct expr *args);
struct expr *bi_list(struct expr *args);
struct expr *bi_sum(struct expr *args);

/* All global state. Can later pass around a pointer to this.
 */
struct globals {
	char (*symbols)[SYMBOL_MAXLEN + 1];
	size_t symbols_size;
	size_t symbols_count;
	struct expr **exprs;
	size_t exprs_size;
	size_t exprs_count;
	struct variable *variables;
} globals;

void init_globals(void) {
	globals.symbols_size = 100;
	globals.symbols = malloc(globals.symbols_size * sizeof *globals.symbols);
	globals.symbols_count = 0;
	globals.exprs_size = 100;
	globals.exprs = malloc(globals.exprs_size * sizeof *globals.exprs);
	globals.exprs_count = 0;
	globals.variables = NULL;
	/* create built-in variables */
	set_variable(save_symbol("pi"),
		     make_number(3.14159265358979323846));
	create_builtin("define", bi_define, 1);
	create_builtin("quote", bi_quote, 1);
	create_builtin("cons", bi_cons, 0);
	create_builtin("car", bi_car, 0);
	create_builtin("cdr", bi_cdr, 0);
	create_builtin("eq", bi_eq, 0);
	create_builtin("list", bi_list, 0);
	create_builtin("+", bi_sum, 0);
}

/* Free a single expression.
 */
void free_expr(struct expr *e)
{
	switch (e->type) {
	case T_SYMBOL:
	case T_NUMBER:
		break;
	case T_STRING:
		free(e->data.string);
		break;
	case T_PAIR:
		--e->data.pair.car->refs;
		--e->data.pair.cdr->refs;
		break;
	case T_BUILTIN:
	case T_LAMBDA:
		/* TODO */
		break;
	}
	free(e);
}

/* Free all unused expressions, collecting garbage.
 */
void free_unused()
{
	size_t freed;
	do {
		size_t i;
		/* free unused, setting them to null */
		freed = 0;
		for (i = 0; i < globals.exprs_count; ++i) {
			if (globals.exprs[i] && globals.exprs[i]->refs == 0) {
				free_expr(globals.exprs[i]);
				globals.exprs[i] = NULL;
				++freed;
			}
		}
	} while (freed > 0);
	{
		/* pack list, removing nulls */
		struct expr **new_exprs =
			malloc(globals.exprs_size * sizeof *new_exprs);
		size_t i;
		size_t j = 0;
		for (i = 0; i < globals.exprs_count; ++i) {
			if (globals.exprs[i])
				new_exprs[j++] = globals.exprs[i];
		}
		free(globals.exprs);
		globals.exprs = new_exprs;
		globals.exprs_count = j;
	}
}


/* Save the symbol in the global list of symbols.
 */
const char *save_symbol(const char *symbol)
{
	char *found = NULL;
	size_t i;
	for (i = 0; i < globals.symbols_count; ++i) {
		if (!strcmp(symbol, globals.symbols[i]))
			found = globals.symbols[i];
	}
	if (!found) {
		/* new symbol */
		++globals.symbols_count;
		if (globals.symbols_count >= globals.symbols_size) {
			globals.symbols_size *= 2;
			globals.symbols =
				realloc(globals.symbols, globals.symbols_size * sizeof *globals.symbols);
		}
		memcpy(globals.symbols[i], symbol, SYMBOL_MAXLEN);
		found = globals.symbols[i];
		/* printf("Created symbol %zd: %s\n", i, buf); */
	}
	return found;
}

/* Construct a new symbol.
 */
struct expr *make_symbol(const char *symbol)
{
	struct expr *e = malloc(sizeof *e);
	e->refs = 0;
	e->type = T_SYMBOL;
	e->data.symbol = save_symbol(symbol);
	return e;
}

/* Construct a new pair.
 */
struct expr *make_pair(struct expr *car, struct expr *cdr)
{
	struct expr *e = malloc(sizeof *e);
	e->refs = 0;
	e->type = T_PAIR;
	e->data.pair.car = car;
	if (car)
		++car->refs;
	e->data.pair.cdr = cdr;
	if (cdr)
		++cdr->refs;
	return e;
}

/* Construct a new string.
 */
struct expr *make_string(const char *text, size_t len)
{
	struct expr *e = malloc(sizeof *e);
	e->refs = 0;
	e->type = T_STRING;
	e->data.string = malloc(len + 1);
	memcpy(e->data.string, text, len + 1);
	return e;
}

/* Construct a new number.
 */
struct expr *make_number(double value)
{
	struct expr *e = malloc(sizeof *e);
	e->refs = 0;
	e->type = T_NUMBER;
	e->data.number = value;
	return e;
}

struct expr *get_variable(const char *symbol)
{
	struct variable *v = globals.variables;
	while (v) {
		if (symbol == v->symbol) {
			return v->value;
		} else {
			int c = strcmp(symbol, v->symbol);
			if (c < 0)
				v = v->left;
		        else if (c > 0)
				v = v->right;
			/* symbols should not be equal if pointers differ */
			else
				assert(0);
		}
	}
	fprintf(stderr, "Undefined variable %s!\n", symbol);
	return NULL;
}

void set_variable(const char *symbol, struct expr *value)
{
	struct variable **v = &globals.variables;
	while (*v) {
		if (symbol == (*v)->symbol) {
			break;
		} else {
			int c = strcmp(symbol, (*v)->symbol);
			if (c < 0)
				v = &(*v)->left;
			else if (c > 0)
				v = &(*v)->right;
			/* symbols should not be equal if pointers differ */
			else
				assert(0);
			/* if empty leaf reached */
			if (!*v)
				break;
		}
	}
	if (*v) {
		/* update refs if changing old variable */
		--(*v)->value->refs;
	} else  {
		/* allocate if creating new variable */
		*v = malloc(sizeof **v);
		(*v)->symbol = symbol;
		(*v)->left = NULL;
		(*v)->right = NULL;
	}
	(*v)->value = value;
	++value->refs;
}

/* Save a builtin as a variable.
 */
void create_builtin(const char *symbol, func_t func, int sf) {
	struct expr *builtin = malloc(sizeof *builtin);
	builtin->type = T_BUILTIN;
	builtin->refs = 0;
	builtin->data.builtin.func = func;
	builtin->data.builtin.spec_form = sf;
	symbol = save_symbol(symbol);
	builtin->data.builtin.name = symbol;
	set_variable(symbol, builtin);
}

/* Evaluate each element of the list. Used for arguments to functions.
 */
struct expr *eval_each(struct expr *list)
{
	struct expr *result = NULL;
	struct expr **curr = &result;
	while (list) {
		assert(list->type = T_PAIR);
		*curr = make_pair(eval_expr(list->data.pair.car), NULL);
		curr = &(*curr)->data.pair.cdr;
		list = list->data.pair.cdr;
	}
	return result;
}

/* Evaluate an expression.
 */
struct expr *eval_expr(struct expr *e)
{
	if (!e) {
		return NULL;
	} else if (e->type == T_SYMBOL) {
		return get_variable(e->data.symbol);
	} else if (e->type == T_PAIR) {
		struct expr *f = eval_expr(e->data.pair.car);
		struct expr *args = e->data.pair.cdr;
		if (!f) {
			fprintf(stderr, "Trying to call non-function nil!\n");
			return NULL;
		} if (f->type == T_BUILTIN) {
			/* if it's not a special form
			   the arguments are evaluated if non-null */
			if (!f->data.builtin.spec_form)
				args = eval_each(args);
			return f->data.builtin.func(args);
		} else if (f->type == T_LAMBDA) {
			return NULL;
		} else {
			fprintf(stderr, "Whooops\n");
			fprintf(stderr,
				"Trying to call non-function of type %s!\n",
				TYPE_NAMES[f->type]);
			return NULL;
		}
	} else {
		return e;
	}
}

/* Print an expression to the file.
 */
void print_expr(struct expr *e, FILE *f)
{
	if (!e) {
		fprintf(f, "()");
	} else {
		switch (e->type) {
		case T_SYMBOL:
			fprintf(f, "%s", e->data.symbol);
			break;
		case T_NUMBER:
			fprintf(f, "%g", e->data.number);
			break;
		case T_STRING:
			fprintf(f, "\"%s\"", e->data.string);
			break;
		case T_PAIR:
			/* print a list, modifying e locally */
			putc('(', f);
			while (e && e->type == T_PAIR) {
				print_expr(e->data.pair.car, f);
				e = e->data.pair.cdr;
				if (e && e->type == T_PAIR)
					putc(' ', f);
			}
			if (e) {
				/* print trailing element */
				fprintf(f, " . ");
				print_expr(e, f);
			}
			putc(')', f);
			break;
		case T_BUILTIN:
			fprintf(f, "[builtin %s]", e->data.builtin.name);
			break;
		case T_LAMBDA:
			fprintf(f, "(lambda ...)");
			break;
		}
	}
}

/* Print an expression with extra debugging information.
 */
void print_dbg_expr(struct expr *e, FILE *f)
{
	putc('[', f);
	if (!e) {
		fprintf(f, "nil");
	} else {
		fprintf(f, "%p %s with %d refs: ",
			(void *) e, TYPE_NAMES[e->type], e->refs);
		if (e->type == T_PAIR) {
			putc('(', f);
			print_dbg_expr(e->data.pair.car, f);
			fprintf(f, " . ");
			print_dbg_expr(e->data.pair.cdr, f);
			putc(')', f);
		} else {
			print_expr(e, f);
		}
	}
	putc(']', f);
}

/* Return a pointer to the first non-space in text.
 * May be end of string.
 */
const char *skip_spaces(const char *text)
{
	while (isspace(*text)) ++text;
	return text;
}

/* Read a list.
 */
struct expr *read_list(const char *text, const char **endptr)
{
	struct expr *e;
	struct expr **f = &e;
	*f = NULL;
	text = skip_spaces(text + 1);
	while (*text != ')') {
		if (!*text) {
			fprintf(stderr, "Unexpected end of input!\n");
			return NULL;
		}
		*f = make_pair(read_expr(text, &text), NULL);
		/* wrong refs for the first pair, but that
		   is corrected at the end */
		++(*f)->refs;
		f = &(*f)->data.pair.cdr;
		text = skip_spaces(text);
	}
	/* skip the trailing ')' */
	++text;
	if (e)
		e->refs = 0;
	if (endptr)
		*endptr = text;
	return e;
}

/* Read a symbol.
 * Symbols are terminated by spaces or chars in NON_SYMBOL_CHARS.
 */
struct expr *read_symbol(const char *text, const char **endptr)
{
	char buf[SYMBOL_MAXLEN];
	struct expr *symbol;
	size_t i = 0;
	while (!isspace(*text)
	       && !strchr(NON_SYMBOL_CHARS, *text)) {
		if (!*text) {
			fprintf(stderr, "Unexpected end of input!\n");
			return NULL;
		}
		buf[i++] = *text++;
		if (i >= SYMBOL_MAXLEN) {
			fprintf(stderr, "Too long symbol!\n");
			return NULL;
		}
	}
	buf[i] = '\0';
	if (endptr)
		*endptr = text;
	symbol = make_symbol(buf);
	return symbol;
}

/* Read a string terminated by '"' from text.
 */
struct expr *read_string(const char *text, const char **endptr)
{
	size_t buf_len = 100;
	char *str_buf = malloc(buf_len);
	struct expr *string;
	size_t i = 0;
	while (*text != '"') {
		if (!*text) {
			free(str_buf);
			fprintf(stderr, "Unexpected end of input!\n");
			return NULL;
		}
		if (i + 1 >= buf_len) {
			buf_len *= 2;
			str_buf = realloc(str_buf, buf_len);
		}
		str_buf[i++] = *text++;
	}
	str_buf[i] = '\0';
	if (endptr)
		*endptr = text + 1;
	string = make_string(str_buf, i + 1);
	free(str_buf);
	return string;
}

/* Reads a number from text. Follows the same rules as strtod.
 */
struct expr *read_number(const char *text, const char **endptr)
{
	struct expr *number;
	number = make_number(strtod(text, (char **) &text));
	if (endptr)
		*endptr = text;
	return number;
}

/* Read an expression from the text. Stores a pointer to after the
 * last read character in endptr, if it is non-null.
 */
struct expr *read_expr(const char *text, const char **endptr) {
	struct expr *e;
	text = skip_spaces(text);
	if (*text == '(') {
		e = read_list(text, &text);
	} else if (*text == '"') {
		e = read_string(text + 1, &text);
	} else if (isdigit(*text)) {
		e = read_number(text, &text);
	} else {
		e = read_symbol(text, &text);
	}
	if (endptr)
		*endptr = text;
	return e;
}

/* Compute the length of the list iteratively.
 */
unsigned int list_length(struct expr *list)
{
	unsigned int len = 0;
	while (list) {
		assert(list->type == T_PAIR);
		list = list->data.pair.cdr;
		++len;
	}
	return len;
}

/* Get an element of the list by iterating through it.
 */
struct expr *list_index(struct expr *list, unsigned int idx)
{
	while (idx > 0) {
		if (!list) {
			fprintf(stderr, "Index out of range!\n");
			return NULL;
		}
		assert(list->type == T_PAIR);
		list = list->data.pair.cdr;
		--idx;
	}
	return list->data.pair.car;
}

/* Check that the correct number of arguments were passed.
 * Returns zero on success.
 */
int check_arg_count(struct expr *args, unsigned int argc)
{
	unsigned int len = list_length(args);
	if (argc != len) {
		fprintf(stderr,
			"Invalid number of arguments: expected %u, got %u!\n",
			argc, len);
		return 1;
	}
	return 0;
}

struct expr *bi_quote(struct expr *args)
{
	if (check_arg_count(args, 1))
		return NULL;
	return list_index(args, 0);
}

struct expr *bi_cons(struct expr *args)
{
	if (check_arg_count(args, 2))
		return NULL;
	return make_pair(list_index(args, 0),
			 list_index(args, 1));
}

struct expr *bi_car(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1))
		return NULL;
	e = list_index(args, 0);
	if (!e || e->type != T_PAIR) {
		fprintf(stderr, "Taking car of non-pair!\n");
		return NULL;
	}
	return e->data.pair.car;
}

struct expr *bi_cdr(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1))
		return NULL;
	e = list_index(args, 0);
	if (!e || e->type != T_PAIR) {
		fprintf(stderr, "Taking cdr of non-pair!\n");
		return NULL;
	}
	return e->data.pair.cdr;
}

struct expr *bi_eq(struct expr *args)
{
	if (check_arg_count(args, 2))
		return NULL;
	if (list_index(args, 0) == list_index(args, 1))
		return make_number(1);
	else
		return make_number(0);
}

struct expr *bi_list(struct expr *args)
{
	/* maybe not the best to simply return the list of args? */
	return args;
}

struct expr *bi_sum(struct expr *args)
{
	double total = 0;
	while (args) {
		struct expr *num;
		assert(args->type == T_PAIR);
		num = args->data.pair.car;
		if (num->type != T_NUMBER) {
			fprintf(stderr, "Cannot sum non-number ");
			print_expr(num, stderr);
			fprintf(stderr, "!\n");
			return NULL;
		}
		total += num->data.number;
		args = args->data.pair.cdr;
	}
	return make_number(total);
}

struct expr *bi_define(struct expr *args)
{
	struct expr *name;
	struct expr *value;
	if (check_arg_count(args, 2))
		return NULL;
	name = list_index(args, 0);
	if (name->type != T_SYMBOL) {
		fprintf(stderr, "Cannot define non-symbol ");
		print_expr(name, stderr);
		fprintf(stderr, "!\n");
		return NULL;
	}
	value = eval_expr(list_index(args, 1));
	set_variable(name->data.symbol, value);
	return NULL;
}

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
		struct expr *e;
		struct expr *r;
		#ifdef USE_READLINE
		char *repl_line = readline("> ");
		e = read_expr(repl_line, NULL);
		free(repl_line);
		#else
		printf("> ");
		fgets(repl_buf, REPL_MAXLEN, stdin);
		e = read_expr(repl_buf, NULL);
		#endif
		r = eval_expr(e);
		print_expr(r, stdout);
		putchar('\n');
	}

	return 0;
}
