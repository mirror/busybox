/* vi: set sw=4 ts=4: */
/*
 * Helper functions shared by init et al.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
extern int kill_init(int sig);
extern int bb_shutdown_system(unsigned long magic);
extern const char * const init_sending_format;

