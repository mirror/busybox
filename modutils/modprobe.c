/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2002 by Robert Griebl, griebl@gmx.de
 * Copyright (c) 2003 by Andrew Dennison, andrew.dennison@motec.com.au
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
	char *  m_options;
	
	int     m_isalias  : 1;
	int     m_reserved : 15;
	
	int     m_depcnt   : 16;	
	char ** m_deparr;
	
	struct dep_t * m_next;
};

struct mod_list_t {
	char *  m_module;
	char *  m_options;
	
	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};


static struct dep_t *depend;
static int autoclean, show_only, quiet, do_syslog, verbose;

int parse_tag_value ( char *buffer, char **ptag, char **pvalue )
{
	char *tag, *value;

	while ( isspace ( *buffer ))
		buffer++;			
	tag = value = buffer;
	while ( !isspace ( *value ))
		value++;
	*value++ = 0;
	while ( isspace ( *value ))
		value++;

	*ptag = tag;
	*pvalue = value;
	
	return bb_strlen( tag ) && bb_strlen( value );
}

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
	if ( bb_strlen ( un.release ) > ( sizeof( buffer ) - 64 )) {
		return 0;
	}
				
	strcpy ( filename, "/lib/modules/" );
	strcat ( filename, un.release );
	strcat ( filename, "/modules.dep" );

	if (( fd = open ( filename, O_RDONLY )) < 0 ) {

		/* Ok, that didn't work.  Fall back to looking in /lib/modules */
		if (( fd = open ( "/lib/modules/modules.dep", O_RDONLY )) < 0 ) {
			return 0;
		}
	}

	while ( reads ( fd, buffer, sizeof( buffer ))) {
		int l = bb_strlen ( buffer );
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
				
				mod = bb_xstrndup ( mods, col - mods - ext );
					
				if ( !current ) {
					first = current = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
				}
				else {
					current-> m_next = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_module  = mod;
				current-> m_options = 0;
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
			
			dep = bb_xstrndup ( deps, end - deps - ext + 1 );
			
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
			
		l = bb_strlen ( buffer );
	
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

				if ( parse_tag_value ( buffer + 6, &alias, &mod )) {
					// fprintf ( stderr, "ALIAS: '%s' -> '%s'\n", alias, mod );
				
					if ( !current ) {
						first = current = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
					}
					else {
						current-> m_next = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
						current = current-> m_next;
					}
					current-> m_module  = bb_xstrdup ( alias );
					current-> m_isalias = 1;
					
					if (( strcmp ( alias, "off" ) == 0 ) || ( strcmp ( alias, "null" ) == 0 )) {
						current-> m_depcnt = 0;
						current-> m_deparr = 0;
					}
					else {
						current-> m_depcnt  = 1;
						current-> m_deparr  = xmalloc ( 1 * sizeof( char * ));
						current-> m_deparr[0] = bb_xstrdup ( mod );
					}
					current-> m_next    = 0;					
				}
			}				
			else if (( strncmp ( buffer, "options", 7 ) == 0 ) && isspace ( buffer [7] )) { 
				char *mod, *opt;
				
				if ( parse_tag_value ( buffer + 8, &mod, &opt )) {
					struct dep_t *dt;
	
					for ( dt = first; dt; dt = dt-> m_next ) {
						if ( strcmp ( dt-> m_module, mod ) == 0 ) 
							break;
					}
					if ( dt ) {
						dt-> m_options = xrealloc ( dt-> m_options, bb_strlen( opt ) + 1 );
						strcpy ( dt-> m_options, opt );
						
						// fprintf ( stderr, "OPTION: '%s' -> '%s'\n", dt-> m_module, dt-> m_options );
					}
				}
			}
		}
	}
	close ( fd );
	
	return first;
}

/* return 1 = loaded, 0 = not loaded, -1 = can't tell */
static int already_loaded (const char *name)
{
	int fd;
	char buffer[256];

	fd = open ("/proc/modules", O_RDONLY);
	if (fd < 0)
		return -1;

	while ( reads ( fd, buffer, sizeof( buffer ))) {
		char *p;

		p = strchr (buffer, ' ');
		if (p) {
			*p = 0;
			if (strcmp (name, buffer) == 0) {
				close (fd);
				return 1;
			}
		}
	}

	close (fd);
	return 0;
}

static int mod_process ( struct mod_list_t *list, int do_insert )
{
	char lcmd [256];
	int rc = 1;

	while ( list ) {
		*lcmd = '\0';
		if ( do_insert ) {
			if (already_loaded (list->m_module) != 1)
				snprintf ( lcmd, sizeof( lcmd ) - 1, "insmod %s %s %s %s %s", do_syslog ? "-s" : "", autoclean ? "-k" : "", quiet ? "-q" : "", list-> m_module, list-> m_options ? list-> m_options : "" );
		} else {
			if (already_loaded (list->m_module) != 0)
				snprintf ( lcmd, sizeof( lcmd ) - 1, "rmmod %s %s", do_syslog ? "-s" : "", list-> m_module );
		}
		
		if ( verbose )
			printf ( "%s\n", lcmd );
		if ( !show_only && *lcmd) {
			int rc2 = system ( lcmd );
			if (do_insert) rc = rc2; /* only last module matters */
			else if (!rc2) rc = 0; /* success if remove any mod */
		}
			
		list = do_insert ? list-> m_prev : list-> m_next;
	}
	return (show_only) ? 0 : rc;
}

static void check_dep ( char *mod, struct mod_list_t **head, struct mod_list_t **tail )
{
	struct mod_list_t *find;
	struct dep_t *dt;
	char *opt = 0;
	int lm;

	// remove .o extension
	lm = bb_strlen ( mod );
	if (( mod [lm-2] == '.' ) && ( mod [lm-1] == 'o' ))
		mod [lm-2] = 0;

	// check dependencies
	for ( dt = depend; dt; dt = dt-> m_next ) {
		if ( strcmp ( dt-> m_module, mod ) == 0 ) {
			opt = dt-> m_options;
			break;
		}
	}
	
	// resolve alias names
	while ( dt && dt-> m_isalias ) {
		if ( dt-> m_depcnt == 1 ) {
			struct dep_t *adt;
		
			for ( adt = depend; adt; adt = adt-> m_next ) {
				if ( strcmp ( adt-> m_module, dt-> m_deparr [0] ) == 0 )
					break;
			}
			if ( adt ) {
				dt = adt;			
				mod = dt-> m_module;
				if ( !opt )
					opt = dt-> m_options;
			}
			else
				return;
		}
		else
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
		find-> m_options = opt;
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
	int rc;
	
	// get dep list for module mod
	check_dep ( mod, &head, &tail );
	
	if ( head && tail ) {
		if ( argc ) {
			int i;		
			int l = 0;
	
			// append module args
			for ( i = 0; i < argc; i++ ) 
				l += ( bb_strlen ( argv [i] ) + 1 );
		
			head-> m_options = xrealloc ( head-> m_options, l + 1 );
			head-> m_options [0] = 0;
		
			for ( i = 0; i < argc; i++ ) {
				strcat ( head-> m_options, argv [i] );
				strcat ( head-> m_options, " " );
			}
		}
		
		// process tail ---> head
		rc = mod_process ( tail, 1 );
	}
	else
		rc = 1;
	
	return rc;
}

static int mod_remove ( char *mod )
{
	int rc;
	static struct mod_list_t rm_a_dummy = { "-a", 0, 0 }; 
	
	struct mod_list_t *head = 0;
	struct mod_list_t *tail = 0;
	
	if ( mod )
		check_dep ( mod, &head, &tail );
	else  // autoclean
		head = tail = &rm_a_dummy;
	
	if ( head && tail )
		rc = mod_process ( head, 0 );  // process head ---> tail
	else
		rc = 1;
	return rc;
	
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
			bb_error_msg_and_die("-t and -C not supported");

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
			bb_show_usage();
			break;
		}
	}
	
	depend = build_dep ( );	

	if ( !depend ) 
		bb_error_msg_and_die ( "could not parse modules.dep\n" );
	
	if (remove_opt) {
		int rc = EXIT_SUCCESS;
		do {
			if (mod_remove ( optind < argc ?
					 bb_xstrdup (argv [optind]) : NULL )) {
				bb_error_msg ("failed to remove module %s",
					argv [optind] );
				rc = EXIT_FAILURE;
			}
		} while ( ++optind < argc );
		
		return rc;
	}

	if (optind >= argc) 
		bb_error_msg_and_die ( "No module or pattern provided\n" );
	
	if ( mod_insert ( bb_xstrdup ( argv [optind] ), argc - optind - 1, argv + optind + 1 )) 
		bb_error_msg_and_die ( "failed to load module %s", argv [optind] );
	
	return EXIT_SUCCESS;
}


