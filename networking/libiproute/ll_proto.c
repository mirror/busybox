/*
 * ll_proto.c
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include "utils.h"

/* Stuff swiped from linux/if_arp.h */
#define ETH_P_LOOP	0x0060		/* Ethernet Loopback packet	*/
#define ETH_P_PUP	0x0200		/* Xerox PUP packet		*/
#define ETH_P_PUPAT	0x0201		/* Xerox PUP Addr Trans packet	*/
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_X25	0x0805		/* CCITT X.25			*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/
#define	ETH_P_BPQ	0x08FF		/* G8BPQ AX.25 Ethernet Packet	[ NOT AN OFFICIALLY REGISTERED ID ] */
#define ETH_P_IEEEPUP	0x0a00		/* Xerox IEEE802.3 PUP packet */
#define ETH_P_IEEEPUPAT	0x0a01		/* Xerox IEEE802.3 PUP Addr Trans packet */
#define ETH_P_DEC       0x6000          /* DEC Assigned proto           */
#define ETH_P_DNA_DL    0x6001          /* DEC DNA Dump/Load            */
#define ETH_P_DNA_RC    0x6002          /* DEC DNA Remote Console       */
#define ETH_P_DNA_RT    0x6003          /* DEC DNA Routing              */
#define ETH_P_LAT       0x6004          /* DEC LAT                      */
#define ETH_P_DIAG      0x6005          /* DEC Diagnostics              */
#define ETH_P_CUST      0x6006          /* DEC Customer use             */
#define ETH_P_SCA       0x6007          /* DEC Systems Comms Arch       */
#define ETH_P_RARP      0x8035		/* Reverse Addr Res packet	*/
#define ETH_P_ATALK	0x809B		/* Appletalk DDP		*/
#define ETH_P_AARP	0x80F3		/* Appletalk AARP		*/
#define ETH_P_8021Q	0x8100          /* 802.1Q VLAN Extended Header  */
#define ETH_P_IPX	0x8137		/* IPX over DIX			*/
#define ETH_P_IPV6	0x86DD		/* IPv6 over bluebook		*/
#define ETH_P_PPP_DISC	0x8863		/* PPPoE discovery messages     */
#define ETH_P_PPP_SES	0x8864		/* PPPoE session messages	*/
#define ETH_P_ATMMPOA	0x884c		/* MultiProtocol Over ATM	*/
#define ETH_P_ATMFATE	0x8884		/* Frame-based ATM Transport over Ethernet */
#define ETH_P_EDP2	0x88A2		/* Coraid EDP2			*/
#define ETH_P_802_3	0x0001		/* Dummy type for 802.3 frames  */
#define ETH_P_AX25	0x0002		/* Dummy protocol id for AX.25  */
#define ETH_P_ALL	0x0003		/* Every packet (be careful!!!) */
#define ETH_P_802_2	0x0004		/* 802.2 frames 		*/
#define ETH_P_SNAP	0x0005		/* Internal only		*/
#define ETH_P_DDCMP     0x0006          /* DEC DDCMP: Internal only     */
#define ETH_P_WAN_PPP   0x0007          /* Dummy type for WAN PPP frames*/
#define ETH_P_PPP_MP    0x0008          /* Dummy type for PPP MP frames */
#define ETH_P_LOCALTALK 0x0009		/* Localtalk pseudo type 	*/
#define ETH_P_PPPTALK	0x0010		/* Dummy type for Atalk over PPP*/
#define ETH_P_TR_802_2	0x0011		/* 802.2 frames 		*/
#define ETH_P_MOBITEX	0x0015		/* Mobitex (kaz@cafe.net)	*/
#define ETH_P_CONTROL	0x0016		/* Card specific control frames */
#define ETH_P_IRDA	0x0017		/* Linux-IrDA			*/
#define ETH_P_ECONET	0x0018		/* Acorn Econet			*/
#define ETH_P_HDLC	0x0019		/* HDLC frames			*/


#define __PF(f,n) { ETH_P_##f, #n },
static struct {
	int id;
	char *name;
} llproto_names[] = {
__PF(LOOP,loop)
__PF(PUP,pup)  
#ifdef ETH_P_PUPAT
__PF(PUPAT,pupat)     
#endif
__PF(IP,ip)
__PF(X25,x25)
__PF(ARP,arp)
__PF(BPQ,bpq)
#ifdef ETH_P_IEEEPUP
__PF(IEEEPUP,ieeepup)  
#endif
#ifdef ETH_P_IEEEPUPAT
__PF(IEEEPUPAT,ieeepupat)  
#endif
__PF(DEC,dec)       
__PF(DNA_DL,dna_dl)    
__PF(DNA_RC,dna_rc)    
__PF(DNA_RT,dna_rt)    
__PF(LAT,lat)       
__PF(DIAG,diag)      
__PF(CUST,cust)      
__PF(SCA,sca)       
__PF(RARP,rarp)      
__PF(ATALK,atalk)     
__PF(AARP,aarp)      
__PF(IPX,ipx)       
__PF(IPV6,ipv6)      
#ifdef ETH_P_PPP_DISC
__PF(PPP_DISC,ppp_disc)      
#endif
#ifdef ETH_P_PPP_SES
__PF(PPP_SES,ppp_ses)      
#endif
#ifdef ETH_P_ATMMPOA
__PF(ATMMPOA,atmmpoa)      
#endif
#ifdef ETH_P_ATMFATE
__PF(ATMFATE,atmfate)      
#endif

__PF(802_3,802_3)     
__PF(AX25,ax25)      
__PF(ALL,all)       
__PF(802_2,802_2)     
__PF(SNAP,snap)      
__PF(DDCMP,ddcmp)     
__PF(WAN_PPP,wan_ppp)   
__PF(PPP_MP,ppp_mp)    
__PF(LOCALTALK,localtalk) 
__PF(PPPTALK,ppptalk)   
__PF(TR_802_2,tr_802_2)  
__PF(MOBITEX,mobitex)   
__PF(CONTROL,control)   
__PF(IRDA,irda)      
#ifdef ETH_P_ECONET
__PF(ECONET,econet)      
#endif

{ 0x8100, "802.1Q" },
{ ETH_P_IP, "ipv4" },
};
#undef __PF


char * ll_proto_n2a(unsigned short id, char *buf, int len)
{
        int i;

	id = ntohs(id);

        for (i=0; i<sizeof(llproto_names)/sizeof(llproto_names[0]); i++) {
                 if (llproto_names[i].id == id)
			return llproto_names[i].name;
	}
        snprintf(buf, len, "[%d]", id);
        return buf;
}

int ll_proto_a2n(unsigned short *id, char *buf)
{
        int i;
        for (i=0; i<sizeof(llproto_names)/sizeof(llproto_names[0]); i++) {
                 if (strcasecmp(llproto_names[i].name, buf) == 0) {
			 *id = htons(llproto_names[i].id);
			 return 0;
		 }
	}
	if (get_u16(id, buf, 0))
		return -1;
	*id = htons(*id);
	return 0;
}
