#ifndef __BUSYBOX_FUNCTIONS_H__
#define __BUSYBOX_FUNCTIONS_H__

int
mkswap(char *device_name, int pages, int check);
/* pages = 0 for autodetection */

int
fdflush(char *filename);

#endif /* __BUSYBOX_FUNCTIONS_H__ */
