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
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "busybox.h"



struct dep_t {
	char *  m_module;
	
	int     m_isalias  : 1;
	int     m_reserved : 15;
	
	int     m_depcnt   : 16;	
	char ** m_deparr;
	
	struct dep_t * m_next;
};

struct mod_list_t {
	char *  m_module;
	
	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};


static struct dep_t *depend;
static int autoclean, show_only, quiet, do_syslog, verbose;


/* Jump through hoops to simulate how fgets() grabs just one line at a
 * time... Don't use any stdio since modprobe gets called from a kernel
 * thread and stdio junk can overflow the limited stack... 
 */
static char *reads ( int fd, char *buffer, size_t len )
{
	int n = read ( fd, buffer, len );
	
	if ( n > 0 ) {
		char *p;
	
		buffer [len-1] = 0;
		p = strchr ( buffer, '\n' );
		
		if ( p ) {
			off_t offset;
			
			offset = lseek ( fd, 0L, SEEK_CUR );               // Get the current file descriptor offset 
			lseek ( fd, offset-n + (p-buffer) + 1, SEEK_SET ); // Set the file descriptor offset to right after the \n

			p[1] = 0;
		}
		return buffer;
	}
	
	else
		return 0;
}

static struct dep_t *build_dep ( void )
{
	int fd;
	struct utsname un;
	struct dep_t *first = 0;
	struct dep_t *current = 0;
	char buffer[256];
	char *filename = buffer;
	int continuation_line = 0;
	
	if ( uname ( &un ))
		return 0;
		
	// check for buffer overflow in following code
	if ( xstrlen ( un.release ) > ( sizeof( buffer ) - 64 )) {
		return 0;
	}
				
	strcpy ( filename, "/lib/modules/" );
	strcat ( filename, un.release );
	strcat ( filename, "/modules.dep" );

	if (( fd = open ( filename, O_RDONLY )) < 0 )
		return 0;

	while ( reads ( fd, buffer, sizeof( buffer ))) {
		int l = xstrlen ( buffer );
		char *p = 0;
		
		while ( isspace ( buffer [l-1] )) {
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
					first = current = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
				}
				else {
					current-> m_next = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_module  = mod;
				current-> m_isalias = 0;
				current-> m_depcnt  = 0;
				current-> m_deparr  = 0;
				current-> m_next    = 0;
						
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
	close ( fd );

	// alias parsing is not 100% correct (no correct handling of continuation lines within an alias) !

	if (( fd = open ( "/etc/modules.conf", O_RDONLY )) < 0 )
		if (( fd = open ( "/etc/conf.modules", O_RDONLY )) < 0 )
			return first;
	
	continuation_line = 0;
	while ( reads ( fd, buffer, sizeof( buffer ))) {
		int l;
		char *p;
		
		p = strchr ( buffer, '#' );	
		if ( p )
			*p = 0;
			
		l = xstrlen ( buffer );
	
		while ( l && isspace ( buffer [l-1] )) {
			buffer [l-1] = 0;
			l--;
		}
		
		if ( l == 0 ) {
			continuation_line = 0;
			continue;
		}
		
		if ( !continuation_line ) {		
			if (( strncmp ( buffer, "alias", 5 ) == 0 ) && isspace ( buffer [5] )) {
				char *alias, *mod;

				alias = buffer + 6;
				
				while ( isspace ( *alias ))
					alias++;			
				mod = alias;					
				while ( !isspace ( *mod ))
					mod++;
				*mod = 0;
				mod++;
				while ( isspace ( *mod ))
					mod++;
										
//					fprintf ( stderr, "ALIAS: '%s' -> '%s'\n", alias, mod );
				
				if ( !current ) {
					first = current = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
				}
				else {
					current-> m_next = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_module  = xstrdup ( alias );
				current-> m_isalias = 1;
				
				if (( strcmp ( alias, "off" ) == 0 ) || ( strcmp ( alias, "null" ) == 0 )) {
					current-> m_depcnt = 0;
					current-> m_deparr = 0;
				}
				else {
					current-> m_depcnt  = 1;
					current-> m_deparr  = xmalloc ( 1 * sizeof( char * ));
					current-> m_deparr[0] = xstrdup ( mod );
				}
				current-> m_next    = 0;					
			}				
		}
	}
	close ( fd );
	
	return first;
}


static int mod_process ( struct mod_list_t *list, int do_insert )
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

	// check dependencies
	for ( dt = depend; dt; dt = dt-> m_next ) {
		if ( strcmp ( dt-> m_module, mod ) == 0 ) 
			break;
	}
	// resolve alias names
	if ( dt && dt-> m_isalias ) {
		if ( dt-> m_depcnt == 1 )
			check_dep ( dt-> m_deparr [0], head, tail );
		printf ( "Got alias: %s -> %s\n", mod, dt-> m_deparr [0] );
			
		return;
	}
	
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
		
	if ( dt ) {	
		int i;
		
		for ( i = 0; i < dt-> m_depcnt; i++ )
			check_dep ( dt-> m_deparr [i], head, tail );
	}
}



static int mod_insert ( char *mod, int argc, char **argv )
{
	struct mod_list_t *tail = 0;
	struct mod_list_t *head = 0; 	
	int rc = 0;
	
	// get dep list for module mod
	check_dep ( mod, &head, &tail );
	
	if ( head && tail ) {
		int i;
		int l = 0;
	
		// append module args
		l = xstrlen ( head-> m_module );
		for ( i = 0; i < argc; i++ ) 
			l += ( xstrlen ( argv [i] ) + 1 );
		
		head-> m_module = xrealloc ( head-> m_module, l + 1 );
		
		for ( i = 0; i < argc; i++ ) {
			strcat ( head-> m_module, " " );
			strcat ( head-> m_module, argv [i] );
		}

		// process tail ---> head
		rc |= mod_process ( tail, 1 );
	}
	else
		rc = 1;
	
	return rc;
}

static void mod_remove ( char *mod )
{
	static struct mod_list_t rm_a_dummy = { "-a", 0, 0 }; 
	
	struct mod_list_t *head = 0;
	struct mod_list_t *tail = 0;
	
	if ( mod )
		check_dep ( mod, &head, &tail );
	else  // autoclean
		head = tail = &rm_a_dummy;
	
	if ( head && tail )
		mod_process ( head, 0 );  // process head ---> tail
}



extern int modprobe_main(int argc, char** argv)
{
	int	opt;
	int remove_opt = 0;

	autoclean = show_only = quiet = do_syslog = verbose = 0;

	while ((opt = getopt(argc, argv, "acdklnqrst:vVC:")) != -1) {
		switch(opt) {
		case 'c': // no config used
		case 'l': // no pattern matching
			return EXIT_SUCCESS;
			break;
		case 'C': // no config used
		case 't': // no pattern matching
			error_msg_and_die("-t and -C not supported");

		case 'a': // ignore
		case 'd': // ignore
			break;
		case 'k':
			autoclean++;
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
		case 'v':
			verbose++;
			break;
		case 'V':
		default:
			show_usage();
			break;
		}
	}
	
	depend = build_dep ( );	

	if ( !depend ) 
		error_msg_and_die ( "could not parse modules.dep\n" );
	
	if (remove_opt) {
		do {
			mod_remove ( optind < argc ? xstrdup ( argv [optind] ) : 0 );
		} while ( ++optind < argc );
		
		return EXIT_SUCCESS;
	}

	if (optind >= argc) 
		error_msg_and_die ( "No module or pattern provided\n" );
	
	return mod_insert ( xstrdup ( argv [optind] ), argc - optind - 1, argv + optind + 1 ) ? \
	       EXIT_FAILURE : EXIT_SUCCESS;
}


