CC = gcc
CFLAGS = -Wall -Wextra -pedantic -fPIC

ifndef XOR_ENC_FN
	XOR_ENC_FN = cezar_enc
endif

ifndef XOR_KEY_FN
	XOR_KEY_FN = cezar_key
endif

.PHONY: all cli lib clean

all: cli lib

cli: build/main

lib: build/libcaesar.so

build/libcaesar.so: libcaesar.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $< -o $@

build/main: main.c
	@mkdir -p $(@D)
	$(CC) \
		-D XOR_ENCRYPT_FN=\"$(XOR_ENC_FN)\" \
		-D XOR_SET_KEY_FN=\"$(XOR_KEY_FN)\" \
		$< -o $@

clean:
	rm -rf build/
