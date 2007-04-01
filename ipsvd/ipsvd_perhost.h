struct hcc {
        char ip[32 - sizeof(int)];
        int pid;
};

void ipsvd_perhost_init(unsigned);
unsigned ipsvd_perhost_add(const char *ip, unsigned maxconn, struct hcc **hccpp);
void ipsvd_perhost_remove(int pid);
//unsigned ipsvd_perhost_setpid(int pid);
//void ipsvd_perhost_free(void);
