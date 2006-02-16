#include "busybox.h"

#ifndef BB_EXTRA_VERSION
#define BANNER "BusyBox v" BB_VER " (" BB_BT ")"
#else
#define BANNER "BusyBox v" BB_VER " (" BB_EXTRA_VERSION ")"
#endif

const char BB_BANNER[]=BANNER;
const char * const bb_msg_full_version = BANNER " multi-call binary";
