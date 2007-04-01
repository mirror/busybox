/* Based on ipsvd utilities written by Gerrit Pape <pape@smarden.org>
 * which are released into public domain by the author.
 * Homepage: http://smarden.sunsite.dk/ipsvd/
 *
 * Copyright (C) 2007 Denis Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

struct hcc {
        char ip[32 - sizeof(int)];
        int pid;
};

void ipsvd_perhost_init(unsigned);
unsigned ipsvd_perhost_add(const char *ip, unsigned maxconn, struct hcc **hccpp);
void ipsvd_perhost_remove(int pid);
//unsigned ipsvd_perhost_setpid(int pid);
//void ipsvd_perhost_free(void);
