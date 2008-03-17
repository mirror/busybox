/* vi: set sw=4 ts=4: */
/* This file was released into the public domain by Paul Fox.
 */
#include "libbb.h"
#include "bbconfigopts.h"

int bbconfig_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bbconfig_main(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	printf(bbconfig_config);
	return 0;
}
