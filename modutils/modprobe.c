/* vi: set sw=4 ts=4: */
/*
 * really dumb modprobe implementation for busybox
 * Copyright (C) 2001 Lineo, davidm@lineo.com
 *
 * CONFIG_MODPROBE_DEPEND stuff was added and is
 * Copyright (C) 2002 Robert Griebl, griebl@gmx.de
 *
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include "busybox.h"

static char cmd[128];

#define CONFIG_MODPROBE_DEPEND

#ifdef CONFIG_MODPROBE_DEPEND

#include <sys/utsname.h>

struct dep_t {
	char *  m_module;
	int     m_depcnt;
	char ** m_deparr;
	
	struct dep_t * m_next;
};


static struct dep_t *build_dep ( )
{
	struct utsname un;
	FILE *f;
	struct dep_t *first = 0;
	struct dep_t *current = 0;
	char buffer [4096];
	char *filename = buffer;
	int continuation_line = 0;
	
	if ( uname ( &un ))
		return 0;
	strcpy ( filename, "/lib/modules/" );
	strcat ( filename, un. release );
	strcat ( filename, "/modules.dep" );

	f = fopen ( filename, "r" );
	if ( !f )
		return 0;
	
	while ( fgets ( buffer, sizeof( buffer), f )) {
		int l = strlen ( buffer );
		char *p = 0;
		
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
				
				*col = 0;
				mods = strrchr ( buffer, '/' );
				
				if ( !mods )
					mods = buffer;
				else
					mods++;
					
				mod = (char *) malloc ( col - mods + 1 );	
				strncpy ( mod, mods, col - mods );
				mod [col - mods] = 0;

				if ( !current ) {
					first = current = (struct dep_t *) malloc ( sizeof ( struct dep_t ));
				}
				else {
					current-> m_next = (struct dep_t *) malloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_module = mod;
				current-> m_depcnt = 0;
				current-> m_deparr = 0;
				current-> m_next = 0;
						
				/* printf ( "%s:\n", mod );	 */
						
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
			
			dep = (char *) malloc ( end - deps + 2 );
			strncpy ( dep, deps, end - deps + 1 );
			dep [end - deps + 1] = 0;
			
			current-> m_depcnt++;
			current-> m_deparr = (char **) realloc ( current-> m_deparr, sizeof ( char *) * current-> m_depcnt );
			current-> m_deparr [current-> m_depcnt - 1] = dep;		
			
			/* printf ( "    %d) %s\n", current-> m_depcnt, current-> m_deparr [current-> m_depcnt -1] ); */
		}
	
		if ( buffer [l-1] == '\\' )
			continuation_line = 1;
		else
			continuation_line = 0;
	}
	fclose ( f );
	
	return first;
}


static struct dep_t *find_dep ( struct dep_t *dt, char *mod )
{
	int lm = strlen ( mod );
	int hasext = 0;
	
	if (( mod [lm-2] == '.' ) && ( mod [lm-1] == 'o' ))
		hasext = 1;
	
	while ( dt ) {
		if ( hasext && !strcmp ( dt-> m_module, mod ))
			break;
		else if ( !hasext && !strncmp ( dt-> m_module, mod, strlen ( dt-> m_module ) - 2 ))
			break;
			
		dt = dt-> m_next;
	}
	return dt;
}


static void check_dep ( char *mod, int do_syslog, 
		int show_only, int verbose, int recursing )
{
	static struct dep_t *depend = (struct dep_t *) -1;	
	struct dep_t *dt;
	
	if ( depend == (struct dep_t *) -1 )
		depend = build_dep ( );
	
	/* printf ( "CHECK: %s (%p)\n", mod, depend ); */
	
	if (( dt = find_dep ( depend, mod ))) {
		int i;
			
		for ( i = 0; i < dt-> m_depcnt; i++ ) 
			check_dep ( dt-> m_deparr [i], do_syslog, 
					show_only, verbose, 1);
	}
	if ( recursing  ) {
		char lcmd [256];
		
		snprintf(lcmd, sizeof(lcmd)-1, "insmod %s -q -k %s 2>/dev/null", 
				do_syslog ? "-s" : "", mod );
		if (show_only || verbose)
			printf("%s\n", lcmd);
		if (!show_only)
			system ( lcmd );	
	}
}	

#endif
		

extern int modprobe_main(int argc, char** argv)
{
	int	ch, rc = 0;
	int	loadall = 0, showconfig = 0, debug = 0, autoclean = 0, list = 0;
	int	show_only = 0, quiet = 0, remove_opt = 0, do_syslog = 0, verbose = 0;
	char	*load_type = NULL, *config = NULL;

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
			sprintf(cmd, "rmmod %s %s %s",
					optind >= argc ? "-a" : "",
					do_syslog ? "-s" : "",
					optind < argc ? argv[optind] : "");
			if (do_syslog)
				syslog(LOG_INFO, "%s", cmd);
			if (show_only || verbose)
				printf("%s\n", cmd);
			if (!show_only)
				rc = system(cmd);
		} while (++optind < argc);
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fprintf(stderr, "No module or pattern provided\n");
		exit(EXIT_FAILURE);
	}

	sprintf(cmd, "insmod %s %s %s",
			do_syslog ? "-s" : "",
			quiet ? "-q" : "",
			autoclean ? "-k" : "");

#ifdef CONFIG_MODPROBE_DEPEND
	check_dep ( argv [optind], do_syslog, show_only, verbose, 0);
#endif

	while (optind < argc) {
		strcat(cmd, " ");
		strcat(cmd, argv[optind]);
		optind++;
	}
	if (do_syslog)
		syslog(LOG_INFO, "%s", cmd);
	if (show_only || verbose)
		printf("%s\n", cmd);
	if (!show_only)
		rc = system(cmd);
	else
		rc = 0;

	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
}


