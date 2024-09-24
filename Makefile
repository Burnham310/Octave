SRC = lexer.c ast.c backend.c common.c
OBJ_DIR = obj
OBJ = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRC))

OUT_DIR = bin
ENTRY_DIR = entry

CC = gcc
INCLUDES = -Iinclude
CFLAGS += -g

all: main
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
$(OUT_DIR):
	mkdir -p $(OUT_DIR)
%: $(OBJ) $(ENTRY_DIR)/%.c | $(OUT_DIR)
	$(CC) $(INCLUDES) $^ -o $(OUT_DIR)/$@ 

backend:
	$(CC) $(CFLAGS) $(INCLUDES) backend.c -o $@ 

test_backend:
	gcc testcase/backend_test.c backend.c -Iinclude -o backend
	./backend
	rm backend
clean:
	rm -rf $(OUT_DIR) $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: all clean

.PRECIOUS: $(OBJ_DIR)/%.o
