CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -pthread -std=c23 -Iinclude 
BIN_DIR = bin
# look for libcaesar.so in the same directory as main
LDFLAGS = -L$(BIN_DIR) -lcaesar -Wl,-rpath,'$$ORIGIN'
NAME = main

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))

.PHONY: all cli lib dirs clean

all: cli lib

lib: dirs $(BIN_DIR)/libcaesar.so

$(BIN_DIR)/libcaesar.so: lib/caesar.c
	@echo "-- MAKING LIB --"
	$(CC) $(CFLAGS) -shared $< -o $@

$(OBJ): $(SRC)
	@echo "-- MAKING OBJECTS --"
	$(CC) $(CFLAGS) -c $< -o $@

cli: dirs $(BIN_DIR)/$(NAME)

$(BIN_DIR)/$(NAME): $(OBJ) lib
	@echo "-- MAKING $(NAME) --"
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

dirs:
	@mkdir -p build
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf build/
	rm -rf bin/
