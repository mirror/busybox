/* minix xargs - Make and execute commands	     
 * Author: Ian Nicholls:  1 Mar 90 */

/*
 * xargs  - Accept words from stdin until, combined with the arguments
 *	    given on the command line, just fit into the command line limit.
 *	    Then, execute the result.
 * 		e.g.    ls | xargs compress
 *			find . -name '*.s' -print | xargs ar qv libc.a
 *
 * flags: -t		Print the command just before it is run
 *	  -l len	Use len as maximum line length (default 490, max 1023)
 *	  -e ending	Append ending to the command before executing it.
 *
 * Exits with:	0  No errors.
 *		1  If any system(3) call returns a nonzero status.
 *		2  Usage error
 *		3  Line length too short to contain some single argument.
 *
 * Examples:	xargs ar qv libc.a < liborder		# Create a new libc.a
 *		find . -name '*.s' -print | xargs rm	# Remove all .s files
 *		find . -type f ! -name '*.Z' \		# Compress old files.
 *		       -atime +60 -print  | xargs compress -v
 *
 * Bugs:  If the command contains unquoted wildflags, then the system(3) call
 *		call may expand this to larger than the maximum line size.
 *	  The command is not executed if nothing was read from stdin.
 *	  xargs may give up too easily when the command returns nonzero.
 */
#define USAGE "usage: xargs [-t] [-l len] [-e endargs] command [args...]\n"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#ifndef MAX_ARGLINE
# define MAX_ARGLINE 1023
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

char outlin[MAX_ARGLINE];
char inlin[MAX_ARGLINE];
char startlin[MAX_ARGLINE];
char *ending = NULL;
char traceflag = 0;

int xargs_main(int ac, char **av)
{
   int outlen, inlen, startlen, endlen=0, i;
   char errflg = 0;
   int maxlin = MAX_ARGLINE;

   while ((i = getopt(ac, av, "tl:e:")) != EOF)
       switch (i) {
	   case 't': traceflag = 1;	  break;
	   case 'l': maxlin = min(MAX_ARGLINE, atoi(optarg)); break;
	   case 'e': ending = optarg;	  break;
	   case '?': errflg++;		  break;
       }
   if (errflg)	{
       fprintf(stderr, USAGE);
       exit(2);
   }

   startlin[0] = 0;
   if (optind == ac) {
       strcat(startlin, "echo ");
   }
   else for ( ; optind < ac; optind++) {
       strcat(startlin, av[optind]);
       strcat(startlin, " ");
   }
   startlen = strlen(startlin);
   if (ending) endlen = strlen(ending);
   maxlin = maxlin - 1 - endlen;	/* Pre-compute */

   strcpy(outlin, startlin);
   outlen = startlen;

   while (gets(inlin) != NULL) {
       inlen = strlen(inlin);
       if (maxlin <= (outlen + inlen)) {
	   if (outlen == startlen) {
	       fprintf(stderr, "%s: Line length too short to process '%s'\n",
		       av[0], inlin);
	       exit(3);
	   }
	   if (ending) strcat(outlin, ending);
	   if (traceflag) fputs(outlin,stderr);
	   errno = 0;
	   if (0 != system(outlin)) {
	       if (errno != 0) perror("xargs");
	       exit(1);
	   }
	   strcpy(outlin, startlin);
	   outlen = startlen;
       }
       strcat(outlin, inlin);
       strcat(outlin, " ");
       outlen = outlen + inlen + 1;
   }
   if (outlen != startlen) {
       if (ending) strcat(outlin, ending);
       if (traceflag) fputs(outlin,stderr);
       errno = 0;
       if (0 != system(outlin)) {
	   if (errno != 0) perror("xargs");
	   exit(1);
       }
   }    
   return 0;
}
