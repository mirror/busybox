#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "leases.h"
#include "busybox.h"

#define REMAINING 0
#define ABSOLUTE 1

int dumpleases_main(int argc, char *argv[])
{
	FILE *fp;
	int i, c, mode = REMAINING;
	long expires;
	const char *file = leases_file;
	struct dhcpOfferedAddr lease;
	struct in_addr addr;
	
	static const struct option options[] = {
		{"absolute", 0, 0, 'a'},
		{"remaining", 0, 0, 'r'},
		{"file", 1, 0, 'f'},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "arf:", options, &option_index);
		if (c == -1) break;
		
		switch (c) {
		case 'a': mode = ABSOLUTE; break;
		case 'r': mode = REMAINING; break;
		case 'f':
			file =  optarg;
			break;
		default:
			bb_show_usage();
		}
	}
			
	fp = bb_xfopen(file, "r");

	printf("Mac Address       IP-Address      Expires %s\n", mode == REMAINING ? "in" : "at");  
	/*     "00:00:00:00:00:00 255.255.255.255 Wed Jun 30 21:49:08 1993" */
	while (fread(&lease, sizeof(lease), 1, fp)) {

		for (i = 0; i < 6; i++) {
			printf("%02x", lease.chaddr[i]);
			if (i != 5) printf(":");
		}
		addr.s_addr = lease.yiaddr;
		printf(" %-15s", inet_ntoa(addr));
		expires = ntohl(lease.expires);
		printf(" ");
		if (mode == REMAINING) {
			if (!expires) printf("expired\n");
			else {
				if (expires > 60*60*24) {
					printf("%ld days, ", expires / (60*60*24));
					expires %= 60*60*24;
				}
				if (expires > 60*60) {
					printf("%ld hours, ", expires / (60*60));
					expires %= 60*60;
				}
				if (expires > 60) {
					printf("%ld minutes, ", expires / 60);
					expires %= 60;
				}
				printf("%ld seconds\n", expires);
			}
		} else printf("%s", ctime(&expires));
	}
	fclose(fp);
	
	return 0;
}
