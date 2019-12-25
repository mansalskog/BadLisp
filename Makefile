FLAGS=-std=c89 -pedantic -Wall -Wextra -g -Og

all:
	gcc $(FLAGS) -DUSE_READLINE lisp.c -o lisp -lreadline
