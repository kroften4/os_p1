CC = gcc
CFLAGS = -Wall -Wextra -pedantic -fPIC

.PHONY: all cli lib clean

all: cli lib

cli: build/main

lib: build/libcaesar.so

build/libcaesar.so: libcaesar.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $< -o $@

build/main: main.c
	@mkdir -p $(@D)
	$(CC) $< -o $@

clean:
	rm -rf build/
