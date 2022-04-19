FLAGS=-std=c89 -pedantic -Wall -Wextra -g -Og

all: lisp.c
	command -v cppcheck && cppcheck lisp.c
	gcc $(FLAGS) -DUSE_READLINE lisp.c -o lisp -lreadline -lm

clean:
	rm lisp
