#include "internal.h"
#include <stdio.h>

extern int
dyadic_main(
 struct FileInfo *	i
,int				argc
,char * *			argv)
{
	int		flags;

	i->dyadic = 1;
	i->destination = argv[argc - 1];

	for ( flags = 0; flags < (argc - 1) && argv[flags + 1][0] == '-' ; flags++ )
		;
	if ( argc - flags < 3 ) {
		usage(i->applet->usage);
		return 1;
	}
	else if ( argc - flags > 3 ) {
		if ( !is_a_directory(i->destination) ) {
			fprintf(stderr, "%s: not a directory.\n", i->destination);
			return 1;
		}
	}
	return monadic_main(i, argc - 1, argv);
}
