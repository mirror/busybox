/*
 * arpping .h
 */

#ifndef ARPPING_H
#define ARPPING_H

#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>

struct arpMsg {
	struct ethhdr ethhdr;	 		/* Ethernet header */
	uint16_t htype;				/* hardware type (must be ARPHRD_ETHER) */
	uint16_t ptype;				/* protocol type (must be ETH_P_IP) */
	uint8_t  hlen;				/* hardware address length (must be 6) */
	uint8_t  plen;				/* protocol address length (must be 4) */
	uint16_t operation;			/* ARP opcode */
	uint8_t  sHaddr[6];			/* sender's hardware address */
	uint8_t  sInaddr[4];			/* sender's IP address */
	uint8_t  tHaddr[6];			/* target's hardware address */
	uint8_t  tInaddr[4];			/* target's IP address */
	uint8_t  pad[18];			/* pad for min. Ethernet payload (60 bytes) */
};

/* function prototypes */
int arpping(uint32_t yiaddr, uint32_t ip, uint8_t *arp, char *interface);

#endif
