#include <stdlib.h>

void rand_str_gen(char *str, int length)
{
	char charset[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

	for (int i = 0; i < length; i++) {
		str[i] = charset[rand() % sizeof(charset)];
	}
}
