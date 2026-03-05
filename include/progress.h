#ifndef __PROGRESS_H__
#define __PROGRESS_H__

#include <stddef.h>

struct progress_format
{
    char done;
    char head;
    char empty;
};

char *repeat_char(char ch, size_t times);

/*
 * Print a progress bar `len` in length.
 *
 * If `format` is `NULL`, use "===>  " format.
 */
void progress_print_bar(size_t done, size_t total, size_t len,
                    struct progress_format *format);

#endif // __PROGRESS_H__
