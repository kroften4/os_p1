#include <stdio.h>
#include "fsize.h"

int fsize(FILE *fp)
{
	int curr = ftell(fp);
	if (fseek(fp, 0, SEEK_END) < 0)
		return -1;
	int res = ftell(fp);
	if (fseek(fp, curr, SEEK_SET) < 0)
		return -1;
	return res;
}
