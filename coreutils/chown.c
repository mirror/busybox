#include "internal.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdio.h>

const char	chown_usage[] = "chown [-R] user-name file [file ...]\n"
"\n\tThe group list is kept in the file /etc/groups.\n\n"
"\t-R:\tRecursively change the mode of all files and directories\n"
"\t\tunder the argument directory.";

int
parse_user_name(const char * s, struct FileInfo * i)
{
	struct	passwd * 	p;
	char *				dot = strchr(s, '.');

	if (! dot )
		dot = strchr(s, ':');

	if ( dot )
		*dot = '\0';

	if ( (p = getpwnam(s)) == 0 ) {
		fprintf(stderr, "%s: no such user.\n", s);
		return 1;
	}
	i->userID = p->pw_uid;

	if ( dot ) {
		struct group *	g = getgrnam(++dot);
		if ( g == 0 ) {
			fprintf(stderr, "%s: no such group.\n", dot);
			return 1;
		}
		i->groupID = g->gr_gid;
		i->changeGroupID = 1;
	}
	return 0;
}

extern int
chown_main(struct FileInfo * i, int argc, char * * argv)
{
	int					status;

	while ( argc >= 3 && strcmp("-R", argv[1]) == 0 ) {
		i->recursive = 1;
		argc--;
		argv++;
	}

	if ( (status = parse_user_name(argv[1], i)) != 0 )
		return status;

	argv++;
	argc--;

	i->changeUserID = 1;
	i->complainInPostProcess = 1;

	return monadic_main(i, argc, argv);
}
