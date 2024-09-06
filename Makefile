main: main.c lexer.c
	gcc -I./ $^ -o $@
