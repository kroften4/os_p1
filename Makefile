CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -pthread -std=c23 -Iinclude -Ilib
BIN_DIR = bin
NAME = main

SRC = $(wildcard src/*.c) 
LIB_SRC = $(wildcard lib/**/*.c)
OBJ = $(patsubst %.c, build/%.o, $(SRC))
LIB_OBJ = $(patsubst %.c, build/%.o, $(LIB_SRC))
ALL_OBJ = $(OBJ) $(LIB_OBJ)

.PHONY: all cli clean

all: cli

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

cli: $(BIN_DIR)/$(NAME)

$(BIN_DIR)/$(NAME): $(ALL_OBJ)
	@mkdir -p $(@D)
	@echo "-- MAKING $(NAME) --"
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build/
	rm -rf bin/
