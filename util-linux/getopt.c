/*
 * getopt.c - Enhanced implementation of BSD getopt(1)
 *   Copyright (c) 1997, 1998, 1999, 2000  Frodo Looijaard <frodol@dds.nl>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Version 1.0-b4: Tue Sep 23 1997. First public release.
 * Version 1.0: Wed Nov 19 1997.
 *   Bumped up the version number to 1.0
 *   Fixed minor typo (CSH instead of TCSH)
 * Version 1.0.1: Tue Jun 3 1998
 *   Fixed sizeof instead of strlen bug
 *   Bumped up the version number to 1.0.1
 * Version 1.0.2: Thu Jun 11 1998 (not present)
 *   Fixed gcc-2.8.1 warnings
 *   Fixed --version/-V option (not present)
 * Version 1.0.5: Tue Jun 22 1999
 *   Make -u option work (not present)
 * Version 1.0.6: Tue Jun 27 2000
 *   No important changes
 * Version 1.1.0: Tue Jun 30 2000
 *   Added NLS support (partly written by Arkadiusz Mi<B6>kiewicz
 *     <misiek@misiek.eu.org>)
 * Ported to Busybox - Alfred M. Szmidt <ams@trillian.itslinux.org>
 *  Removed --version/-V and --help/-h in
 *  Removed prase_error(), using bb_error_msg() from Busybox instead
 *  Replaced our_malloc with xmalloc and our_realloc with xrealloc
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>

#include "busybox.h"

/* NON_OPT is the code that is returned when a non-option is found in '+'
   mode */
static const int NON_OPT = 1;
/* LONG_OPT is the code that is returned when a long option is found. */
static const int LONG_OPT = 2;

/* The shells recognized. */
typedef enum {BASH,TCSH} shell_t;


/* Some global variables that tells us how to parse. */
static shell_t shell=BASH; /* The shell we generate output for. */
static int quiet_errors=0; /* 0 is not quiet. */
static int quiet_output=0; /* 0 is not quiet. */
static int quote=1; /* 1 is do quote. */
static int alternative=0; /* 0 is getopt_long, 1 is getopt_long_only */

/* Function prototypes */
static const char *normalize(const char *arg);
static int generate_output(char * argv[],int argc,const char *optstr,
                    const struct option *longopts);
static void add_long_options(char *options);
static void add_longopt(const char *name,int has_arg);
static void set_shell(const char *new_shell);


/*
 * This function 'normalizes' a single argument: it puts single quotes around
 * it and escapes other special characters. If quote is false, it just
 * returns its argument.
 * Bash only needs special treatment for single quotes; tcsh also recognizes
 * exclamation marks within single quotes, and nukes whitespace.
 * This function returns a pointer to a buffer that is overwritten by
 * each call.
 */
const char *normalize(const char *arg)
{
        static char *BUFFER=NULL;
        const char *argptr=arg;
        char *bufptr;

        free(BUFFER);

        if (!quote) { /* Just copy arg */
               BUFFER=bb_xstrdup(arg);
                return BUFFER;
        }

        /* Each character in arg may take upto four characters in the result:
           For a quote we need a closing quote, a backslash, a quote and an
           opening quote! We need also the global opening and closing quote,
           and one extra character for '\0'. */
        BUFFER=xmalloc(strlen(arg)*4+3);

        bufptr=BUFFER;
        *bufptr++='\'';

        while (*argptr) {
                if (*argptr == '\'') {
                        /* Quote: replace it with: '\'' */
                        *bufptr++='\'';
                        *bufptr++='\\';
                        *bufptr++='\'';
                        *bufptr++='\'';
                } else if (shell==TCSH && *argptr=='!') {
                        /* Exclamation mark: replace it with: \! */
                        *bufptr++='\'';
                        *bufptr++='\\';
                        *bufptr++='!';
                        *bufptr++='\'';
                } else if (shell==TCSH && *argptr=='\n') {
                        /* Newline: replace it with: \n */
                        *bufptr++='\\';
                        *bufptr++='n';
                } else if (shell==TCSH && isspace(*argptr)) {
                        /* Non-newline whitespace: replace it with \<ws> */
                        *bufptr++='\'';
                        *bufptr++='\\';
                        *bufptr++=*argptr;
                        *bufptr++='\'';
                } else
                        /* Just copy */
                        *bufptr++=*argptr;
                argptr++;
        }
        *bufptr++='\'';
        *bufptr++='\0';
        return BUFFER;
}

/*
 * Generate the output. argv[0] is the program name (used for reporting errors).
 * argv[1..] contains the options to be parsed. argc must be the number of
 * elements in argv (ie. 1 if there are no options, only the program name),
 * optstr must contain the short options, and longopts the long options.
 * Other settings are found in global variables.
 */
int generate_output(char * argv[],int argc,const char *optstr,
                    const struct option *longopts)
{
        int exit_code = 0; /* We assume everything will be OK */
        int opt;
        int longindex;
        const char *charptr;

        if (quiet_errors) /* No error reporting from getopt(3) */
                opterr=0;
        optind=0; /* Reset getopt(3) */

        while ((opt = (alternative?
                       getopt_long_only(argc,argv,optstr,longopts,&longindex):
                       getopt_long(argc,argv,optstr,longopts,&longindex)))
               != EOF)
                if (opt == '?' || opt == ':' )
                        exit_code = 1;
                else if (!quiet_output) {
                        if (opt == LONG_OPT) {
                                printf(" --%s",longopts[longindex].name);
                                if (longopts[longindex].has_arg)
                                        printf(" %s",
                                               normalize(optarg?optarg:""));
                        } else if (opt == NON_OPT)
                                printf(" %s",normalize(optarg));
                        else {
                                printf(" -%c",opt);
                                charptr = strchr(optstr,opt);
                                if (charptr != NULL && *++charptr == ':')
                                        printf(" %s",
                                               normalize(optarg?optarg:""));
                        }
                }

        if (! quiet_output) {
                printf(" --");
                while (optind < argc)
                        printf(" %s",normalize(argv[optind++]));
                printf("\n");
        }
        return exit_code;
}

static struct option *long_options=NULL;
static int long_options_length=0; /* Length of array */
static int long_options_nr=0; /* Nr of used elements in array */
static const int LONG_OPTIONS_INCR = 10;
#define init_longopt() add_longopt(NULL,0)

/* Register a long option. The contents of name is copied. */
void add_longopt(const char *name,int has_arg)
{
        if (!name) { /* init */
                free(long_options);
                long_options=NULL;
                long_options_length=0;
                long_options_nr=0;
        }

        if (long_options_nr == long_options_length) {
                long_options_length += LONG_OPTIONS_INCR;
                long_options=xrealloc(long_options,
                                         sizeof(struct option) *
                                         long_options_length);
        }

        long_options[long_options_nr].name=NULL;
        long_options[long_options_nr].has_arg=0;
        long_options[long_options_nr].flag=NULL;
        long_options[long_options_nr].val=0;

        if (long_options_nr) { /* Not for init! */
                long_options[long_options_nr-1].has_arg=has_arg;
                long_options[long_options_nr-1].flag=NULL;
                long_options[long_options_nr-1].val=LONG_OPT;
               long_options[long_options_nr-1].name=bb_xstrdup(name);
        }
        long_options_nr++;
}


/*
 * Register several long options. options is a string of long options,
 * separated by commas or whitespace.
 * This nukes options!
 */
void add_long_options(char *options)
{
        int arg_opt, tlen;
        char *tokptr=strtok(options,", \t\n");
        while (tokptr) {
                arg_opt=no_argument;
		tlen=strlen(tokptr);
                if (tlen > 0) {
                        if (tokptr[tlen-1] == ':') {
                                if (tlen > 1 && tokptr[tlen-2] == ':') {
                                        tokptr[tlen-2]='\0';
					tlen -= 2;
                                        arg_opt=optional_argument;
                                } else {
                                        tokptr[tlen-1]='\0';
					tlen -= 1;
                                        arg_opt=required_argument;
                                }
                                if (tlen == 0)
                                        bb_error_msg("empty long option after -l or --long argument");
                        }
                        add_longopt(tokptr,arg_opt);
                }
                tokptr=strtok(NULL,", \t\n");
        }
}

void set_shell(const char *new_shell)
{
        if (!strcmp(new_shell,"bash"))
                shell=BASH;
        else if (!strcmp(new_shell,"tcsh"))
                shell=TCSH;
        else if (!strcmp(new_shell,"sh"))
                shell=BASH;
        else if (!strcmp(new_shell,"csh"))
                shell=TCSH;
        else
                bb_error_msg("unknown shell after -s or --shell argument");
}


/* Exit codes:
 *   0) No errors, succesful operation.
 *   1) getopt(3) returned an error.
 *   2) A problem with parameter parsing for getopt(1).
 *   3) Internal error, out of memory
 *   4) Returned for -T
 */

static struct option longopts[]=
{
        {"options",required_argument,NULL,'o'},
        {"longoptions",required_argument,NULL,'l'},
        {"quiet",no_argument,NULL,'q'},
        {"quiet-output",no_argument,NULL,'Q'},
        {"shell",required_argument,NULL,'s'},
        {"test",no_argument,NULL,'T'},
        {"unquoted",no_argument,NULL,'u'},
        {"alternative",no_argument,NULL,'a'},
        {"name",required_argument,NULL,'n'},
        {NULL,0,NULL,0}
};

/* Stop scanning as soon as a non-option argument is found! */
static const char *shortopts="+ao:l:n:qQs:Tu";


int getopt_main(int argc, char *argv[])
{
        char *optstr=NULL;
        char *name=NULL;
        int opt;
        int compatible=0;

        init_longopt();

        if (getenv("GETOPT_COMPATIBLE"))
                compatible=1;

        if (argc == 1) {
                if (compatible) {
                        /* For some reason, the original getopt gave no error
                           when there were no arguments. */
                        printf(" --\n");
                       return 0;
                } else
                        bb_error_msg_and_die("missing optstring argument");
        }

        if (argv[1][0] != '-' || compatible) {
                quote=0;
                optstr=xmalloc(strlen(argv[1])+1);
                strcpy(optstr,argv[1]+strspn(argv[1],"-+"));
                argv[1]=argv[0];
               return (generate_output(argv+1,argc-1,optstr,long_options));
        }

        while ((opt=getopt_long(argc,argv,shortopts,longopts,NULL)) != EOF)
                switch (opt) {
                case 'a':
                        alternative=1;
                        break;
                case 'o':
                       free(optstr);
                       optstr=bb_xstrdup(optarg);
                        break;
                case 'l':
                        add_long_options(optarg);
                        break;
                case 'n':
                       free(name);
                       name=bb_xstrdup(optarg);
                        break;
                case 'q':
                        quiet_errors=1;
                        break;
                case 'Q':
                        quiet_output=1;
                        break;
                case 's':
                        set_shell(optarg);
                        break;
                case 'T':
                       return 4;
                case 'u':
                        quote=0;
                        break;
                default:
                        bb_show_usage();
                }

        if (!optstr) {
                if (optind >= argc)
                        bb_error_msg_and_die("missing optstring argument");
                else {
                       optstr=bb_xstrdup(argv[optind]);
                        optind++;
                }
        }
        if (name)
                argv[optind-1]=name;
        else
                argv[optind-1]=argv[0];
       return (generate_output(argv+optind-1,argc-optind+1,optstr,long_options));
}

/*
  Local Variables:
  c-file-style: "linux"
  c-basic-offset: 4
  tab-width: 4
  End:
*/
