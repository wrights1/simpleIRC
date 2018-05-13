#include <stdio.h>
#include <stdlib.h>

int main()
{
	char *user = getlogin();
	fprintf(stdout, "getlogin() %d\n", errno);

	return 0;
}
