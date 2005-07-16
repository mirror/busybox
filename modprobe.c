/* vi: set sw=4 ts=4: */
/*
 * really dumb modprobe implementation for busybox
 * Copyright (C) 2001 Lineo, davidm@lineo.com
 *
 * BB_FEATURE_MODPROBE_DEPEND stuff was added and is
 * Copyright (C) 2002 Robert Griebl, griebl@gmx.de
 *
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "busybox.h"

#define BB_FEATURE_MODPROBE_DEPEND

#ifdef BB_FEATURE_MODPROBE_DEPEND

#include <sys/utsname.h>

/* Jump through hoops to simulate how fgets() grabs just one line at a time...
 * Don't use any stdio since modprobe gets called from a kernel thread and
 * stdio junk can overflow the limited stack...     */
static char *reads ( int fd, char *buffer, size_t len )
{            
	ssize_t n = read ( fd, buffer, len );
	if ( n > 0 ) {
		char *p;
		buffer [len-1] = 0;
		p = strchr ( buffer, '\n' );
		if ( p ) {
			off_t offset;
			offset = lseek ( fd, 0L, SEEK_CUR );
			/* Get the current file descriptor offset */
			lseek ( fd, offset-n + (p-buffer) + 1, SEEK_SET );
			/* Set the file descriptor offset to right after the \n */
			p[1] = 0;
		}
		return buffer;
	} else {
		return 0;
	}
}

struct dep_t {
	char *  m_module;
	int     m_depcnt;
	char ** m_deparr;
	
	struct dep_t * m_next;
};


static struct dep_t *build_dep ( void )
{
	int fd;
	struct utsname un;
	struct dep_t *first = 0;
	struct dep_t *current = 0;
	char buffer [256];
	char *filename = buffer;
	int continuation_line = 0;
	
	if ( uname ( &un ))
		return 0;
	strcpy ( filename, "/lib/modules/" );
	strcat ( filename, un.release );
	strcat ( filename, "/modules.dep" );

	if ((fd = open ( filename, O_RDONLY )) < 0)
		return 0;
	
	while ( reads ( fd, buffer, sizeof( buffer ))) {
		char *p;
		int l = xstrlen ( buffer );

		if ( buffer [l-1] == '\n' ) {
			buffer [l-1] = 0;
			l--;
		}
		
		if ( l == 0 ) {
			continuation_line = 0;
			continue;
		}
		
		if ( !continuation_line ) {		
			char *col = strchr ( buffer, ':' );
		
			if ( col ) {
				char *mods;
				char *mod;
				int ext = 0;
				
				*col = 0;
				mods = strrchr ( buffer, '/' );
				
				if ( !mods )
					mods = buffer;
				else
					mods++;
					
				if (( *(col-2) == '.' ) && ( *(col-1) == 'o' ))
					ext = 2;
				
				mod = xstrndup ( mods, col - mods - ext );
					
				if ( !current ) {
					first = current = (struct dep_t *) malloc ( sizeof ( struct dep_t ));
				}
				else {
					current->m_next = (struct dep_t *) malloc ( sizeof ( struct dep_t ));
					current = current->m_next;
				}
				current->m_module = mod;
				current->m_depcnt = 0;
				current->m_deparr = 0;
				current->m_next = 0;
						
				//printf ( "%s:\n", mod );
						
				p = col + 1;		
			}
			else
				p = 0;
		}
		else
			p = buffer;
			
		if ( p && *p ) {
			char *end = &buffer [l-1];
			char *deps = strrchr ( end, '/' );
			char *dep;
			int ext = 0;
			
			while ( isblank ( *end ) || ( *end == '\\' ))
				end--;
				
			deps = strrchr ( p, '/' );
			
			if ( !deps || ( deps < p )) {
				deps = p;
		
				while ( isblank ( *deps ))
					deps++;
			}
			else
				deps++;
			
			if (( *(end-1) == '.' ) && ( *end == 'o' ))
				ext = 2;

			/* Cope with blank lines */
			if ((end-deps-ext+1) <= 0)
				continue;
			
			dep = xstrndup ( deps, end - deps - ext + 1 );
			
			current->m_depcnt++;
			current->m_deparr = (char **) xrealloc ( current->m_deparr, sizeof ( char *) * current->m_depcnt );
			current->m_deparr [current->m_depcnt - 1] = dep;		
			
			//printf ( "    %d) %s\n", current->m_depcnt, current->m_deparr [current->m_depcnt -1] );
		}
	
		if ( buffer [l-1] == '\\' )
			continuation_line = 1;
		else
			continuation_line = 0;
	}
	close ( fd );
	
	return first;
}


static struct dep_t *find_dep ( struct dep_t *dt, char *mod )
{
	int lm = xstrlen ( mod );
	int extpos = 0;

	if (( mod [lm-2] == '.' ) && ( mod [lm-1] == 'o' ))
		extpos = 2;

	if ( extpos > 0 )	
		mod [lm - extpos] = 0;

	while ( dt ) {
		if ( !strcmp ( dt->m_module, mod ))
			break;

		dt = dt->m_next;
	}
	if ( extpos > 0 )
		mod [lm - extpos] = '.';

	return dt;
}

#define MODPROBE_EXECUTE	0x1
#define MODPROBE_INSERT		0x2
#define MODPROBE_REMOVE		0x4

static void check_dep ( char *mod, int do_syslog, 
		int show_only, int verbose, int flags )
{
	static struct dep_t *depend = (struct dep_t *) -1;	
	struct dep_t *dt;
	
	if ( depend == (struct dep_t *) -1 )
		depend = build_dep ( );
	
	if (( dt = find_dep ( depend, mod ))) {
		int i;
			
		for ( i = 0; i < dt->m_depcnt; i++ ) 
			check_dep ( dt->m_deparr [i], do_syslog, 
					show_only, verbose, flags|MODPROBE_EXECUTE);
	}
	if ( flags & MODPROBE_EXECUTE ) {
		char lcmd [256];
		if ( flags & MODPROBE_INSERT ) {
			snprintf(lcmd, sizeof(lcmd)-1, "insmod %s -q -k %s 2>/dev/null", 
					do_syslog ? "-s" : "", mod );
		}
		if ( flags & MODPROBE_REMOVE ) {
			snprintf(lcmd, sizeof(lcmd)-1, "insmod %s -q -k %s 2>/dev/null", 
					do_syslog ? "-s" : "", mod );
		}
		if ( flags & (MODPROBE_REMOVE|MODPROBE_INSERT) ) {
			if (verbose)
				printf("%s\n", lcmd);
			if (!show_only)
				system ( lcmd );
		}
	}
}	

#endif
		

extern int modprobe_main(int argc, char** argv)
{
	int	ch, rc = 0;
	int	loadall = 0, showconfig = 0, debug = 0, autoclean = 0, list = 0;
	int	show_only = 0, quiet = 0, remove_opt = 0, do_syslog = 0, verbose = 0;
	char	*load_type = NULL, *config = NULL;
	char cmd[256];

	while ((ch = getopt(argc, argv, "acdklnqrst:vVC:")) != -1)
		switch(ch) {
		case 'a':
			loadall++;
			break;
		case 'c':
			showconfig++;
			break;
		case 'd':
			debug++;
			break;
		case 'k':
			autoclean++;
			break;
		case 'l':
			list++;
			break;
		case 'n':
			show_only++;
			break;
		case 'q':
			quiet++;
			break;
		case 'r':
			remove_opt++;
			break;
		case 's':
			do_syslog++;
			break;
		case 't':
			load_type = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'C':
			config = optarg;
			break;
		case 'V':
		default:
			show_usage();
			break;
		}

	if (load_type || config) {
		fprintf(stderr, "-t and -C not supported\n");
		exit(EXIT_FAILURE);
	}

	if (showconfig)
		exit(EXIT_SUCCESS);
	
	if (list)
		exit(EXIT_SUCCESS);
	
	if (remove_opt) {
		do {
			snprintf(cmd, sizeof(cmd)-1, "rmmod %s %s %s",
					optind >= argc ? "-a" : "",
					do_syslog ? "-s" : "",
					optind < argc ? argv[optind] : "");
			if (do_syslog)
				syslog(LOG_INFO, "%s", cmd);
			if (verbose)
				printf("%s\n", cmd);
			if (!show_only)
				rc = system(cmd);
				
#ifdef BB_FEATURE_MODPROBE_DEPEND
			if ( optind < argc )
				check_dep ( argv [optind], do_syslog, show_only, verbose, MODPROBE_REMOVE);
#endif				
		} while (++optind < argc);
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fprintf(stderr, "No module or pattern provided\n");
		exit(EXIT_FAILURE);
	}

	snprintf(cmd, sizeof(cmd)-1, "insmod %s %s %s",
			do_syslog ? "-s" : "",
			quiet ? "-q" : "",
			autoclean ? "-k" : "");

#ifdef BB_FEATURE_MODPROBE_DEPEND
	check_dep ( argv [optind], do_syslog, show_only, verbose, MODPROBE_INSERT);
#endif

	while (optind < argc) {
		if (strlen(cmd) + 1 + strlen(argv[optind]) >= sizeof(cmd))
		{
			error_msg_and_die("Too many module parameters!\n");
		}
		strcat(cmd, " ");
		strcat(cmd, argv[optind]);
		optind++;
	}
	if (do_syslog)
		syslog(LOG_INFO, "%s", cmd);
	if (verbose)
		printf("%s\n", cmd);
	if (!show_only)
		rc = system(cmd);
	else
		rc = 0;

	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
}


