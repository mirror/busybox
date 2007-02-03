/* vi: set sw=4 ts=4: */
/* This file was released into the public domain by Paul Fox.
 */
#include "busybox.h"
#include "bbconfigopts.h"

int bbconfig_main(int argc, char **argv);
int bbconfig_main(int argc, char **argv)
{
	printf(bbconfig_config);
	return 0;
}
