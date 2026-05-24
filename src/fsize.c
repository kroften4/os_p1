#include <stdio.h>
#include "fsize.h"

long fsize(FILE *fp)
{
	long curr = ftell(fp);
	if (fseek(fp, 0, SEEK_END) < 0)
		return -1;
	long res = ftell(fp);
	if (fseek(fp, curr, SEEK_SET) < 0)
		return -1;
	return res;
}
