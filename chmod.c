#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "internal.h"

const char	chmod_usage[] = "chmod [-R] mode file [file ...]\n"
"\nmode may be an octal integer representing the bit pattern for the\n"
"\tnew mode, or a symbolic value matching the pattern\n"
"\t[ugoa]{+|-|=}[rwxst] .\n"
"\t\tu:\tUser\n"
"\t\tg:\tGroup\n"
"\t\to:\tOthers\n"
"\t\ta:\tAll\n"
"\n"
"\n+:\tAdd privilege\n"
"\n-:\tRemove privilege\n"
"\n=:\tSet privilege\n"
"\n"
"\t\tr:\tRead\n"
"\t\tw:\tWrite\n"
"\t\tx:\tExecute\n"
"\t\ts:\tSet User ID\n"
"\t\tt:\t\"Sticky\" Text\n"
"\n"
"\tModes may be concatenated, as in \"u=rwx,g=rx,o=rx,-t,-s\n"
"\n"
"\t-R:\tRecursively change the mode of all files and directories\n"
"\t\tunder the argument directory.";

int
parse_mode(
 const char *	s
,mode_t *		or
,mode_t *		and
,int *			group_execute)
{
	/* [ugoa]{+|-|=}[rwxstl] */
	mode_t	mode = 0;
	mode_t	groups = S_ISVTX;
	char	type;
	char	c;

	do {
		for ( ; ; ) {
			switch ( c = *s++ ) {
			case '\0':
				return -1;
			case 'u':
				groups |= S_ISUID|S_IRWXU;
				continue;
			case 'g':
				groups |= S_ISGID|S_IRWXG;
				continue;
			case 'o':
				groups |= S_IRWXO;
				continue;
			case 'a':
				groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				continue;
			case '+':
			case '=':
			case '-':
				type = c;
				if ( groups == S_ISVTX ) /* The default is "all" */
					groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				break;
			default:
				if ( c >= '0' && c <= '7' && mode == 0 && groups == S_ISVTX ) {
					*and = 0;
					*or = strtol(--s, 0, 010);
					return 0;
				}
				else
					return -1;
			}
			break;
		}

		while ( (c = *s++) != '\0' ) {
			switch ( c ) {
			case ',':
				break;
			case 'r':
				mode |= S_IRUSR|S_IRGRP|S_IROTH;
				continue;
			case 'w':
				mode |= S_IWUSR|S_IWGRP|S_IWOTH;
				continue;
			case 'x':
				mode |= S_IXUSR|S_IXGRP|S_IXOTH;
				continue;
			case 's':
				if ( group_execute != 0 && (groups & S_IRWXG) ) {
					if ( *group_execute < 0 )
						return -1;
					if ( type != '-' ) {
						mode |= S_IXGRP;
						*group_execute = 1;
					}
				}
				mode |= S_ISUID|S_ISGID;
				continue;
			case 'l':
				if ( *group_execute > 0 )
					return -1;
				if ( type != '-' ) {
					*and &= ~S_IXGRP;
					*group_execute = -1;
				}
				mode |= S_ISGID;
				groups |= S_ISGID;
				continue;
			case 't':
				mode |= S_ISVTX;
				continue;
			default:
				return -1;
			}
			break;
		}
		switch ( type ) {
		case '=':
			*and &= ~(groups);
			/* fall through */
		case '+':
			*or |= mode & groups;
			break;
		case '-':
			*and &= ~(mode & groups);
			*or &= *and;
			break;
		}
	} while ( c == ',' );
	return 0;
}

extern int
chmod_main(struct FileInfo * i, int argc, char * * argv)
{
	i->andWithMode = S_ISVTX|S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
	i->orWithMode = 0;

	while ( argc >= 3 ) {
		if ( parse_mode(argv[1], &i->orWithMode, &i->andWithMode, 0)
		 == 0 ) {
			argc--;
			argv++;
		}
		else if ( strcmp(argv[1], "-R") == 0 ) {
			i->recursive = 1;
			argc--;
			argv++;
		}
		else
			break;
	}

	i->changeMode = 1;
	i->complainInPostProcess = 1;

	return monadic_main(i, argc, argv);
}
