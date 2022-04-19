#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "lisp.h"

/* These characters, as well as spaces, are not allowed in symbols. */
const char *NON_SYMBOL_CHARS = "'()\".";

const char *TYPE_NAMES[] = {
	"symbol",
	"number",
	"string",
	"pair",
	"builtin",
	"lambda"
};

/* Initialize all global state.
 */
void init_globals(void) {
	globals.symbols_size = 100;
	globals.symbols = malloc(globals.symbols_size * sizeof *globals.symbols);
	globals.symbols_count = 0;
	globals.exprs_size = 100;
	globals.exprs = malloc(globals.exprs_size * sizeof *globals.exprs);
	globals.exprs_count = 0;
	globals.variables = NULL;
	globals.error = ERR_NONE;
	globals.debug = 0;
	globals.TRUE = make_symbol("true");
	set_variable(save_symbol("true"), globals.TRUE);
	globals.FALSE = make_symbol("false");
	set_variable(save_symbol("false"), globals.FALSE);
	/* create built-in variables */
	set_variable(save_symbol("pi"),
		     make_number(3.14159265358979323846));
	create_builtin("define", bi_define, 1);
	create_builtin("lambda", bi_lambda, 1);
	create_builtin("if", bi_if, 1);
	create_builtin("apply", bi_apply, 0);
	create_builtin("quote", bi_quote, 1);
	create_builtin("cons", bi_cons, 0);
	create_builtin("car", bi_car, 0);
	create_builtin("cdr", bi_cdr, 0);
	create_builtin("eq", bi_eq, 0);
	create_builtin("list", bi_list, 0);
	create_builtin("append", bi_append, 0);
	create_builtin("+", bi_sum, 0);
	create_builtin("*", bi_prod, 0);
	create_builtin("-", bi_diff, 0);
	create_builtin("/", bi_quot, 0);
	create_builtin("^", bi_pow, 0);
	create_builtin("<", bi_numle, 0);
	create_builtin("=", bi_numeq, 0);
	create_builtin("and", bi_and, 1);
	create_builtin("or", bi_or, 1);
	create_builtin("pair", bi_pair, 0);
	create_builtin("debug", bi_debug, 0);
	create_builtin("exit", bi_exit, 0);
	create_function("not", "(e)", "(if e false true)");
	create_function("null", "(e)", "(eq e ())");
	create_function("<=", "(lhs rhs)", "(or (< lhs rhs) (= rhs lhs))");
	create_function(">", "(lhs rhs)", "(not (<= lhs rhs))");
	create_function(">=", "(lhs rhs)", "(not (< lhs rhs))");
	create_function("abs", "(x)", "(if (< x 0) (- x) x)");
	create_function("equal",
			"(x y)",
			"(if (and (pair x) (pair y)) (and (equal (car x) (car y)) (equal (cdr x) (cdr y))) (eq x y))");
	create_function("map",
			"(f lst)",
			"(if (null lst) () (cons (f (car lst)) (map f (cdr lst))))");
	create_function("length",
			"(lst)",
			"(apply + (map (lambda (e) 1) lst))");
	/* create_function("reverse", "(lst)", "" */
	create_function("member",
			"(e lst)",
			"(if (null lst) false (or (equal e (car lst)) (member e (cdr lst))))");
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
			if (globals.exprs[i]) {
				new_exprs[j++] = globals.exprs[i];
			}
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
		if (!strcmp(symbol, globals.symbols[i])) {
			found = globals.symbols[i];
		}
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
	if (car) {
		++car->refs;
	}
	e->data.pair.cdr = cdr;
	if (cdr) {
		++cdr->refs;
	}
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

/* Construct a new lambda.
 */
struct expr *make_lambda(struct expr *params, struct expr *body)
{
	struct expr *e = malloc(sizeof *e);
	e->refs = 0;
	e->type = T_LAMBDA;
	e->data.lambda.params = params;
	e->data.lambda.body = body;
	return e;
}

/* Get the value of a variable.
 */
struct expr *get_variable(const char *symbol)
{
	struct variable *v = globals.variables;
	while (v) {
		if (symbol == v->symbol) {
			return v->value;
		} else {
			int c = strcmp(symbol, v->symbol);
			if (c < 0) {
				v = v->left;
			} else if (c > 0) {
				v = v->right;
			} else {
				/* symbols should not be equal if pointers differ */
				assert(0);
			}
		}
	}
	fprintf(stderr, "Undefined variable %s!\n", symbol);
	globals.error = ERR_USER;
	return NULL;
}

/* Set the value of a variable.
 */
void set_variable(const char *symbol, struct expr *value)
{
	struct variable **v = &globals.variables;
	while (*v) {
		if (symbol == (*v)->symbol) {
			break;
		} else {
			int c = strcmp(symbol, (*v)->symbol);
			if (c < 0) {
				v = &(*v)->left;
			} else if (c > 0) {
				v = &(*v)->right;
			} else {
				/* symbols should not be equal if pointers differ */
				assert(0);
			}
			/* if empty leaf reached */
			if (!*v) {
				break;
			}
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
void create_builtin(const char *symbol, func_t func, int sf)
{
	struct expr *builtin = malloc(sizeof *builtin);
	builtin->type = T_BUILTIN;
	builtin->refs = 0;
	builtin->data.builtin.func = func;
	builtin->data.builtin.spec_form = sf;
	symbol = save_symbol(symbol);
	builtin->data.builtin.name = symbol;
	set_variable(symbol, builtin);
}

/* Save a function. Reads parameters and body from strings.
 */
void create_function(const char *symbol, const char *params, const char *body)
{
	struct expr *ps;
	struct expr *b;
	const char *endptr;
	ps = read_list(params, &endptr);
	assert(*endptr == '\0');
	b = read_list(body, &endptr);
	assert(*endptr == '\0');
	symbol = save_symbol(symbol);
	set_variable(symbol, make_lambda(ps, b));
}

/* Create a deep copy of a list.
 */
struct expr *expr_copy(struct expr *e)
{
	if (!e) {
		return NULL;
	}
	switch (e->type) {
	case T_SYMBOL:
	case T_NUMBER:
	case T_BUILTIN:
	case T_LAMBDA:
		return e;
	case T_STRING:
		return make_string(e->data.string, strlen(e->data.string));
	case T_PAIR:
		return make_pair(expr_copy(e->data.pair.car),
				 expr_copy(e->data.pair.cdr));
	default:
		/* invalid type */
		assert(0);
	}
}

/* Recursively replace all occurences of sym in exp with val.
 * Assumes that sym is "saved" so that pointer comparison can be done.
 */
struct expr *replace_symbol(struct expr *exp, const char *sym, struct expr *val)
{
	if (!exp) {
		return NULL;
	}
	if (exp->type == T_SYMBOL) {
		if (exp->data.symbol == sym) {
			if (val && val->type == T_PAIR) {
				/* really ugly hack,
				   TODO rewrite lambdas entirely */
				val = make_pair(make_symbol("quote"),
						make_pair(val, NULL));
			}
			return val;
		} else {
			return exp;
		}
	} else if (exp->type == T_PAIR) {
		struct expr *car = replace_symbol(exp->data.pair.car, sym, val);
		struct expr *cdr = replace_symbol(exp->data.pair.cdr, sym, val);
		if (car != exp->data.pair.car || cdr != exp->data.pair.cdr) {
			/* if car or cdr changed, we need to
			   construct a new pair */
			/* TODO reference counting */
			return make_pair(car, cdr);
		} else {
			return exp;
		}
	} else {
		return exp;
	}
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

struct expr *eval_lambda(struct lambda *lambda, struct expr *args)
{
	struct expr *result = lambda->body;
	struct expr *param = lambda->params;
	unsigned int param_count = list_length(lambda->params);
	if (check_arg_count(args, param_count)) {
		return NULL;
	}
	while (param) {
		/* replace a single parameter with its argument value */
		struct expr *sym;
		struct expr *arg;
		assert(param->type == T_PAIR && args->type == T_PAIR);
		sym = param->data.pair.car;
		assert(sym->type == T_SYMBOL);
		arg = args->data.pair.car;
		result = replace_symbol(result, sym->data.symbol, arg);
		param = param->data.pair.cdr;
		args = args->data.pair.cdr;
	}
	if (globals.debug) {
		fprintf(stderr, "Evaluating lambda: ");
		print_expr(result, stderr);
		putc('\n', stderr);
	}
	return eval_expr(result);
}

struct expr *eval_funcall(struct expr *f, struct expr *args)
{
	if (!f) {
		fprintf(stderr, "Trying to call non-function nil!\n");
		globals.error = ERR_USER;
		return NULL;
	} if (f->type == T_BUILTIN) {
		/* if it's not a special form
		   the arguments are evaluated if non-null */
		if (!f->data.builtin.spec_form) {
			args = eval_each(args);
		}
		return f->data.builtin.func(args);
	} else if (f->type == T_LAMBDA) {
		args = eval_each(args);
		return eval_lambda(&f->data.lambda, args);
	} else {
		fprintf(stderr,
			"Trying to call non-function of type %s!\n",
			TYPE_NAMES[f->type]);
		globals.error = ERR_USER;
		return NULL;
	}
}

/* Evaluate an expression.
 */
struct expr *eval_expr(struct expr *e)
{
	/* TODO reference counting */
	if (!e) {
		return NULL;
	} else if (e->type == T_SYMBOL) {
		return get_variable(e->data.symbol);
	} else if (e->type == T_PAIR) {
		struct expr *f = eval_expr(e->data.pair.car);
		struct expr *args = e->data.pair.cdr;
		return eval_funcall(f, args);
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
				if (e && e->type == T_PAIR) {
					putc(' ', f);
				}
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
			fprintf(f, "(lambda ");
			print_expr(e->data.lambda.params, f);
			putc(' ', f);
			print_expr(e->data.lambda.body, f);
			putc(')', f);
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
			globals.error = ERR_PARSE;
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
	if (e) {
		e->refs = 0;
	}
	if (endptr) {
		*endptr = text;
	}
	return e;
}

int is_symbol_char(char c)
{
	return c != '\0'
		&& !isspace(c)
		&& !strchr(NON_SYMBOL_CHARS, c);
}

/* Read a symbol.
 * Symbols are terminated by spaces or chars in NON_SYMBOL_CHARS.
 */
struct expr *read_symbol(const char *text, const char **endptr)
{
	char buf[SYMBOL_MAXLEN];
	struct expr *symbol;
	size_t i = 0;
	while (is_symbol_char(*text)) {
		if (!*text) {
			fprintf(stderr, "Unexpected end of input!\n");
			globals.error = ERR_PARSE;
			return NULL;
		}
		buf[i++] = *text++;
		if (i >= SYMBOL_MAXLEN) {
			fprintf(stderr, "Too long symbol!\n");
			globals.error = ERR_PARSE;
			return NULL;
		}
	}
	buf[i] = '\0';
	if (endptr) {
		*endptr = text;
	}
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
			globals.error = ERR_PARSE;
			return NULL;
		}
		if (i + 1 >= buf_len) {
			char *new_str_buf;
			buf_len *= 2;
			new_str_buf = realloc(str_buf, buf_len);
			if (!new_str_buf) {
				fprintf(stderr, "Cannot allocate input buffer!\n");
				return NULL;
			}
			str_buf = new_str_buf;
		}
		str_buf[i++] = *text++;
	}
	str_buf[i] = '\0';
	if (endptr) {
		*endptr = text + 1;
	}
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
	if (endptr) {
		*endptr = text;
	}
	return number;
}

/* Read an expression from the text. Stores a pointer to after the
 * last read character in endptr, if it is non-null.
 */
struct expr *read_expr(const char *text, const char **endptr) {
	struct expr *e = NULL;
	text = skip_spaces(text);
	if (*text == '(') {
		e = read_list(text, &text);
	} else if (*text == '"') {
		e = read_string(text + 1, &text);
	} else if (isdigit(*text) || (*text == '-' && isdigit(text[1]))) {
		/* pretty ugly hack to handle reading of negative numbers */
		e = read_number(text, &text);
	} else if (is_symbol_char(*text)) {
		e = read_symbol(text, &text);
	} else {
		fprintf(stderr, "No parse for \"%s\"!\n", text);
		globals.error = ERR_PARSE;
	}
	if (endptr) {
		*endptr = text;
	}
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
			globals.error = ERR_USER;
			return NULL;
		}
		assert(list->type == T_PAIR);
		list = list->data.pair.cdr;
		--idx;
	}
	return list->data.pair.car;
}

/* Check that the correct number of arguments were passed.
 * Prints an error message, sets the global error state and returns non-zero
 * if the number of arguments is incorrect.
 */
int check_arg_count(struct expr *args, unsigned int argc)
{
	unsigned int len = list_length(args);
	if (argc != len) {
		fprintf(stderr,
			"Invalid number of arguments: expected %u, got %u!\n",
			argc,
			len);
		globals.error = ERR_USER;
		return 1;
	}
	return 0;
}

/* Check that the expression has the correct type.
 * Prints an error message, sets the global error state and returns non-zero
 * if the expression has the wrong type.
 */
int check_type(struct expr *e, enum type t)
{
	if (!e) {
		fprintf(stderr,
			"Invalid type: expected %s, got nil!\n",
			TYPE_NAMES[t]);
		globals.error = ERR_USER;
		return 1;
	}
	if (e->type != t) {
		fprintf(stderr,
			"Invalid type: expected %s, got %s!\n",
			TYPE_NAMES[t],
			TYPE_NAMES[e->type]);
		globals.error = ERR_USER;
		return 1;
	}
	return 0;
}

/* Built-in functions. */

struct expr *bi_define(struct expr *args)
{
	struct expr *name;
	struct expr *value;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	name = list_index(args, 0);
	if (check_type(name, T_SYMBOL)) {
		return NULL;
	}
	value = eval_expr(list_index(args, 1));
	set_variable(name->data.symbol, value);
	return NULL;
}

struct expr *bi_lambda(struct expr *args)
{
	struct expr *params;
	struct expr *body;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	params = list_index(args, 0);
	if (args->type != T_PAIR) {
		fprintf(stderr, "Invalid parameter list ");
		print_expr(params, stderr);
		fprintf(stderr, "!\n");
		globals.error = ERR_USER;
		return NULL;
	}
	body = list_index(args, 1);
	return make_lambda(params, body);
}

struct expr *bi_if(struct expr *args)
{
	/* TODO: create a type for booleans (and make numbers primitive) */
	struct expr *test;
	if (check_arg_count(args, 3)) {
		return NULL;
	}
	test = eval_expr(list_index(args, 0));
	if (check_type(test, T_SYMBOL)) {
		return NULL;
	} else if (test->data.symbol == globals.TRUE->data.symbol) {
		return eval_expr(list_index(args, 1));
	} else if (test->data.symbol == globals.FALSE->data.symbol) {
		return eval_expr(list_index(args, 2));
	} else {
		fprintf(stderr, "Invalid truth value: ");
		print_expr(test, stderr);
		globals.error = ERR_USER;
		return NULL;
	}
}

struct expr *bi_apply(struct expr *args)
{
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	return eval_funcall(list_index(args, 0), list_index(args, 1));
}

struct expr *bi_quote(struct expr *args)
{
	if (check_arg_count(args, 1)) {
		return NULL;
	}
	return list_index(args, 0);
}

struct expr *bi_cons(struct expr *args)
{
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	return make_pair(list_index(args, 0),
			 list_index(args, 1));
}

struct expr *bi_car(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1)) {
		return NULL;
	}
	e = list_index(args, 0);
	if (check_type(e, T_PAIR)) {
		return NULL;
	}
	return e->data.pair.car;
}

struct expr *bi_cdr(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1)) {
		return NULL;
	}
	e = list_index(args, 0);
        if (check_type(e, T_PAIR)) {
		return NULL;
	}
	return e->data.pair.cdr;
}

struct expr *bi_eq(struct expr *args)
{
	struct expr *x;
	struct expr *y;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	x = list_index(args, 0);
	y = list_index(args, 1);
	if (x == y) {
		/* handles reference equality and symbols */
		return globals.TRUE;
	} else if (x && y
		   && x->type == T_NUMBER
		   && y->type == T_NUMBER
		   && x->data.number == y->data.number) {
		/* TODO handle numbers without pointers */
		return globals.TRUE;
	} else {
		return globals.FALSE;
	}
}

struct expr *bi_list(struct expr *args)
{
	/* maybe not the best to simply return the list of args? */
	return args;
}

struct expr *bi_append(struct expr *args)
{
	struct expr *before;
	struct expr *after;
	struct expr *iter;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	before = list_index(args, 0);
	after = list_index(args, 1);
	if (!before) {
		return after;
	}
	before = expr_copy(before);
	iter = before;
	assert(iter->type == T_PAIR);
	while (iter->data.pair.cdr) {
		assert(iter->type == T_PAIR);
		iter = iter->data.pair.cdr;
	}
	iter->data.pair.cdr = after;
	return before;
}

struct expr *bi_sum(struct expr *args)
{
	double tot = 0;
	while (args) {
		struct expr *num;
		assert(args->type == T_PAIR);
		num = args->data.pair.car;
		if (check_type(num, T_NUMBER)) {
			return NULL;
		}
		tot += num->data.number;
		args = args->data.pair.cdr;
	}
	return make_number(tot);
}

struct expr *bi_prod(struct expr *args)
{
	/* should be an exact copy of bi_sum, except the operator */
	double tot = 1;
	while (args) {
		struct expr *num;
		assert(args->type == T_PAIR);
		num = args->data.pair.car;
		if (check_type(num, T_NUMBER)) {
			return NULL;
		}
		tot *= num->data.number;
		args = args->data.pair.cdr;
	}
	return make_number(tot);
}

struct expr *bi_diff(struct expr *args)
{
	int processed = 0;
	double tot = 0.0;
	while (args) {
		struct expr *num;
		assert(args->type == T_PAIR);
		num = args->data.pair.car;
		if (check_type(num, T_NUMBER)) {
			return NULL;
		}
		if (processed == 0) {
			tot = num->data.number;
		} else {
			tot -= num->data.number;
		}
		++processed;
		args = args->data.pair.cdr;
	}
	return processed == 1 ? make_number(-tot) : make_number(tot);
}

struct expr *bi_quot(struct expr *args)
{
	/* should be an exact copy of bi_diff, except the operator */
	int first = 1;
	double tot = 0.0;
	while (args) {
		struct expr *num;
		assert(args->type == T_PAIR);
		num = args->data.pair.car;
		if (check_type(num, T_NUMBER)) {
			return NULL;
		}
		if (first) {
			tot = num->data.number;
			first = 0;
		} else {
			tot /= num->data.number;
		}
		args = args->data.pair.cdr;
	}
	return make_number(tot);
}

struct expr *bi_pow(struct expr *args)
{
	struct expr *base;
	struct expr *expt;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	base = list_index(args, 0);
	expt = list_index(args, 1);
	if (check_type(base, T_NUMBER) || check_type(expt, T_NUMBER)) {
		return NULL;
	}
	return make_number(pow(base->data.number, expt->data.number));
}

struct expr *bi_numle(struct expr *args)
{
	struct expr *lhs;
	struct expr *rhs;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	lhs = list_index(args, 0);
	rhs = list_index(args, 1);
	if (check_type(lhs, T_NUMBER) || check_type(rhs, T_NUMBER)) {
		return NULL;
	}
	return lhs->data.number < rhs->data.number ? globals.TRUE : globals.FALSE;
}

struct expr *bi_numeq(struct expr *args)
{
	struct expr *lhs;
	struct expr *rhs;
	if (check_arg_count(args, 2)) {
		return NULL;
	}
	lhs = list_index(args, 0);
	rhs = list_index(args, 1);
	if (check_type(lhs, T_NUMBER) || check_type(rhs, T_NUMBER)) {
		return NULL;
	}
	return lhs->data.number == rhs->data.number ? globals.TRUE : globals.FALSE;
}

struct expr *bi_and(struct expr *args)
{
	while (args) {
		assert(args->type == T_PAIR);
		if (eval_expr(args->data.pair.car)->data.symbol == globals.FALSE->data.symbol) {
			return globals.FALSE;
		}
		args = args->data.pair.cdr;
	}
	/* and of empty list is true */
	return globals.TRUE;
}

struct expr *bi_or(struct expr *args)
{
	while (args) {
		assert(args->type == T_PAIR);
		if (eval_expr(args->data.pair.car)->data.symbol == globals.TRUE->data.symbol) {
			return globals.TRUE;
		}
		args = args->data.pair.cdr;
	}
	/* or of empty list is false */
	return globals.FALSE;
}

struct expr *bi_pair(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1)) {
		return NULL;
	}
	e = list_index(args, 0);
	if (e && e->type == T_PAIR) {
		return globals.TRUE;
	} else {
		return globals.FALSE;
	}
}

struct expr *bi_debug(struct expr *args)
{
	struct expr *e;
	if (check_arg_count(args, 1)) {
		return NULL;
	}
	e = list_index(args, 0);
	if (check_type(e, T_SYMBOL)) {
		return NULL;
	}
	if (e->data.symbol == globals.TRUE->data.symbol) {
		globals.debug = 1;
	} else if (e->data.symbol == globals.FALSE->data.symbol) {
		globals.debug = 0;
	} else {
		fprintf(stderr, "Invalid truth value: ");
		print_expr(e, stderr);
		globals.error = ERR_USER;
	}
	return NULL;
}

struct expr *bi_exit(struct expr *args)
{
	int length = list_length(args);
	if (length == 0) {
		exit(0);
	} else if (length == 1) {
		struct expr *ret = list_index(args, 0);
		if (check_type(ret, T_NUMBER)) {
			return NULL;
		}
		exit(ret->data.number);
	} else {
		fprintf(stderr, "Too many arguments, expected 0 or 1!\n");
		return NULL;
	}
}
