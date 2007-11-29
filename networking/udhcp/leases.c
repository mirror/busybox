/* vi: set sw=4 ts=4: */
/*
 * leases.c -- tools to manage DHCP leases
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include "common.h"
#include "dhcpd.h"


/* Find the oldest expired lease, NULL if there are no expired leases */
static struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
// TODO: use monotonic_sec()
	unsigned long oldest_lease = time(0);
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (oldest_lease > leases[i].expires) {
			oldest_lease = leases[i].expires;
			oldest = &(leases[i]);
		}
	return oldest;
}


/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
static void clear_lease(const uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned i, j;

	for (j = 0; j < 16 && !chaddr[j]; j++)
		continue;

	for (i = 0; i < server_config.max_leases; i++)
		if ((j != 16 && memcmp(leases[i].chaddr, chaddr, 16) == 0)
		 || (yiaddr && leases[i].yiaddr == yiaddr)
		) {
			memset(&(leases[i]), 0, sizeof(leases[i]));
		}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(const uint8_t *chaddr, uint32_t yiaddr, unsigned long lease)
{
	struct dhcpOfferedAddr *oldest;

	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);

	oldest = oldest_expired_lease();

	if (oldest) {
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		oldest->expires = time(0) + lease;
	}

	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) time(0));
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(const uint8_t *chaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (!memcmp(leases[i].chaddr, chaddr, 16))
			return &(leases[i]);

	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(uint32_t yiaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (leases[i].yiaddr == yiaddr)
			return &(leases[i]);

	return NULL;
}


/* check is an IP is taken, if it is, add it to the lease table */
static int nobody_responds_to_arp(uint32_t addr)
{
	/* 16 zero bytes */
	static const uint8_t blank_chaddr[16] = { 0 };
	/* = { 0 } helps gcc to put it in rodata, not bss */

	struct in_addr temp;
	int r;

	r = arpping(addr, server_config.server, server_config.arp, server_config.interface);
	if (r)
		return r;

	temp.s_addr = addr;
	bb_info_msg("%s belongs to someone, reserving it for %u seconds",
		inet_ntoa(temp), (unsigned)server_config.conflict_time);
	add_lease(blank_chaddr, addr, server_config.conflict_time);
	return 0;
}


/* find an assignable address, if check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
uint32_t find_address(int check_expired)
{
	uint32_t addr, ret;
	struct dhcpOfferedAddr *lease = NULL;

	addr = server_config.start_ip; /* addr is in host order here */
	for (; addr <= server_config.end_ip; addr++) {
		/* ie, 192.168.55.0 */
		if (!(addr & 0xFF))
			continue;
		/* ie, 192.168.55.255 */
		if ((addr & 0xFF) == 0xFF)
			continue;
		/* Only do if it isn't assigned as a static lease */
		ret = htonl(addr);
		if (!reservedIp(server_config.static_leases, ret)) {
			/* lease is not taken */
			lease = find_lease_by_yiaddr(ret);
			/* no lease or it expired and we are checking for expired leases */
			if ((!lease || (check_expired && lease_expired(lease)))
			 && nobody_responds_to_arp(ret) /* it isn't used on the network */
			) {
				return ret;
			}
		}
	}
	return 0;
}
