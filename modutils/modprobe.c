/* vi: set sw=4 ts=4: */
/*
 * really dumb modprobe implementation for busybox
 * Copyright (C) 2001 Lineo, davidm@lineo.com
 *
 * dependency specific stuff completly rewritten and
 * copyright (c) 2002 by Robert Griebl, griebl@gmx.de
 *
 */

#include <sys/utsname.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include "busybox.h"



struct dep_t {
	char *  m_module;
	
	int     m_depcnt;
	char ** m_deparr;
	
	struct dep_t * m_next;
};

struct mod_list_t {
	char *  m_module;
	
	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};


static struct dep_t *depend = 0;


static struct dep_t *build_dep ( void )
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
		int l = xstrlen ( buffer );
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
					current-> m_next = (struct dep_t *) malloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_module = mod;
				current-> m_depcnt = 0;
				current-> m_deparr = 0;
				current-> m_next   = 0;
						
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
			
			dep = xstrndup ( deps, end - deps - ext + 1 );
			
			current-> m_depcnt++;
			current-> m_deparr = (char **) xrealloc ( current-> m_deparr, sizeof ( char *) * current-> m_depcnt );
			current-> m_deparr [current-> m_depcnt - 1] = dep;		
			
			//printf ( "    %d) %s\n", current-> m_depcnt, current-> m_deparr [current-> m_depcnt -1] );
		}
	
		if ( buffer [l-1] == '\\' )
			continuation_line = 1;
		else
			continuation_line = 0;
	}
	fclose ( f );
	
	return first;
}


static int mod_process ( struct mod_list_t *list, int do_insert, int autoclean, int quiet, int do_syslog, int show_only, int verbose )
{
	char lcmd [256];
	int rc = 0;

	if ( !list )
		return 1;

	while ( list ) {
		if ( do_insert )
			snprintf ( lcmd, sizeof( lcmd ) - 1, "insmod %s %s %s %s 2>/dev/null", do_syslog ? "-s" : "", autoclean ? "-k" : "", quiet ? "-q" : "", list-> m_module );
		else
			snprintf ( lcmd, sizeof( lcmd ) - 1, "rmmod %s %s 2>/dev/null", do_syslog ? "-s" : "", list-> m_module );
		
		if ( verbose )
			printf ( "%s\n", lcmd );
		if ( !show_only )
			rc |= system ( lcmd );
			
		list = do_insert ? list-> m_prev : list-> m_next;
	}
	return rc;
}

static void check_dep ( char *mod, struct mod_list_t **head, struct mod_list_t **tail )
{
	struct mod_list_t *find;
	struct dep_t *dt;

	int lm;

	// remove .o extension
	lm = xstrlen ( mod );
	if (( mod [lm-2] == '.' ) && ( mod [lm-1] == 'o' ))
		mod [lm-2] = 0;

	//printf ( "check_dep: %s\n", mod );

	// search for duplicates
	for ( find = *head; find; find = find-> m_next ) {
		if ( !strcmp ( mod, find-> m_module )) {
			// found -> dequeue it

			if ( find-> m_prev )
				find-> m_prev-> m_next = find-> m_next;
			else
				*head = find-> m_next;
					
			if ( find-> m_next )
				find-> m_next-> m_prev = find-> m_prev;
			else
				*tail = find-> m_prev;
					
			//printf ( "DUP\n" );
					
			break; // there can be only one duplicate
		}				
	}

	if ( !find ) { // did not find a duplicate
		find = (struct mod_list_t *) xmalloc ( sizeof(struct mod_list_t));		
		find-> m_module = mod;
	}

	// enqueue at tail	
	if ( *tail )
		(*tail)-> m_next = find;
	find-> m_prev   = *tail;
	find-> m_next   = 0;
	
	if ( !*head )
		*head = find;
	*tail = find;
		
	// check dependencies
	for ( dt = depend; dt; dt = dt-> m_next ) {
		if ( !strcmp ( dt-> m_module, mod )) {
			int i;
			
			for ( i = 0; i < dt-> m_depcnt; i++ )
				check_dep ( dt-> m_deparr [i], head, tail );
		}
	}		
}



static int mod_insert ( char *mod, int argc, char **argv, int autoclean, int quiet, int do_syslog, int show_only, int verbose )
{
	struct mod_list_t *tail = 0;
	struct mod_list_t *head = 0; 	
	int rc = 0;
	
	// get dep list for module mod
	check_dep ( mod, &head, &tail );
	
	if ( head && tail ) {
		int i;
	
		// append module args
		head-> m_module = xmalloc ( 256 );
		strcpy ( head-> m_module, mod );
		
		for ( i = 0; i < argc; i++ ) {
			strcat ( head-> m_module, " " );
			strcat ( head-> m_module, argv [i] );
		}

		// process tail ---> head
		rc |= mod_process ( tail, 1, autoclean, quiet, do_syslog, show_only, verbose );
	}
	else
		rc = 1;
	
	return rc;
}

static void mod_remove ( char *mod, int do_syslog, int show_only, int verbose )
{
	static struct mod_list_t rm_a_dummy = { "-a", 0, 0 }; 
	
	struct mod_list_t *head = 0;
	struct mod_list_t *tail = 0;
	
	if ( mod )
		check_dep ( mod, &head, &tail );
	else  // autoclean
		head = tail = &rm_a_dummy;
	
	if ( head && tail ) {
		// process head ---> tail
		mod_process ( head, 0, 0, 0, do_syslog, show_only, verbose );
	}
}



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
	
	depend = build_dep ( );	

	if ( !depend ) {
		fprintf (stderr, "could not parse modules.dep\n" );
		exit (EXIT_FAILURE);
	}
	
	if (remove_opt) {
		do {
			mod_remove ( optind < argc ? argv [optind] : 0, do_syslog, show_only, verbose );
		} while ( ++optind < argc );
		
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fprintf(stderr, "No module or pattern provided\n");
		exit(EXIT_FAILURE);
	}
	
	rc = mod_insert ( argv [optind], argc - optind - 1, argv + optind + 1, autoclean, quiet, do_syslog, show_only, verbose );

	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
}


