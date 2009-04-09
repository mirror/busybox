/* vi: set sw=4 ts=4: */
#ifndef LL_MAP_H
#define LL_MAP_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

int ll_remember_index(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
int ll_init_map(struct rtnl_handle *rth);
int xll_name_to_index(const char *const name);
const char *ll_index_to_name(int idx);
const char *ll_idx_n2a(int idx, char *buf);
/* int ll_index_to_type(int idx); */
unsigned ll_index_to_flags(int idx);

POP_SAVED_FUNCTION_VISIBILITY

#endif
