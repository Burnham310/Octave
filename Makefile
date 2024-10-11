SRC = lexer.c backend.c utils.c sema.c ds.c parser.c compiler.c midi.c
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
main: $(OBJ) $(ENTRY_DIR)/main.c | $(OUT_DIR)
	$(CC) $(INCLUDES) $^ -o $(OUT_DIR)/$@ 
lexer: $(OBJ) $(ENTRY_DIR)/lexer.c | $(OUT_DIR)
	$(CC) $(INCLUDES) $^ -o $(OUT_DIR)/$@ 
parser: $(OBJ) $(ENTRY_DIR)/parser.c | $(OUT_DIR)
	$(CC) $(INCLUDES) $^ -o $(OUT_DIR)/$@ 

backend:
	gcc $(ENTRY_DIR)/backend.c backend.c midi.c -Iinclude -g -o backend

test_backend:
	gcc $(ENTRY_DIR)/backend.c backend.c midi.c -Iinclude -g -o backend
	./backend
	rm backend

webasm:
	emcc $(SRC) $(ENTRY_DIR)/main.c $(INCLUDES) -o web/octc.js -s WASM=1 -s EXPORTED_FUNCTIONS='["_main"]' -s EXTRA_EXPORTED_RUNTIME_METHODS='["callMain"]'
	echo "web generate successfully!"

clean:
	rm -rf $(OUT_DIR) $(OBJ_DIR)


$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: all clean

.PRECIOUS: $(OBJ_DIR)/%.o
