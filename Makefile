CC      = gcc
CFLAGS  = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2
SRC     = src/lexer.c src/parser.c src/codegen.c src/main.c
BIN     = dialect
OUT_C   = /tmp/dialect_out.c
OUT_BIN = /tmp/dialect_agent

all: $(BIN)

$(BIN): $(SRC) src/lexer.h src/parser.h src/codegen.h
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

# dialect <file.dialect> → C → нативный бинарь → запуск
compile: $(BIN)
	./$(BIN) examples/hello.dialect > $(OUT_C)
	$(CC) -O2 -o $(OUT_BIN) $(OUT_C)
	$(OUT_BIN)

tokens: $(BIN)
	./$(BIN) --tokens examples/hello.dialect

clean:
	rm -f $(BIN) $(OUT_C) $(OUT_BIN)
