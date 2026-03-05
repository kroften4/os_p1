#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "progress.h"

char *repeat_char(char ch, size_t times)
{
	char *s = malloc(times + 1);
	
	for (size_t i = 0; i < times; i++) {
		s[i] = ch;
	}
	s[times] = '\0';
	return s;
}

void progress_print_bar(const size_t done, const size_t total, const size_t len,
					struct progress_format *format)
{
	struct progress_format fmt;
	if (format == NULL) {
		fmt =
			(struct progress_format){ .done = '=', .head = '>', .empty = ' ' };
	} else {
		fmt = *format;
	}

	size_t done_len = ((float)done / total) * len;
	if (done_len != 0) {
		done_len--;
	}

	char *done_str = repeat_char(fmt.done, done_len);
	char *empty_str = repeat_char(fmt.empty, len - done_len - 1);
	printf("%s%c%s", done_str, fmt.head, empty_str);
	free(done_str);
	free(empty_str);
}
