/* vi: set sw=4 ts=4: */
#include <unistd.h>

#include "../include/autoconf.h"
#include "../include/busybox.h"

static const char usage_messages[] = ""
#define MAKE_USAGE
#include "usage.h"
#include "applets.h"
;

int main(void)
{
	write(1, usage_messages, sizeof(usage_messages));
	return 0;
}
