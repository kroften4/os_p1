CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -pthread -std=c23
CPPFLAGS := -MMD -MP -Iinclude -Ilib
BIN_DIR := bin
NAME := main

SRC := $(shell find src/ -type f -name '*.c') 
LIB_SRC := $(shell find lib/ -type f -name '*.c')
OBJ := $(SRC:%.c=build/%.o)
LIB_OBJ := $(LIB_SRC:%.c=build/%.o)
ALL_OBJ := $(OBJ) $(LIB_OBJ)

.PHONY: all cli clean

all: cli

-include $(ALL_OBJ:.o=.d)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

cli: $(BIN_DIR)/$(NAME)

$(BIN_DIR)/$(NAME): $(ALL_OBJ)
	@mkdir -p $(@D)
	@echo "-- MAKING $(NAME) --"
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build/ $(BIN_DIR)/
