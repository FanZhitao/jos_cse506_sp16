#include <inc/lib.h>

void
ln(const char *src, const char *dst)
{
	link(src, dst);
}

void
umain(int argc, char **argv)
{
	ln(argv[1], argv[2]);
}

