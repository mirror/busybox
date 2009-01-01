/* vi: set sw=4 ts=4: */
/*
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "common.h"
#include "dhcpd.h"

int dumpleases_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dumpleases_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	int i;
	unsigned opt;
	leasetime_t expires;
	leasetime_t curr;
	const char *file = LEASES_FILE;
	struct dhcpOfferedAddr lease;
	struct in_addr addr;

	enum {
		OPT_a	= 0x1,	// -a
		OPT_r	= 0x2,	// -r
		OPT_f	= 0x4,	// -f
	};
#if ENABLE_GETOPT_LONG
	static const char dumpleases_longopts[] ALIGN1 =
		"absolute\0"  No_argument       "a"
		"remaining\0" No_argument       "r"
		"file\0"      Required_argument "f"
		;

	applet_long_options = dumpleases_longopts;
#endif
	opt_complementary = "=0:a--r:r--a";
	opt = getopt32(argv, "arf:", &file);

	fd = xopen(file, O_RDONLY);

	printf("Mac Address       IP-Address      Expires %s\n", (opt & OPT_a) ? "at" : "in");
	/*     "00:00:00:00:00:00 255.255.255.255 Wed Jun 30 21:49:08 1993" */

	curr = time(NULL);
	while (full_read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
		const char *fmt = ":%02x" + 1;
		for (i = 0; i < 6; i++) {
			printf(fmt, lease.chaddr[i]);
			fmt = ":%02x";
		}
		addr.s_addr = lease.yiaddr;
		printf(" %-15s ", inet_ntoa(addr));
		if (lease.expires == 0) {
			puts("expired");
			continue;
		}
		expires = ntohl(lease.expires);
		if (!(opt & OPT_a)) { /* no -a */
			unsigned d, h, m;
			d = expires / (24*60*60); expires %= (24*60*60);
			h = expires / (60*60); expires %= (60*60);
			m = expires / 60; expires %= 60;
			if (d)
				printf("%u days ", d);
			printf("%02u:%02u:%02u\n", h, m, (unsigned)expires);
		} else { /* -a */
			time_t t = expires + curr;
			fputs(ctime(&t), stdout);
		}
	}
	/* close(fd); */

	return 0;
}
