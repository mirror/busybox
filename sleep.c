#include "internal.h"
#include <stdio.h>

const char	sleep_usage[] = " NUMBER\n"
"Pause for NUMBER seconds.\n";

extern int
sleep_main(int argc, char * * argv)
{
        if ( (argc < 2) || (**(argv+1) == '-') ) {
	    fprintf(stderr, "Usage: %s %s", *argv, sleep_usage);
	    exit(FALSE);
	}

	if ( sleep(atoi(*(++argv))) != 0 ) {
		perror( "sleep");
		exit (FALSE);
	} else
		exit (TRUE);
}
