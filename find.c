#include "internal.h"
#include <errno.h>
#include <stdio.h>

const char	find_usage[] = "find dir [pattern]\n"
"\n"
"\tFind files.\n";

extern int
find_main(struct FileInfo * i, int argc, char * * argv)
{
	i->recursive=1;
	i->processDirectoriesAfterTheirContents=1;
	return monadic_main(i, argc, argv);
}

extern int
find_fn(const struct FileInfo * i)
{
	printf("%s\n",i->source);

	return(0);      
}
