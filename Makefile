
bin/main: main.c lexer.c ast.c
	mkdir -p bin
	gcc -Iinclude $^ -o $@
