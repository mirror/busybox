/*
 * $Id: hostname.c,v 1.1 1999/12/07 23:14:59 andersen Exp $
 * Mini hostname implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
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
 */

#include "internal.h"
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

static const char* hostname_usage = "hostname [OPTION] {hostname | -F file}\n\n"
"Options:\n"
"\t-s, --short\t\tshort\n"
"\t-i, --ip-address\t\taddresses for the hostname\n"
"\t-d, --domain\t\tDNS domain name\n"
"If a hostname is given, or a file is given with the -F parameter, the host\n"
"name will be set\n";

static char short_opts[] = "sidF:";
static const struct option long_opts[] = {
    { "short", no_argument, NULL, 's' },
    { "ip-address", no_argument, NULL, 'i' },
    { "domain", no_argument, NULL, 'd' },
    { NULL, 0, NULL, 0 }
};

void do_sethostname(char *s, int isfile)
{
    FILE *f;
    char buf[255];
    
    if (!s) return;
    if (!isfile) {
        if (sethostname(s, strlen(s)) < 0) {
	    if (errno == EPERM) 
	        fprintf(stderr, "hostname: you must be root to change the hostname\n");
	    else	  	
	        perror("sethostname");
	    exit(1);
	}	
    } else {
        if ((f = fopen(s, "r")) == NULL) {
	    perror(s);
	    exit(1);
	} else {
            fgets(buf, 255, f);
	    fclose(f);
	    if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
	    if (sethostname(buf, strlen(buf)) < 0) {
	       perror("sethostname");
	       exit(1);
	    }
	}
    }	
}

int hostname_main(int argc, char **argv)
{
    int c;
    int opt_short = 0;
    int opt_domain = 0;
    int opt_ip = 0;
    int opt_file = 0;
    struct hostent *h;
    char *filename = NULL;
    char buf[255];
    char *s = NULL;
    
    if (argc < 1) usage(hostname_usage);

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (c) {
          case 's': opt_short = 1; break;
	  case 'i': opt_ip = 1; break;
	  case 'd': opt_domain = 1; break;
          case 'F': opt_file = 1; filename = optarg; break;
	  default: usage(hostname_usage);		    
        }
    }

    if (optind < argc) {
	do_sethostname(argv[optind], 0);
    } else if (opt_file) {
        do_sethostname(filename, 1);
    } else {
	gethostname(buf, 255);
        if (opt_short) {
            s = strchr(buf, '.');
	    if (!s) s = buf; *s = 0;
	    printf("%s\n", buf);
	} else if (opt_domain) {
	    s = strchr(buf, '.');
	    printf("%s\n", (s ? s+1 : ""));
        } else if (opt_ip) {
	    h = gethostbyname(buf);
	    if (!h) {
	        printf("Host not found\n");
		exit(1);
	    }
	    printf("%s\n", inet_ntoa(*(struct in_addr *)(h->h_addr)));
        } else {
	  printf("%s\n", buf);
        }
    }
    return 0;
}
	  
