/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2002 by Robert Griebl, griebl@gmx.de
 * Copyright (c) 2003 by Andrew Dennison, andrew.dennison@motec.com.au
 * Copyright (c) 2005 by Jim Bauer, jfbauer@nfr.com
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "busybox.h"



struct dep_t {
	char *  m_name;
	char *  m_path;
	char *  m_options;

	int     m_isalias  : 1;
	int     m_reserved : 15;

	int     m_depcnt   : 16;
	char ** m_deparr;

	struct dep_t * m_next;
};

struct mod_list_t {
	char *  m_name;
	char *  m_path;
	char *  m_options;

	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};


static struct dep_t *depend;
static int autoclean, show_only, quiet, do_syslog, verbose;
static int k_version;

static int parse_tag_value ( char *buffer, char **ptag, char **pvalue )
{
	char *tag, *value;

	while ( isspace ( *buffer ))
		buffer++;
	tag = value = buffer;
	while ( !isspace ( *value ))
		if (!*value) return 0;
		else value++;
	*value++ = 0;
	while ( isspace ( *value ))
		value++;
	if (!*value) return 0;

	*ptag = tag;
	*pvalue = value;

	return 1;
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
	char buffer[2048];
	char *filename = buffer;
	int continuation_line = 0;

	k_version = 0;
	if ( uname ( &un ))
		return 0;

	// check for buffer overflow in following code
	if ( bb_strlen ( un.release ) > ( sizeof( buffer ) - 64 )) {
		return 0;
	}
	if (un.release[0] == '2') {
		k_version = un.release[2] - '0';
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
			char *dot = col;

			if ( col ) {
				char *mods;
				char *modpath;
				char *mod;

				*col = 0;
				mods = strrchr ( buffer, '/' );

				if ( !mods )
					mods = buffer;
				else
					mods++;

				modpath = strchr ( buffer, '/' );
				if ( !modpath )
					modpath = buffer;
#if defined(CONFIG_FEATURE_2_6_MODULES)
				if ((k_version > 4) && ( *(col-3) == '.' ) &&
						( *(col-2) == 'k' ) && ( *(col-1) == 'o' ))
					dot = col - 3;
				else
#endif
					if (( *(col-2) == '.' ) && ( *(col-1) == 'o' ))
						dot = col - 2;

				mod = bb_xstrndup ( mods, dot - mods );

				if ( !current ) {
					first = current = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
				}
				else {
					current-> m_next = (struct dep_t *) xmalloc ( sizeof ( struct dep_t ));
					current = current-> m_next;
				}
				current-> m_name  = mod;
				current-> m_path  = bb_xstrdup(modpath);
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

		while ( p && *p && isblank(*p))
			p++;

		if ( p && *p ) {
			char *end = &buffer [l-1];
			char *deps;
			char *dep;
			char *next;
			int ext = 0;

			while ( isblank ( *end ) || ( *end == '\\' ))
				end--;

			do
			{
				next = strchr (p, ' ' );
				if (next)
				{
					*next = 0;
					next--;
				}
				else
					next = end;

				deps = strrchr ( p, '/' );

				if ( !deps || ( deps < p )) {
					deps = p;

					while ( isblank ( *deps ))
						deps++;
				}
				else
					deps++;

#if defined(CONFIG_FEATURE_2_6_MODULES)
				if ((k_version > 4) && ( *(next-2) == '.' ) && *(next-1) == 'k'  &&
						( *next == 'o' ))
					ext = 3;
				else
#endif
					if (( *(next-1) == '.' ) && ( *next == 'o' ))
						ext = 2;

				/* Cope with blank lines */
				if ((next-deps-ext+1) <= 0)
					continue;
				dep = bb_xstrndup ( deps, next - deps - ext + 1 );

				current-> m_depcnt++;
				current-> m_deparr = (char **) xrealloc ( current-> m_deparr,
						sizeof ( char *) * current-> m_depcnt );
				current-> m_deparr [current-> m_depcnt - 1] = dep;

				//printf ( "    %d) %s\n", current-> m_depcnt, current-> m_deparr [current-> m_depcnt -1] );
				p = next + 2;
			} while (next < end);
		}

		if ( buffer [l-1] == '\\' )
			continuation_line = 1;
		else
			continuation_line = 0;
	}
	close ( fd );

	// alias parsing is not 100% correct (no correct handling of continuation lines within an alias) !

#if defined(CONFIG_FEATURE_2_6_MODULES)
	if (( fd = open ( "/etc/modprobe.conf", O_RDONLY )) < 0 )
#endif
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
						first = current = (struct dep_t *) xcalloc ( 1, sizeof ( struct dep_t ));
					}
					else {
						current-> m_next = (struct dep_t *) xcalloc ( 1, sizeof ( struct dep_t ));
						current = current-> m_next;
					}
					current-> m_name  = bb_xstrdup ( alias );
					current-> m_isalias = 1;

					if (( strcmp ( mod, "off" ) == 0 ) || ( strcmp ( mod, "null" ) == 0 )) {
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
						if ( strcmp ( dt-> m_name, mod ) == 0 )
							break;
					}
					if ( dt ) {
						dt-> m_options = xrealloc ( dt-> m_options, bb_strlen( opt ) + 1 );
						strcpy ( dt-> m_options, opt );

						// fprintf ( stderr, "OPTION: '%s' -> '%s'\n", dt-> m_name, dt-> m_options );
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
	char buffer[4096];

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

#ifdef CONFIG_MODPROBE_MULTIPLE_OPTIONS
/* static char* parse_command_string( char* src, char **dst );
 *   src: pointer to string containing argument
 *   dst: pointer to where to store the parsed argument
 *   return value: the pointer to the first char after the parsed argument,
 *                 NULL if there was no argument parsed (only trailing spaces).
 *   Note that memory is allocated with bb_xstrdup when a new argument was
 *   parsed. Don't forget to free it!
 */
#define ARG_EMPTY      0x00
#define ARG_IN_DQUOTES 0x01
#define ARG_IN_SQUOTES 0x02
static char *parse_command_string( char *src, char **dst )
{
	int opt_status = ARG_EMPTY;
	char* tmp_str;

	/* Dumb you, I have nothing to do... */
	if( src == NULL ) return src;

	/* Skip leading spaces */
	while( *src == ' ' ) {
		src++;
	}
	/* Is the end of string reached? */
	if( *src == '\0' ) {
		return NULL;
	}
	/* Reached the start of an argument
	 * By the way, we duplicate a little too much here :-/ but that's the easy way:
	 * cost effective wrt code, cost consumming wrt memory usage. */
	*dst = tmp_str = bb_xstrdup( src );
	/* Get to the end of that argument */
	while(    ( *tmp_str != '\0' )
		   && (    ( *tmp_str != ' ' )
				|| ( opt_status & ( ARG_IN_DQUOTES | ARG_IN_SQUOTES ) ) ) ) {
		switch( *tmp_str ) {
			case '\'':
				if( opt_status & ARG_IN_DQUOTES ) {
					/* Already in double quotes, keep current char as is */
				} else {
					/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
					memmove( tmp_str, tmp_str + 1, strlen( tmp_str ) );
					/* mark me: we enter or leave single quotes */
					opt_status ^= ARG_IN_SQUOTES;
					/* Back one char, as we need to re-scan the new char there. */
					tmp_str--;
				}
			break;
			case '"':
				if( opt_status & ARG_IN_SQUOTES ) {
					/* Already in single quotes, keep current char as is */
				} else {
					/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
					memmove( tmp_str, tmp_str + 1, strlen( tmp_str ) );
					/* mark me: we enter or leave double quotes */
					opt_status ^= ARG_IN_DQUOTES;
					/* Back one char, as we need to re-scan the new char there. */
					tmp_str--;
				}
			break;
			case '\\':
				if( opt_status & ARG_IN_SQUOTES ) {
					/* Between single quotes: keep as is. */
				} else {
					switch( *(tmp_str+1) ) {
						case 'a':
						case 'b':
						case 't':
						case 'n':
						case 'v':
						case 'f':
						case 'r':
						case '0':
							/* We escaped a special character. For now, keep
							 * both the back-slash and the following char. */
							tmp_str++; src++;
							break;
						default:
							/* We escaped a space or a single or double quote,
							 * or a back-slash, or a non-escapable char. Remove
							 * the '\' and keep the new current char as is. */
							memmove( tmp_str, tmp_str + 1, strlen( tmp_str ) );
							break;
					}
				}
			break;
			/* Any other char that is special shall appear here.
			 * Example: $ starts a variable
			case '$':
				do_variable_expansion();
				break;
			 * */
			default:
				/* any other char is kept as is. */
				break;
		}
		tmp_str++; /* Go to next char */
		src++; /* Go to next char to find the end of the argument. */
	}
	/* End of string, but still no ending quote */
	if( opt_status & ( ARG_IN_DQUOTES | ARG_IN_SQUOTES ) ) {
		bb_error_msg_and_die( "unterminated (single or double) quote in options list: %s", src );
	}
	*tmp_str = '\0';
	return src;
}
#endif /* CONFIG_MODPROBE_MULTIPLE_OPTIONS */

static int mod_process ( struct mod_list_t *list, int do_insert )
{
	int rc = 0;
#ifdef CONFIG_MODPROBE_MULTIPLE_OPTIONS
	char **argv = NULL;
	char *opts;
#ifdef CONFIG_FEATURE_CLEAN_UP
	int argc_malloc;
#endif
#else /* CONFIG_MODPROBE_MULTIPLE_OPTIONS */
	char *argv[10];
#endif
	int argc;

	while ( list ) {
		argc = 0;
#ifdef CONFIG_MODPROBE_MULTIPLE_OPTIONS
#ifdef CONFIG_FEATURE_CLEAN_UP
		argc_malloc = 0;
#endif
		argv = (char**) malloc( 6 * sizeof( char* ) ); /* enough for minimal insmod (5 args + NULL) or rmmod (3 args + NULL) */
#endif
		if ( do_insert ) {
			if (already_loaded (list->m_name) != 1) {
				argv[argc++] = "insmod";
				if (do_syslog)
					argv[argc++] = "-s";
				if (autoclean)
					argv[argc++] = "-k";
				if (quiet)
					argv[argc++] = "-q";
				else if(verbose) /* verbose and quiet are mutually exclusive */
					argv[argc++] = "-v";
				argv[argc++] = list-> m_path;
#ifdef CONFIG_MODPROBE_MULTIPLE_OPTIONS
#ifdef CONFIG_FEATURE_CLEAN_UP
				argc_malloc = argc;
#endif
				opts = list-> m_options;
				while( ( opts = parse_command_string( opts, &(argv[argc]) ) ) != NULL ) {
					/* Increase the argv array by 1 */
					argc++;
					argv = (char**) xrealloc( argv, ( argc + 1 ) * sizeof( char* ) );
				}
#else /* CONFIG_MODPROBE_MULTIPLE_OPTIONS */
				if (list-> m_options)
					argv[argc++] = list-> m_options;
#endif /* CONFIG_MODPROBE_MULTIPLE_OPTIONS */
			}
		} else {
			/* modutils uses short name for removal */
			if (already_loaded (list->m_name) != 0) {
				argv[argc++] = "rmmod";
				if (do_syslog)
					argv[argc++] = "-s";
				argv[argc++] = list->m_name;
#if ( defined CONFIG_MODPROBE_MULTIPLE_OPTIONS ) && ( defined CONFIG_FEATURE_CLEAN_UP )
				argc_malloc = argc;
#endif
			}
		}
		argv[argc] = NULL;

		if (argc) {
			if (verbose) {
				int i;
				printf("argc=%d\n", argc );
				for (i=0; i<argc; i++)
				  printf("argv[%02d]=\"%s\"\n", i, argv[i]);
			}
			if (!show_only) {
				int rc2 = 0;
				int status;
				switch (fork()) {
				case -1:
					rc2 = 1;
					break;
				case 0: //child
					execvp(argv[0], argv);
					bb_perror_msg_and_die("exec of %s", argv[0]);
					/* NOTREACHED */
				default:
					if (wait(&status) == -1) {
						rc2 = 1;
						break;
					}
					if (WIFEXITED(status))
						rc2 = WEXITSTATUS(status);
					if (WIFSIGNALED(status))
						rc2 = WTERMSIG(status);
					break;
				}
				if (do_insert) {
					rc = rc2; /* only last module matters */
				}
				else if (!rc2) {
					rc = 0; /* success if remove any mod */
				}
			}
#if ( defined CONFIG_MODPROBE_MULTIPLE_OPTIONS ) && ( defined CONFIG_FEATURE_CLEAN_UP )
			/* the last value in the array has index == argc, but
			 * it is the terminatign NULL, so we must not free it. */
			while( argc_malloc < argc ) {
				free( argv[argc_malloc++] );
			}
		}
		free( argv );
		/* If CONFIG_FEATURE_CLEAN_UP is not defined, then we leak memory
		 * here. But it is (quite) small amounts of memory that leak each
		 * time a module is loaded,  and it is reclaimed when modprobe
		 * exits anyway.
		 * This could become a problem when loading a module with LOTS of
		 * dependencies, with LOTS of options for each dependencies, with
		 * very little memory on the target... But in that case, the module
		 * would not load because there is no more memory, so there's no
		 * problem. Hmm, wait... Is this true, whatever the allocation policy? */
		argv = NULL;
#else /* CONFIG_MODPROBE_MULTIPLE_OPTIONS && CONFIG_FEATURE_CLEAN_UP */
		}
#endif
		list = do_insert ? list-> m_prev : list-> m_next;
	}
	return (show_only) ? 0 : rc;
}

static void check_dep ( char *mod, struct mod_list_t **head, struct mod_list_t **tail )
{
	struct mod_list_t *find;
	struct dep_t *dt;
	char *opt = 0;
	char *path = 0;

	// check dependencies
	for ( dt = depend; dt; dt = dt-> m_next ) {
		if ( strcmp ( dt-> m_name, mod ) == 0) {
			mod = dt-> m_name;
			path = dt-> m_path;
			opt = dt-> m_options;
			break;
		}
	}

	// resolve alias names
	while ( dt && dt-> m_isalias ) {
		if ( dt-> m_depcnt == 1 ) {
			struct dep_t *adt;

			for ( adt = depend; adt; adt = adt-> m_next ) {
				if ( strcmp ( adt-> m_name, dt-> m_deparr [0] ) == 0 )
					break;
			}
			if ( adt ) {
				dt = adt;
				mod = dt-> m_name;
				path = dt-> m_path;
				if ( !opt )
					opt = dt-> m_options;
			}
			else
				return;
		}
		else
			return;
	}

	if ( !path ) {
		bb_error_msg ("module %s not found.", mod);
		return;
	}

	// search for duplicates
	for ( find = *head; find; find = find-> m_next ) {
		if ( !strcmp ( mod, find-> m_name )) {
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
		find-> m_name = mod;
		find-> m_path = path;
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
#if defined(CONFIG_FEATURE_2_6_MODULES)
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
#endif

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
				quiet++; verbose=0;
				break;
			case 'r':
				remove_opt++;
				break;
			case 's':
				do_syslog++;
				break;
			case 'v':
				verbose++; quiet=0;
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
