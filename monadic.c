#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <grp.h>

extern int
monadic_main(
 struct FileInfo *	i
,int				argc
,char * *			argv)
{
	int	status = 0;

	while ( argc > 1 && argv[1][0] == '-' ) {
		switch ( argv[1][1] ) {
		case 'c':
			i->create = 1;
			break;
		case 'f':
			i->force = 1;
			break;
		case 'g':
			if ( argc > 2 ) {
				struct group *	g;
				if ( (g = getgrnam(argv[2])) == 0 ) {
					fprintf(stderr, "%s: no such group.\n", argv[1]);
					return 1;
				}
				i->groupID = g->gr_gid;
				i->changeGroupID = 1;
				i->complainInPostProcess = 1;
				argc--;
				argv++;
				break;
			}
			usage(i->applet->usage);
			return 1;
		case 'm':
			if ( argc > 2 ) {
				status = parse_mode(
				 argv[2]
				,&i->orWithMode
				,&i->andWithMode, 0);

				if ( status == 0 ) {
					i->changeMode = 1;
					i->complainInPostProcess = 1;
					argc--;
					argv++;
					break;
				}
			}
			usage(i->applet->usage);
			return 1;
		case 'o':
			if ( argc > 2 ) {
				status = parse_user_name(argv[2], i);
				if ( status != 0 )
					return status;

				i->changeUserID = 1;
				i->complainInPostProcess = 1;
				argc--;
				argv++;
				break;
			}
			usage(i->applet->usage);
			return 1;
		case 'p':
			i->makeParentDirectories = 1;
			break;
		case 'r':
		case 'R':
			i->recursive = 1;
			break;
		case 's':
			i->makeSymbolicLink = 1;
			break;
		default:
			usage(i->applet->usage);
			return 1;
		}
		argv++;
		argc--;
	}
	while ( argc > 1 ) {
		char *	slash;
		i->source = argv[1];
		if ( (slash = strrchr(i->source, '/')) != 0 ) {
			i->directoryLength = slash - i->source;
			if ( i->source[i->directoryLength] == '\0' )
				i->directoryLength = 0;
		}
		else
			i->directoryLength = 0;
		if ( !i->dyadic )
			i->destination = i->source;

		if ( lstat(i->source, &i->stat) == 0 ) {
			i->isSymbolicLink = (i->stat.st_mode & S_IFMT)==S_IFLNK;
			if ( i->isSymbolicLink )
				if ( stat(i->source, &i->stat) != 0 )
					memset(&i->stat, 0, sizeof(i->stat));
		}
		else
			memset(&i->stat, 0, sizeof(i->stat));

		if ( i->isSymbolicLink
		 || !i->recursive
		 || ((i->stat.st_mode & S_IFMT) != S_IFDIR) ) {

			if ( i->applet->function )
				status = i->applet->function(i);
			if ( status == 0 )
				status = post_process(i);
		}
		else
			status = descend(i, i->applet->function);

		if ( status != 0 && !i->force )
			return status;
		argv++;
		argc--;
	}
	return 0;
}
