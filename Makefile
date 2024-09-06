
bin/main: main.c lexer.c
	mkdir -p bin
	gcc -I./ $^ -o $@
