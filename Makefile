
bin/main: main.c lexer.c ast.c
	mkdir -p bin
	gcc -Iinclude $^ -o $@

# make target 'all' to compile all the files
all: bin/main

# make target 'clean' to remove all the compiled files
clean:
	rm -rf bin

