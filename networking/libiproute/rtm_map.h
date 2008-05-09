/* vi: set sw=4 ts=4: */
#ifndef __RTM_MAP_H__
#define __RTM_MAP_H__ 1

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

const char *rtnl_rtntype_n2a(int id, char *buf, int len);
int rtnl_rtntype_a2n(int *id, char *arg);

int get_rt_realms(uint32_t *realms, char *arg);

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif /* __RTM_MAP_H__ */
