/* vi: set sw=4 ts=4: */
/*
 * really dumb modprobe implementation for busybox
 * Copyright (C) 2001 Lineo, davidm@lineo.com
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include "busybox.h"

static char cmd[128];

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
	while (optind < argc) {
		strcat(cmd, argv[optind]);
		strcat(cmd, " ");
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


