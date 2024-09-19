SRC = main.c lexer.c ast.c
OBJ = $(SRC:.c=.o)

OUT_DIR = bin
OUT = $(OUT_DIR)/main

CC = gcc
INCLUDES = -Iinclude
CFLAGS += -g

$(OUT): $(OBJ)
	mkdir -p $(OUT_DIR)
	$(CC) $(INCLUDES) $^ -o $@ 

all: $(OUT)

backend:
	$(CC) $(CFLAGS) $(INCLUDES) backend.c -o $@ 

test_backend:
	gcc testcase/backend_test.c backend.c -Iinclude -o backend
	./backend
	rm backend
clean:
	rm -rf $(OUT_DIR) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: all clean
