#include "busybox.h"
#include "bbconfigopts.h"

int bbconfig_main(int argc, char **argv)
{
	printf(bbconfig_config);
	return 0;
}
