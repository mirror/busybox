/* vi: set sw=4 ts=4: */
/*
 * Applet table generator.
 * Runs on host and produces include/applet_tables.h
 *
 * Copyright (C) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file License in this tarball for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/autoconf.h"
#include "../include/busybox.h"

struct bb_applet {
	const char *name;
	const char *main;
	enum bb_install_loc_t install_loc;
	enum bb_suid_t need_suid;
	/* true if instead of fork(); exec("applet"); waitpid();
	 * one can do fork(); exit(applet_main(argc,argv)); waitpid(); */
	unsigned char noexec;
	/* Even nicer */
	/* true if instead of fork(); exec("applet"); waitpid();
	 * one can simply call applet_main(argc,argv); */
	unsigned char nofork;
};

/* Define struct bb_applet applets[] */
#include "../include/applets.h"

enum { NUM_APPLETS = sizeof(applets)/sizeof(applets[0]) };

static int offset[NUM_APPLETS];

static int cmp_name(const void *a, const void *b)
{
	const struct bb_applet *aa = a;
	const struct bb_applet *bb = b;
	return strcmp(aa->name, bb->name);
}

int main(int argc, char **argv)
{
	int i;
	int ofs;

	qsort(applets, NUM_APPLETS, sizeof(applets[0]), cmp_name);

	/* Keep in sync with include/busybox.h! */

	puts("const char applet_names[] ALIGN1 =");
	ofs = 0;
	i = 0;
	for (i = 0; i < NUM_APPLETS; i++) {
		offset[i] = ofs;
		ofs += strlen(applets[i].name) + 1;
		printf("\"%s\" \"\\0\"\n", applets[i].name);
	}
	puts(";");

	puts("int (*const applet_mains[])(int argc, char **argv) = {");
	for (i = 0; i < NUM_APPLETS; i++) {
		printf("%s_main,\n", applets[i].main);
	}
	puts("};");

#if ENABLE_FEATURE_INSTALLER || ENABLE_FEATURE_PREFER_APPLETS
	puts("const uint32_t applet_nameofs[] = {");
#else
	puts("const uint16_t applet_nameofs[] ALIGN2 = {");
#endif
	for (i = 0; i < NUM_APPLETS; i++) {
		printf("0x%08x,\n",
			offset[i]
#if ENABLE_FEATURE_SUID
			+ (applets[i].need_suid   << 14) /* 2 bits */
#endif
#if ENABLE_FEATURE_INSTALLER
			+ (applets[i].install_loc << 16) /* 3 bits */
#endif
#if ENABLE_FEATURE_PREFER_APPLETS
			+ (applets[i].nofork << 19)
			+ (applets[i].noexec << 20)
#endif
		);
	}
	puts("};");

	return 0;
}
