/* vi: set sw=4 ts=4: */
/*
 * telnet implementation for busybox
 *
 * Author: Tomi Ollila <too@iki.fi>
 * Copyright (C) 1994-2000 by Tomi Ollila
 *
 * Created: Thu Apr  7 13:29:41 1994 too
 * Last modified: Fri Jun  9 14:34:24 2000 too
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * HISTORY
 * Revision 3.1  1994/04/17  11:31:54  too
 * initial revision
 * Modified 2000/06/13 for inclusion into BusyBox by Erik Andersen <andersen@codepoet.org>
 * Modified 2001/05/07 to add ability to pass TTYPE to remote host by Jim McQuillan
 * <jam@ltsp.org>
 * Modified 2004/02/11 to add ability to pass the USER variable to remote host
 * by Fernando Silveira <swrh@gmx.net>
 *
 */

#include <termios.h>
#include <arpa/telnet.h>
#include <netinet/in.h>
#include "libbb.h"

#ifdef DOTRACE
#define TRACE(x, y) do { if (x) printf y; } while (0)
#else
#define TRACE(x, y)
#endif

enum {
	DATABUFSIZE = 128,
	IACBUFSIZE  = 128,

	CHM_TRY = 0,
	CHM_ON = 1,
	CHM_OFF = 2,

	UF_ECHO = 0x01,
	UF_SGA = 0x02,

	TS_0 = 1,
	TS_IAC = 2,
	TS_OPT = 3,
	TS_SUB1 = 4,
	TS_SUB2 = 5,
};

typedef unsigned char byte;

struct globals {
	int	netfd; /* console fd:s are 0 and 1 (and 2) */
	short	iaclen; /* could even use byte */
	byte	telstate; /* telnet negotiation state from network input */
	byte	telwish;  /* DO, DONT, WILL, WONT */
	byte    charmode;
	byte    telflags;
	byte	gotsig;
	byte	do_termios;
#if ENABLE_FEATURE_TELNET_TTYPE
	char	*ttype;
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
	const char *autologin;
#endif
#if ENABLE_FEATURE_AUTOWIDTH
	unsigned win_width, win_height;
#endif
	/* same buffer used both for network and console read/write */
	char    buf[DATABUFSIZE];
	/* buffer to handle telnet negotiations */
	char    iacbuf[IACBUFSIZE];
	struct termios termios_def;
	struct termios termios_raw;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
void BUG_telnet_globals_too_big(void);
#define INIT_G() do { \
	if (sizeof(G) > COMMON_BUFSIZE) \
		BUG_telnet_globals_too_big(); \
	/* memset(&G, 0, sizeof G); - already is */ \
} while (0)

/* Function prototypes */
static void rawmode(void);
static void cookmode(void);
static void do_linemode(void);
static void will_charmode(void);
static void telopt(byte c);
static int subneg(byte c);

static void iacflush(void)
{
	write(G.netfd, G.iacbuf, G.iaclen);
	G.iaclen = 0;
}

#define write_str(fd, str) write(fd, str, sizeof(str) - 1)

static void doexit(int ev) ATTRIBUTE_NORETURN;
static void doexit(int ev)
{
	cookmode();
	exit(ev);
}

static void conescape(void)
{
	char b;

	if (G.gotsig)	/* came from line  mode... go raw */
		rawmode();

	write_str(1, "\r\nConsole escape. Commands are:\r\n\n"
			" l	go to line mode\r\n"
			" c	go to character mode\r\n"
			" z	suspend telnet\r\n"
			" e	exit telnet\r\n");

	if (read(STDIN_FILENO, &b, 1) <= 0)
		doexit(EXIT_FAILURE);

	switch (b) {
	case 'l':
		if (!G.gotsig) {
			do_linemode();
			goto rrturn;
		}
		break;
	case 'c':
		if (G.gotsig) {
			will_charmode();
			goto rrturn;
		}
		break;
	case 'z':
		cookmode();
		kill(0, SIGTSTP);
		rawmode();
		break;
	case 'e':
		doexit(EXIT_SUCCESS);
	}

	write_str(1, "continuing...\r\n");

	if (G.gotsig)
		cookmode();

 rrturn:
	G.gotsig = 0;

}

static void handlenetoutput(int len)
{
	/* here we could do smart tricks how to handle 0xFF:s in output
	 * stream like writing twice every sequence of FF:s (thus doing
	 * many write()s. But I think interactive telnet application does
	 * not need to be 100% 8-bit clean, so changing every 0xff:s to
	 * 0x7f:s
	 *
	 * 2002-mar-21, Przemyslaw Czerpak (druzus@polbox.com)
	 * I don't agree.
	 * first - I cannot use programs like sz/rz
	 * second - the 0x0D is sent as one character and if the next
	 *	char is 0x0A then it's eaten by a server side.
	 * third - whay doy you have to make 'many write()s'?
	 *	I don't understand.
	 * So I implemented it. It's realy useful for me. I hope that
	 * others people will find it interesting too.
	 */

	int i, j;
	byte * p = (byte*)G.buf;
	byte outbuf[4*DATABUFSIZE];

	for (i = len, j = 0; i > 0; i--, p++) {
		if (*p == 0x1d) {
			conescape();
			return;
		}
		outbuf[j++] = *p;
		if (*p == 0xff)
			outbuf[j++] = 0xff;
		else if (*p == 0x0d)
			outbuf[j++] = 0x00;
	}
	if (j > 0)
		write(G.netfd, outbuf, j);
}

static void handlenetinput(int len)
{
	int i;
	int cstart = 0;

	for (i = 0; i < len; i++) {
		byte c = G.buf[i];

		if (G.telstate == 0) { /* most of the time state == 0 */
			if (c == IAC) {
				cstart = i;
				G.telstate = TS_IAC;
			}
		} else
			switch (G.telstate) {
			case TS_0:
				if (c == IAC)
					G.telstate = TS_IAC;
				else
					G.buf[cstart++] = c;
				break;

			case TS_IAC:
				if (c == IAC) { /* IAC IAC -> 0xFF */
					G.buf[cstart++] = c;
					G.telstate = TS_0;
					break;
				}
				/* else */
				switch (c) {
				case SB:
					G.telstate = TS_SUB1;
					break;
				case DO:
				case DONT:
				case WILL:
				case WONT:
					G.telwish =  c;
					G.telstate = TS_OPT;
					break;
				default:
					G.telstate = TS_0;	/* DATA MARK must be added later */
				}
				break;
			case TS_OPT: /* WILL, WONT, DO, DONT */
				telopt(c);
				G.telstate = TS_0;
				break;
			case TS_SUB1: /* Subnegotiation */
			case TS_SUB2: /* Subnegotiation */
				if (subneg(c))
					G.telstate = TS_0;
				break;
			}
	}
	if (G.telstate) {
		if (G.iaclen) iacflush();
		if (G.telstate == TS_0)	G.telstate = 0;
		len = cstart;
	}

	if (len)
		write(STDOUT_FILENO, G.buf, len);
}

static void putiac(int c)
{
	G.iacbuf[G.iaclen++] = c;
}

static void putiac2(byte wwdd, byte c)
{
	if (G.iaclen + 3 > IACBUFSIZE)
		iacflush();

	putiac(IAC);
	putiac(wwdd);
	putiac(c);
}

#if ENABLE_FEATURE_TELNET_TTYPE
static void putiac_subopt(byte c, char *str)
{
	int	len = strlen(str) + 6;   // ( 2 + 1 + 1 + strlen + 2 )

	if (G.iaclen + len > IACBUFSIZE)
		iacflush();

	putiac(IAC);
	putiac(SB);
	putiac(c);
	putiac(0);

	while (*str)
		putiac(*str++);

	putiac(IAC);
	putiac(SE);
}
#endif

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
static void putiac_subopt_autologin(void)
{
	int len = strlen(G.autologin) + 6;	// (2 + 1 + 1 + strlen + 2)
	const char *user = "USER";

	if (G.iaclen + len > IACBUFSIZE)
		iacflush();

	putiac(IAC);
	putiac(SB);
	putiac(TELOPT_NEW_ENVIRON);
	putiac(TELQUAL_IS);
	putiac(NEW_ENV_VAR);

	while (*user)
		putiac(*user++);

	putiac(NEW_ENV_VALUE);

	while (*G.autologin)
		putiac(*G.autologin++);

	putiac(IAC);
	putiac(SE);
}
#endif

#if ENABLE_FEATURE_AUTOWIDTH
static void putiac_naws(byte c, int x, int y)
{
	if (G.iaclen + 9 > IACBUFSIZE)
		iacflush();

	putiac(IAC);
	putiac(SB);
	putiac(c);

	putiac((x >> 8) & 0xff);
	putiac(x & 0xff);
	putiac((y >> 8) & 0xff);
	putiac(y & 0xff);

	putiac(IAC);
	putiac(SE);
}
#endif

static char const escapecharis[] ALIGN1 = "\r\nEscape character is ";

static void setConMode(void)
{
	if (G.telflags & UF_ECHO) {
		if (G.charmode == CHM_TRY) {
			G.charmode = CHM_ON;
			printf("\r\nEntering character mode%s'^]'.\r\n", escapecharis);
			rawmode();
		}
	} else {
		if (G.charmode != CHM_OFF) {
			G.charmode = CHM_OFF;
			printf("\r\nEntering line mode%s'^C'.\r\n", escapecharis);
			cookmode();
		}
	}
}

static void will_charmode(void)
{
	G.charmode = CHM_TRY;
	G.telflags |= (UF_ECHO | UF_SGA);
	setConMode();

	putiac2(DO, TELOPT_ECHO);
	putiac2(DO, TELOPT_SGA);
	iacflush();
}

static void do_linemode(void)
{
	G.charmode = CHM_TRY;
	G.telflags &= ~(UF_ECHO | UF_SGA);
	setConMode();

	putiac2(DONT, TELOPT_ECHO);
	putiac2(DONT, TELOPT_SGA);
	iacflush();
}

static void to_notsup(char c)
{
	if (G.telwish == WILL)
		putiac2(DONT, c);
	else if (G.telwish == DO)
		putiac2(WONT, c);
}

static void to_echo(void)
{
	/* if server requests ECHO, don't agree */
	if (G.telwish == DO) {
		putiac2(WONT, TELOPT_ECHO);
		return;
	}
	if (G.telwish == DONT)
		return;

	if (G.telflags & UF_ECHO) {
		if (G.telwish == WILL)
			return;
	} else if (G.telwish == WONT)
		return;

	if (G.charmode != CHM_OFF)
		G.telflags ^= UF_ECHO;

	if (G.telflags & UF_ECHO)
		putiac2(DO, TELOPT_ECHO);
	else
		putiac2(DONT, TELOPT_ECHO);

	setConMode();
	write_str(1, "\r\n");  /* sudden modec */
}

static void to_sga(void)
{
	/* daemon always sends will/wont, client do/dont */

	if (G.telflags & UF_SGA) {
		if (G.telwish == WILL)
			return;
	} else if (G.telwish == WONT)
		return;

	G.telflags ^= UF_SGA; /* toggle */
	if (G.telflags & UF_SGA)
		putiac2(DO, TELOPT_SGA);
	else
		putiac2(DONT, TELOPT_SGA);
}

#if ENABLE_FEATURE_TELNET_TTYPE
static void to_ttype(void)
{
	/* Tell server we will (or won't) do TTYPE */

	if (G.ttype)
		putiac2(WILL, TELOPT_TTYPE);
	else
		putiac2(WONT, TELOPT_TTYPE);
}
#endif

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
static void to_new_environ(void)
{
	/* Tell server we will (or will not) do AUTOLOGIN */

	if (G.autologin)
		putiac2(WILL, TELOPT_NEW_ENVIRON);
	else
		putiac2(WONT, TELOPT_NEW_ENVIRON);
}
#endif

#if ENABLE_FEATURE_AUTOWIDTH
static void to_naws(void)
{
	/* Tell server we will do NAWS */
	putiac2(WILL, TELOPT_NAWS);
}
#endif

static void telopt(byte c)
{
	switch (c) {
	case TELOPT_ECHO:
		to_echo(); break;
	case TELOPT_SGA:
		to_sga(); break;
#if ENABLE_FEATURE_TELNET_TTYPE
	case TELOPT_TTYPE:
		to_ttype(); break;
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
	case TELOPT_NEW_ENVIRON:
		to_new_environ(); break;
#endif
#if ENABLE_FEATURE_AUTOWIDTH
	case TELOPT_NAWS:
		to_naws();
		putiac_naws(c, G.win_width, G.win_height);
		break;
#endif
	default:
		to_notsup(c);
		break;
	}
}

/* subnegotiation -- ignore all (except TTYPE,NAWS) */
static int subneg(byte c)
{
	switch (G.telstate) {
	case TS_SUB1:
		if (c == IAC)
			G.telstate = TS_SUB2;
#if ENABLE_FEATURE_TELNET_TTYPE
		else
		if (c == TELOPT_TTYPE)
			putiac_subopt(TELOPT_TTYPE, G.ttype);
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
		else
		if (c == TELOPT_NEW_ENVIRON)
			putiac_subopt_autologin();
#endif
		break;
	case TS_SUB2:
		if (c == SE)
			return TRUE;
		G.telstate = TS_SUB1;
		/* break; */
	}
	return FALSE;
}

static void fgotsig(int sig)
{
	G.gotsig = sig;
}


static void rawmode(void)
{
	if (G.do_termios)
		tcsetattr(0, TCSADRAIN, &G.termios_raw);
}

static void cookmode(void)
{
	if (G.do_termios)
		tcsetattr(0, TCSADRAIN, &G.termios_def);
}

/* poll gives smaller (-70 bytes) code */
#define USE_POLL 1

int telnet_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int telnet_main(int argc, char **argv)
{
	char *host;
	int port;
	int len;
#ifdef USE_POLL
	struct pollfd ufds[2];
#else
	fd_set readfds;
	int maxfd;
#endif

	INIT_G();

#if ENABLE_FEATURE_AUTOWIDTH
	get_terminal_width_height(0, &G.win_width, &G.win_height);
#endif

#if ENABLE_FEATURE_TELNET_TTYPE
	G.ttype = getenv("TERM");
#endif

	if (tcgetattr(0, &G.termios_def) >= 0) {
		G.do_termios = 1;
		G.termios_raw = G.termios_def;
		cfmakeraw(&G.termios_raw);
	}

	if (argc < 2)
		bb_show_usage();

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
	if (1 & getopt32(argv, "al:", &G.autologin))
		G.autologin = getenv("USER");
	argv += optind;
#else
	argv++;
#endif
	if (!*argv)
		bb_show_usage();
	host = *argv++;
	port = bb_lookup_port(*argv ? *argv++ : "telnet", "tcp", 23);
	if (*argv) /* extra params?? */
		bb_show_usage();

	G.netfd = create_and_connect_stream_or_die(host, port);

	setsockopt(G.netfd, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1));

	signal(SIGINT, fgotsig);

#ifdef USE_POLL
	ufds[0].fd = 0; ufds[1].fd = G.netfd;
	ufds[0].events = ufds[1].events = POLLIN;
#else
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);
	FD_SET(G.netfd, &readfds);
	maxfd = G.netfd + 1;
#endif

	while (1) {
#ifndef USE_POLL
		fd_set rfds = readfds;

		switch (select(maxfd, &rfds, NULL, NULL, NULL))
#else
		switch (poll(ufds, 2, -1))
#endif
		{
		case 0:
			/* timeout */
		case -1:
			/* error, ignore and/or log something, bay go to loop */
			if (G.gotsig)
				conescape();
			else
				sleep(1);
			break;
		default:

#ifdef USE_POLL
			if (ufds[0].revents) /* well, should check POLLIN, but ... */
#else
			if (FD_ISSET(STDIN_FILENO, &rfds))
#endif
			{
				len = read(STDIN_FILENO, G.buf, DATABUFSIZE);
				if (len <= 0)
					doexit(EXIT_SUCCESS);
				TRACE(0, ("Read con: %d\n", len));
				handlenetoutput(len);
			}

#ifdef USE_POLL
			if (ufds[1].revents) /* well, should check POLLIN, but ... */
#else
			if (FD_ISSET(G.netfd, &rfds))
#endif
			{
				len = read(G.netfd, G.buf, DATABUFSIZE);
				if (len <= 0) {
					write_str(1, "Connection closed by foreign host\r\n");
					doexit(EXIT_FAILURE);
				}
				TRACE(0, ("Read netfd (%d): %d\n", G.netfd, len));
				handlenetinput(len);
			}
		}
	}
}
