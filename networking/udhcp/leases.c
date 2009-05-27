/* vi: set sw=4 ts=4: */
/*
 * leases.c -- tools to manage DHCP leases
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "common.h"
#include "dhcpd.h"


/* Find the oldest expired lease, NULL if there are no expired leases */
static struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest_lease = NULL;
	leasetime_t oldest_time = time(NULL);
	unsigned i;

	/* Unexpired leases have leases[i].expires >= current time
	 * and therefore can't ever match */
	for (i = 0; i < server_config.max_leases; i++) {
		if (leases[i].expires < oldest_time) {
			oldest_time = leases[i].expires;
			oldest_lease = &(leases[i]);
		}
	}
	return oldest_lease;
}


/* Clear every lease out that chaddr OR yiaddr matches and is nonzero */
static void clear_lease(const uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned i, j;

	for (j = 0; j < 16 && !chaddr[j]; j++)
		continue;

	for (i = 0; i < server_config.max_leases; i++) {
		if ((j != 16 && memcmp(leases[i].chaddr, chaddr, 16) == 0)
		 || (yiaddr && leases[i].yiaddr == yiaddr)
		) {
			memset(&(leases[i]), 0, sizeof(leases[i]));
		}
	}
}


/* Add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr* FAST_FUNC add_lease(
		const uint8_t *chaddr, uint32_t yiaddr,
		leasetime_t leasetime, uint8_t *hostname)
{
	struct dhcpOfferedAddr *oldest;
	uint8_t hostname_length;

	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);

	oldest = oldest_expired_lease();

	if (oldest) {
		oldest->hostname[0] = '\0';
		if (hostname) {
			/* option size byte, + 1 for NUL */
        		hostname_length = hostname[-1] + 1;
			if (hostname_length > sizeof(oldest->hostname))
				hostname_length = sizeof(oldest->hostname);
            		hostname = (uint8_t*) safe_strncpy((char*)oldest->hostname, (char*)hostname, hostname_length);
			/* sanitization (s/non-ASCII/^/g) */
			while (*hostname) {
				if (*hostname < ' ' || *hostname > 126)
					*hostname = '^';
				hostname++;
			}
		}
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		oldest->expires = time(NULL) + leasetime;
	}

	return oldest;
}


/* True if a lease has expired */
int FAST_FUNC lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (leasetime_t) time(NULL));
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr* FAST_FUNC find_lease_by_chaddr(const uint8_t *chaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (!memcmp(leases[i].chaddr, chaddr, 16))
			return &(leases[i]);

	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr* FAST_FUNC find_lease_by_yiaddr(uint32_t yiaddr)
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
	add_lease(blank_chaddr, addr, server_config.conflict_time, NULL);
	return 0;
}


/* Find a new usable (we think) address. */
uint32_t FAST_FUNC find_free_or_expired_address(void)
{
	uint32_t addr;
	struct dhcpOfferedAddr *oldest_lease = NULL;

	addr = server_config.start_ip; /* addr is in host order here */
	for (; addr <= server_config.end_ip; addr++) {
		uint32_t net_addr;
		struct dhcpOfferedAddr *lease;

		/* ie, 192.168.55.0 */
		if ((addr & 0xff) == 0)
			continue;
		/* ie, 192.168.55.255 */
		if ((addr & 0xff) == 0xff)
			continue;
		net_addr = htonl(addr);
		/* addr has a static lease? */
		if (reservedIp(server_config.static_leases, net_addr))
			continue;

		lease = find_lease_by_yiaddr(net_addr);
		if (!lease) {
			if (nobody_responds_to_arp(net_addr))
				return net_addr;
		} else {
			if (!oldest_lease || lease->expires < oldest_lease->expires)
				oldest_lease = lease;
		}
	}

	if (oldest_lease && lease_expired(oldest_lease)
	 && nobody_responds_to_arp(oldest_lease->yiaddr)
	) {
		return oldest_lease->yiaddr;
	}

	return 0;
}
