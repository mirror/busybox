/*
 * $Id: telnet.c,v 1.2 2000/05/01 19:10:52 erik Exp $
 * Mini telnet implementation for busybox
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This version of telnet is adapted (but very heavily modified) from the 
 * telnet in netkit-telnet 0.16, which is:
 *
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Original copyright notice is retained at the end of this file.
 */

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <termios.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#define TELOPTS
#include <arpa/telnet.h>
#include <arpa/inet.h>

static int STDIN = 0;
static int STDOUT = 1;
static const char *telnet_usage = "telnet host [port]\n\n";
static struct termios saved_tc;
static unsigned char options[NTELOPTS];
static char tr_state = 0; /* telnet send and receive state */
static unsigned char subbuffer[256];
static unsigned char *subpointer, *subend;
#define	SB_CLEAR()	subpointer = subbuffer;
#define	SB_TERM()	{ subend = subpointer; SB_CLEAR(); }
#define	SB_ACCUM(c)	if (subpointer < (subbuffer+sizeof subbuffer)) { *subpointer++ = (c); }
#define	SB_GET()	(*subpointer++)
#define	SB_PEEK()	(*subpointer)
#define	SB_EOF()	(subpointer >= subend)
#define	SB_LEN()	(subend - subpointer)

#define TELNETPORT		23
#define MASK_WILL		0x01
#define MASK_WONT		0x04
#define MASK_DO			0x10
#define MASK_DONT		0x40

#define TFLAG_ISSET(opt, flag) (options[opt] & MASK_##flag)
#define TFLAG_SET(opt, flag) (options[opt] |= MASK_##flag)
#define TFLAG_CLR(opt, flag) (options[opt] &= ~MASK_##flag)

#define PERROR(ctx) do { fprintf(stderr, "%s: %s\n", ctx, strerror(errno)); \
                         return; } while (0)
						 
#define	TS_DATA		0
#define	TS_IAC		1
#define	TS_WILL		2
#define	TS_WONT		3
#define	TS_DO		4
#define	TS_DONT		5
#define	TS_CR		6
#define	TS_SB		7		/* sub-option collection */
#define	TS_SE		8		/* looking for sub-option end */

/* prototypes */
static void telnet_init(void);
static void telnet_start(char *host, int port);
static void telnet_shutdown(void);
/* ******************************************************************* */
#define SENDCOMMAND(c,o) \
	char buf[3]; \
	buf[0] = IAC; buf[1] = c; buf[2] = o; \
	write(s, buf, sizeof(buf)); 

static inline void telnet_sendwill(int s, int c) { SENDCOMMAND(WILL, c); }
static inline void telnet_sendwont(int s, int c) { SENDCOMMAND(WONT, c); }
static inline void telnet_senddo(int s, int c) { SENDCOMMAND(DO, c); }
static inline void telnet_senddont(int s, int c) { SENDCOMMAND(DONT, c); }

static void telnet_setoptions(int s)
{
	/*
	telnet_sendwill(s, TELOPT_NAWS); TFLAG_SET(TELOPT_NAWS, WILL);
	telnet_sendwill(s, TELOPT_TSPEED); TFLAG_SET(TELOPT_TSPEED, WILL);
	telnet_sendwill(s, TELOPT_NEW_ENVIRON); TFLAG_SET(TELOPT_NEW_ENVIRON, WILL);
	telnet_senddo(s, TELOPT_STATUS); TFLAG_SET(TELOPT_STATUS, DO);
	telnet_sendwill(s, TELOPT_TTYPE); TFLAG_SET(TELOPT_TTYPE, WILL);
	*/
	telnet_senddo(s, TELOPT_SGA); TFLAG_SET(TELOPT_SGA, DO);
	telnet_sendwill(s, TELOPT_LFLOW); TFLAG_SET(TELOPT_LFLOW, WILL);
	telnet_sendwill(s, TELOPT_LINEMODE); TFLAG_SET(TELOPT_LINEMODE, WILL);
	telnet_senddo(s, TELOPT_BINARY); TFLAG_SET(TELOPT_BINARY, DO);
	telnet_sendwill(s, TELOPT_BINARY); TFLAG_SET(TELOPT_BINARY, WILL);
}

static void telnet_suboptions(int net)
{
	char buf[256];
	switch (SB_GET()) {
		case TELOPT_TTYPE:
			if (TFLAG_ISSET(TELOPT_TTYPE, WONT)) return;
			if (SB_EOF() || SB_GET() != TELQUAL_SEND) {
      			return;
    		} else {
      			const char *name = getenv("TERM");
				if (name) {
					snprintf(buf, sizeof(buf), "%c%c%c%c%s%c%c", IAC, SB,
						TELOPT_TTYPE, TELQUAL_IS, name, IAC, SE);
					write(net, buf, strlen(name)+6);
				}
			}
    		break;
  		case TELOPT_TSPEED:
			if (TFLAG_ISSET(TELOPT_TSPEED, WONT)) return;
    		if (SB_EOF()) return;
    		if (SB_GET() == TELQUAL_SEND) {
				/*
      			long oospeed, iispeed;
      			netoring.printf("%c%c%c%c%ld,%ld%c%c", IAC, SB, TELOPT_TSPEED,
		      TELQUAL_IS, oospeed, iispeed, IAC, SE);
    		    */
			}
    		break;
		/*
		case TELOPT_LFLOW:
			if (TFLAG_ISSET(TELOPT_LFLOW, WONT)) return;
			if (SB_EOF()) return;
    		switch(SB_GET()) {
    			case 1: localflow = 1; break;
				case 0: localflow = 0; break;
				default: return;
			}
    		break;
  		case TELOPT_LINEMODE:
			if (TFLAG_ISSET(TELOPT_LINEMODE, WONT)) return;
			if (SB_EOF()) return;
			switch (SB_GET()) {
    			case WILL: lm_will(subpointer, SB_LEN()); break;
				case WONT: lm_wont(subpointer, SB_LEN()); break;
				case DO: lm_do(subpointer, SB_LEN()); break;
				case DONT: lm_dont(subpointer, SB_LEN()); break;
				case LM_SLC: slc(subpointer, SB_LEN()); break;
				case LM_MODE: lm_mode(subpointer, SB_LEN(), 0); break;
				default: break;
			}
    		break;
		case TELOPT_ENVIRON:
			if (SB_EOF()) return;
			switch(SB_PEEK()) {
				case TELQUAL_IS:
				case TELQUAL_INFO:
					if (TFLAG_ISSET(TELOPT_ENVIRON, DONT)) return;
					break;
				case TELQUAL_SEND:
					if (TFLAG_ISSET(TELOPT_ENVIRON, WONT)) return;
					break;
				default:
					return;
			}
			env_opt(subpointer, SB_LEN());
			break;
		*/
		case TELOPT_XDISPLOC:
			if (TFLAG_ISSET(TELOPT_XDISPLOC, WONT)) return;
    		if (SB_EOF()) return;
			if (SB_GET() == TELQUAL_SEND) {
				const char *dp = getenv("DISPLAY");
				if (dp) {
					snprintf(buf, sizeof(buf), "%c%c%c%c%s%c%c", IAC, SB,
						TELOPT_XDISPLOC, TELQUAL_IS, dp, IAC, SE);
					write(net, buf, strlen(dp)+6);
				}
			}
    		break;
    	default:
			break;
	}
}

static void sighandler(int sig)
{
	telnet_shutdown();
	exit(0);
}

static int telnet_send(int tty, int net)
{
	int ret;
	unsigned char ch;

	while ((ret = read(tty, &ch, 1)) > 0) {
		if (ch == 29) { /* 29 -- ctrl-] */
			/* do something here? */
			exit(0);
		} else {
			ret = write(net, &ch, 1);
			break;
		}
	}
	if (ret == -1 && errno == EWOULDBLOCK) return 1;
	return ret;
}

static int telnet_recv(int net, int tty)
{
	/* globals: tr_state - telnet receive state */
	int ret, c = 0;
	unsigned char ch;

	while ((ret = read(net, &ch, 1)) > 0) {
		c = ch;
		/* printf("%02X ", c); fflush(stdout); */
		switch (tr_state) {
			case TS_DATA:
				if (c == IAC) {
					tr_state = TS_IAC;
					break;
				} else {
					write(tty, &c, 1);
				}
				break;
			case TS_IAC:
				switch (c) {
					case WILL:
						tr_state = TS_WILL; break;
					case WONT:
						tr_state = TS_WONT; break;
					case DO:
						tr_state = TS_DO; break;
					case DONT:
						tr_state = TS_DONT; break;
					case SB:
						SB_CLEAR();
						tr_state = TS_SB; break;
					case IAC:
						write(tty, &c, 1); /* fallthrough */
					default:
						tr_state = TS_DATA;
				}
			
			/* subnegotiation -- ignored for now */
			case TS_SB:
				if (c == IAC) tr_state = TS_SE;
				else SB_ACCUM(c);
				break;
			case TS_SE:
				if (c == IAC) {
					SB_ACCUM(IAC);
					tr_state = TS_SB;
				} else if (c == SE) {
					SB_ACCUM(IAC);
					SB_ACCUM(SE);
					subpointer -= 2;
					SB_TERM();
					telnet_suboptions(net);
					tr_state = TS_DATA;
				}
			    /* otherwise this is an error, but we ignore it for now */
				break;
			/* handle incoming requests */
			case TS_WILL: 
				printf("WILL %s\n", telopts[c]);
				if (!TFLAG_ISSET(c, DO)) {
					if (c == TELOPT_BINARY) {
						TFLAG_SET(c, DO);
						TFLAG_CLR(c, DONT);
						telnet_senddo(net, c);
					} else {
						TFLAG_SET(c, DONT);
						telnet_senddont(net, c);
					}
				}
				telnet_senddont(net, c);
				tr_state = TS_DATA;
				break;
			case TS_WONT:
				printf("WONT %s\n", telopts[c]);
				if (!TFLAG_ISSET(c, DONT)) {
					TFLAG_SET(c, DONT);
					TFLAG_CLR(c, DO);
					telnet_senddont(net, c);
				}
				tr_state = TS_DATA;
				break;
			case TS_DO:
				printf("DO %s\n", telopts[c]);
				if (!TFLAG_ISSET(c, WILL)) {
					if (c == TELOPT_BINARY) {
						TFLAG_SET(c, WILL);
						TFLAG_CLR(c, WONT);
						telnet_sendwill(net, c);
					} else {
						TFLAG_SET(c, WONT);
						telnet_sendwont(net, c);
					}
				}
				tr_state = TS_DATA;
				break;
			case TS_DONT:
				printf("DONT %s\n", telopts[c]);
				if (!TFLAG_ISSET(c, WONT)) {
					TFLAG_SET(c, WONT);
					TFLAG_CLR(c, WILL);
					telnet_sendwont(net, c);
				}
				tr_state = TS_DATA;
				break;
		}
					
	}
	if (ret == -1 && errno == EWOULDBLOCK) return 1;
	return ret;
}

/* ******************************************************************* */
static void telnet_init(void)
{
	struct termios tmp_tc;
	cc_t esc = (']' & 0x1f); /* ctrl-] */
	
	memset(options, 0, sizeof(options));
	SB_CLEAR();

	tcgetattr(STDIN, &saved_tc);

	tmp_tc = saved_tc;
    tmp_tc.c_lflag &= ~ECHO; /* echo */
	tmp_tc.c_oflag |= ONLCR; /* NL->CRLF translation */
	tmp_tc.c_iflag |= ICRNL; 
	tmp_tc.c_iflag &= ~(IXANY|IXOFF|IXON); /* no flow control */
	tmp_tc.c_lflag |= ISIG; /* trap signals */
	tmp_tc.c_lflag &= ~ICANON; /* edit mode  */
   
	/* misc settings, compat with default telnet stuff */
	tmp_tc.c_oflag &= ~TABDLY;
	
	/* 8-bit clean */
	tmp_tc.c_iflag &= ~ISTRIP;
	tmp_tc.c_cflag &= ~(CSIZE|PARENB);
	tmp_tc.c_cflag |= saved_tc.c_cflag & (CSIZE|PARENB);
	tmp_tc.c_oflag |= OPOST;

	/* set escape character */
	tmp_tc.c_cc[VEOL] = esc;
	tcsetattr(STDIN, TCSADRAIN, &tmp_tc);
}

static void telnet_start(char *hostname, int port)
{
    struct hostent *host = 0;
	struct sockaddr_in addr;
    int s, c;
	fd_set rfds, wfds;
	
	memset(&addr, 0, sizeof(addr));
	host = gethostbyname(hostname);
	if (!host) {
		fprintf(stderr, "Unknown host: %s\n", hostname);
		return;
	}
	addr.sin_family = host->h_addrtype;
	memcpy(&addr.sin_addr, host->h_addr, sizeof(addr.sin_addr));
	addr.sin_port = htons(port);

	printf("Trying %s...\n", inet_ntoa(addr.sin_addr));
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) PERROR("socket");
	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	    PERROR("connect");
	printf("Connected to %s\n", hostname);
	printf("Escape character is ^]\n");

	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGPIPE, sighandler);
	signal(SIGWINCH, sighandler);

	/* make inputs nonblocking */
	c = 1;
 	ioctl(s, FIONBIO, &c);
	ioctl(STDIN, FIONBIO, &c);
	
	if (port == TELNETPORT) telnet_setoptions(s);
	
	/* shuttle data back and forth between tty and socket */
	while (1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
	
		FD_SET(s, &rfds);
		/* FD_SET(s, &wfds); */
		FD_SET(STDIN, &rfds);
		/* FD_SET(STDOUT, &wfds); */
	
		if ((c = select(s+1, &rfds, &wfds, 0, 0))) {
			if (c == -1) {
			    /* handle errors */
				PERROR("select");
			}
			if (FD_ISSET(s, &rfds)) {
				/* input from network */
				FD_CLR(s, &rfds);
				c = telnet_recv(s, STDOUT);
				if (c == 0) break;
				if (c < 0) PERROR("telnet_recv");
			}
			if (FD_ISSET(STDIN, &rfds)) {
				/* input from tty */
				FD_CLR(STDIN, &rfds);
				c = telnet_send(STDIN, s);
				if (c == 0) break;
				if (c < 0) PERROR("telnet_send");
			}
		}
	}
	
    return;
}

static void telnet_shutdown(void)
{
	printf("\n");
	tcsetattr(STDIN, TCSANOW, &saved_tc);
}

#ifdef STANDALONE_TELNET
void usage(const char *msg)
{
	printf("%s", msg);
	exit(0);
}

int main(int argc, char **argv)
#else
int telnet_main(int argc, char **argv)
#endif
{
    int port = TELNETPORT;
	
    argc--; argv++;
    if (argc < 1) usage(telnet_usage);
    if (argc > 1) port = atoi(argv[1]);
    telnet_init();
    atexit(telnet_shutdown);

    telnet_start(argv[0], port);
    return 0;
}

/*
 * Copyright (c) 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
