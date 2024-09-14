
main_: main.c lexer.c
	mkdir -p bin
	gcc -I./ $^ -o $@

# make target 'all' to compile all the files
all: main_

# make target 'clean' to remove all the compiled files
clean:
	rm -rf bin

