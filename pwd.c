#include "internal.h"
#include <stdio.h>

const char	pwd_usage[] = "Print the current directory.\n";

extern int
pwd_main(int argc, char * * argv)
{
	char		buf[1024];

	if ( getcwd(buf, sizeof(buf)) == NULL ) {
		perror("get working directory");
		exit( FALSE);
	}

	printf("%s\n", buf);
	exit( TRUE);
}
