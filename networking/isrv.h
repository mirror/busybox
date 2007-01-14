/* vi: set sw=4 ts=4: */
/*
 * Generic non-forking server infrastructure.
 * Intended to make writing telnetd-type servers easier.
 *
 * Copyright (C) 2007 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

/* opaque structure */
struct isrv_state_t;
typedef struct isrv_state_t isrv_state_t;

/* callbacks */
void isrv_want_rd(isrv_state_t *state, int fd);
void isrv_want_wr(isrv_state_t *state, int fd);
void isrv_dont_want_rd(isrv_state_t *state, int fd);
void isrv_dont_want_wr(isrv_state_t *state, int fd);
int isrv_register_fd(isrv_state_t *state, int peer, int fd);
void isrv_close_fd(isrv_state_t *state, int fd);
int isrv_register_peer(isrv_state_t *state, void *param);

/* driver */
void isrv_run(
	int listen_fd,
	int (*new_peer)(isrv_state_t *state, int fd),
	int (*do_rd)(int fd, void **),
	int (*do_wr)(int fd, void **),
	int (*do_timeout)(void **),
	int timeout,
	int linger_timeout
);
