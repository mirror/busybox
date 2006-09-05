/* vi: set ts=4 :
 * 
 * bbsh - busybox shell
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

// Handle embedded NUL bytes in the command line.

#include <busybox.h>

static int handle(char *command)
{
	int argc=0;
	char *argv[10], *start = command;

	// Parse command into argv[]
	for (;;) {
		char *end;

		// Skip leading whitespace and detect EOL.
		while(isspace(*start)) start++;
		if(!*start || *start=='#') break;

		// Grab next word.  (Add dequote and envvar logic here)
		end=start;
		while(*end && !isspace(*end)) end++;
		argv[argc++]=xstrndup(start,end-start);
		start=end;
	}
	argv[argc]=0;

	if (!argc) return 0;
	if (argc==2 && !strcmp(argv[0],"cd")) chdir(argv[1]);
	else if(!strcmp(argv[0],"exit")) exit(argc>1 ? atoi(argv[1]) : 0); 
	else {
		int status;
		pid_t pid=fork();
		if(!pid) {
			run_applet_by_name(argv[0],argc,argv);
			execvp(argv[0],argv);
			printf("No %s",argv[0]);
			exit(1);
		} else waitpid(pid, &status, 0);
	}
	while(argc) free(argv[--argc]);

	return 0;
}

int bbsh_main(int argc, char *argv[])
{
	char *command=NULL;
	FILE *f;

	bb_getopt_ulflags(argc, argv, "c:", &command);

	f = argv[optind] ? xfopen(argv[optind],"r") : NULL;
	if (command) handle(command);
	else {
		unsigned cmdlen=0;
		for (;;) {
			if(!f) putchar('$');
			if(1 > getline(&command,&cmdlen,f ? : stdin)) break;
			handle(command);
		}
		if (ENABLE_FEATURE_CLEAN_UP) free(command);
	}
		
	return 1;
}
