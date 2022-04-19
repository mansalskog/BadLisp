FLAGS=-std=c89 -pedantic -Wall -Wextra -g -Og

all: lint test main

main: lisp.c lisp.h main.c
	gcc $(FLAGS) -DUSE_READLINE lisp.c main.c -o lisp -lreadline -lm

test: lisp.c lisp.h test.c
	gcc $(FLAGS) lisp.c test.c -o test -lm
	./test

lint: lisp.c lisp.h main.c test.c
	command -v cppcheck && cppcheck lisp.c lisp.h main.c test.c

clean:
	test -f lisp && rm lisp
	test -f test && rm test
