#include "internal.h"
#include <stdio.h>

const char	pwd_usage[] = "Print the current directory.\n";

extern int
pwd_main(struct FileInfo * i, int argc, char * * argv)
{
	char		buf[1024];

	if ( getcwd(buf, sizeof(buf)) == NULL ) {
		name_and_error("get working directory");
		return 1;
	}

	printf("%s\n", buf);
	return 0;
}
