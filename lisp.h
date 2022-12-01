#ifndef _LISP_H_
#define _LISP_H_

#include <stdlib.h>
#include <stdio.h>

#define SYMBOL_MAXLEN 30

enum error {
	ERR_NONE,
	ERR_PARSE,
	ERR_USER
};

enum type {
	T_SYMBOL,
	T_NUMBER,
	T_STRING,
	T_PAIR,
	T_BUILTIN,
	T_LAMBDA
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
void create_function(const char *symbol, const char *params, const char *body);

struct expr *expr_copy(struct expr *e);

struct expr *replace_symbol(struct expr *exp, const char *sym, struct expr *val);
struct expr *eval_each(struct expr *list);
struct expr *eval_lambda(struct lambda *lambda, struct expr *args);
struct expr *eval_funcall(struct expr *f, struct expr *args);
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
struct expr *bi_lambda(struct expr *args);
struct expr *bi_if(struct expr *args);
struct expr *bi_apply(struct expr *args);
struct expr *bi_quote(struct expr *args);
struct expr *bi_cons(struct expr *args);
struct expr *bi_car(struct expr *args);
struct expr *bi_cdr(struct expr *args);
struct expr *bi_eq(struct expr *args);
struct expr *bi_list(struct expr *args);
struct expr *bi_append(struct expr *args);
struct expr *bi_sum(struct expr *args);
struct expr *bi_prod(struct expr *args);
struct expr *bi_diff(struct expr *args);
struct expr *bi_quot(struct expr *args);
struct expr *bi_pow(struct expr *args);
struct expr *bi_numle(struct expr *args);
struct expr *bi_numeq(struct expr *args);
struct expr *bi_and(struct expr *args);
struct expr *bi_or(struct expr *args);
struct expr *bi_pair(struct expr *args);
struct expr *bi_debug(struct expr *args);
struct expr *bi_exit(struct expr *args);

/* All global state. Can later pass around a pointer to this.
 */
struct globals {
	char (*symbols)[SYMBOL_MAXLEN + 1];
	size_t symbols_size;
	size_t symbols_count;
	struct expr **exprs;
	size_t exprs_size;
	size_t exprs_count;
	enum error error;
	int debug;
	struct variable *variables;
	struct expr *TRUE;
	struct expr *FALSE;
} globals;

#endif
