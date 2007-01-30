/* vi: set sw=4 ts=4: */
#ifndef RT_NAMES_H_
#define RT_NAMES_H_ 1

#include <stdint.h>

extern const char* rtnl_rtprot_n2a(int id, char *buf, int len);
extern const char* rtnl_rtscope_n2a(int id, char *buf, int len);
extern const char* rtnl_rtrealm_n2a(int id, char *buf, int len);
extern const char* rtnl_dsfield_n2a(int id, char *buf, int len);
extern const char* rtnl_rttable_n2a(int id, char *buf, int len);
extern int rtnl_rtprot_a2n(uint32_t *id, char *arg);
extern int rtnl_rtscope_a2n(uint32_t *id, char *arg);
extern int rtnl_rtrealm_a2n(uint32_t *id, char *arg);
extern int rtnl_dsfield_a2n(uint32_t *id, char *arg);
extern int rtnl_rttable_a2n(uint32_t *id, char *arg);


extern const char* ll_type_n2a(int type, char *buf, int len);

extern const char* ll_addr_n2a(unsigned char *addr, int alen, int type,
				char *buf, int blen);
extern int ll_addr_a2n(unsigned char *lladdr, int len, char *arg);

extern const char* ll_proto_n2a(unsigned short id, char *buf, int len);
extern int ll_proto_a2n(unsigned short *id, char *buf);

#endif
