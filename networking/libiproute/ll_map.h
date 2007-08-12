/* vi: set sw=4 ts=4: */
#ifndef __LL_MAP_H__
#define __LL_MAP_H__ 1

int ll_remember_index(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
int ll_init_map(struct rtnl_handle *rth);
int xll_name_to_index(const char *const name);
const char *ll_index_to_name(int idx);
const char *ll_idx_n2a(int idx, char *buf);
/* int ll_index_to_type(int idx); */
unsigned ll_index_to_flags(int idx);

#endif /* __LL_MAP_H__ */
