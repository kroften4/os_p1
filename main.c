#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define IO_CHUNK_SIZE 512
#define XOR_SET_KEY_FN "xor_set_key"
#define XOR_ENCRYPT_FN "xor_encrypt"

int main(int argc, char *argv[])
{
	if (argc != 5) {
		printf("Usage: %s <lib> <key> <src_file> <dst_file>\n",
		       argv[0]);
		return EXIT_FAILURE;
	}

	char *lib = argv[1];
	char *key = argv[2];
	char *src_file = argv[3];
	char *dst_file = argv[4];

	FILE *src_fp = fopen(src_file, "rb");
	FILE *dst_fp = fopen(dst_file, "wb");

	void *handle;
	handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
	void (*xor_set_key)(uint8_t key) = dlsym(handle, XOR_SET_KEY_FN);
	void (*xor_encrypt)(const void *, void *, size_t) =
		dlsym(handle, XOR_ENCRYPT_FN);

	if (!xor_set_key) {
		printf("No " XOR_SET_KEY_FN " symbol found in %s\n", lib);
        return EXIT_FAILURE;
    }
	if (!xor_encrypt) {
		printf("No " XOR_ENCRYPT_FN " symbol found in %s\n", lib);
        return EXIT_FAILURE;
    }

	xor_set_key(key[0]);
	while (!feof(src_fp)) {
		uint8_t *buf = malloc(IO_CHUNK_SIZE);
		size_t len = fread(buf, sizeof(buf[0]), IO_CHUNK_SIZE, src_fp);
		xor_encrypt(buf, buf, len);
		(void)fwrite(buf, sizeof(buf[0]), len, dst_fp);
	}

	dlclose(handle);
	(void)fclose(src_fp);
	(void)fclose(dst_fp);
	return EXIT_SUCCESS;
}
