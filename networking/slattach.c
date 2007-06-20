/*
 * Stripped down version of net-tools for busybox.
 *
 * Author: Ignacio Garcia Perez (iggarpe at gmail dot com)
 *
 * License: GPLv2 or later, see LICENSE file in this tarball.
 *
 * There are some differences from the standard net-tools slattach:
 *
 * - The -l option is not supported.
 *
 * - The -F options allows disabling of RTS/CTS flow control.
 */

#include "libbb.h"

/* Line discipline code table */
static const char *const proto_names[] = {
	"slip",		/* 0 */
	"cslip",	/* 1 */
	"slip6",	/* 2 */
	"cslip6",	/* 3 */
	"adaptive",	/* 8 */
	NULL
};

struct globals {
	int handle;
	int saved_disc;
	struct termios saved_state;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define handle       (G.handle      )
#define saved_disc   (G.saved_disc  )
#define saved_state  (G.saved_state )
#define INIT_G() do {} while (0)


/*
 * Save tty state and line discipline
 *
 * It is fine here to bail out on errors, since we haven modified anything yet
 */
static void save_state(void)
{
	/* Save line status */
	if (tcgetattr(handle, &saved_state) < 0)
		bb_perror_msg_and_die("get state");

	/* Save line discipline */
	if (ioctl(handle, TIOCGETD, &saved_disc) < 0)
		bb_perror_msg_and_die("get discipline");
}

/*
 * Restore state and line discipline for ALL managed ttys
 *
 * Restoring ALL managed ttys is the only way to have a single
 * hangup delay.
 *
 * Go on after errors: we want to restore as many controlled ttys
 * as possible.
 */
static void restore_state_and_exit(int exitcode) ATTRIBUTE_NORETURN;
static void restore_state_and_exit(int exitcode)
{
	struct termios state;

	/* Restore line discipline */
	if (ioctl(handle, TIOCSETD, &saved_disc) < 0) {
		bb_perror_msg("set discipline");
		exitcode = 1;
	}

	/* Hangup */
	memcpy(&state, &saved_state, sizeof(state));
	cfsetispeed(&state, B0);
	cfsetospeed(&state, B0);
	if (tcsetattr(handle, TCSANOW, &state) < 0) {
		bb_perror_msg("set state");
		exitcode = 1;
	}

	sleep(1);

	/* Restore line status */
	if (tcsetattr(handle, TCSANOW, &saved_state) < 0) {
		bb_perror_msg_and_die("set state");
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(handle);

	exit(exitcode);
}

/*
 * Set tty state, line discipline and encapsulation
 */
static void set_state(struct termios *state, int encap)
{
	int disc;

	/* Set line status */
	if (tcsetattr(handle, TCSANOW, state) < 0) {
		bb_perror_msg("set state");
		goto bad;
	}

	/* Set line discliple (N_SLIP always) */
	disc = N_SLIP;
	if (ioctl(handle, TIOCSETD, &disc) < 0) {
		bb_perror_msg("set discipline");
		goto bad;
	}

	/* Set encapsulation (SLIP, CSLIP, etc) */
	if (ioctl(handle, SIOCSIFENCAP, &encap) < 0) {
		bb_perror_msg("set encapsulation");
 bad:
		restore_state_and_exit(1);
	}
}

static void sig_handler(int signo)
{
	restore_state_and_exit(0);
}

int slattach_main(int argc, char **argv);
int slattach_main(int argc, char **argv)
{
	int i, encap, opt;
	struct termios state;
	const char *proto = "cslip";
	const char *extcmd;				/* Command to execute after hangup */
	const char *baud_str;
	int baud_code = -1;				/* Line baud rate (system code) */

	enum {
		OPT_p_proto  = 1 << 0,
		OPT_s_baud   = 1 << 1,
		OPT_c_extcmd = 1 << 2,
		OPT_e_quit   = 1 << 3,
		OPT_h_watch  = 1 << 4,
		OPT_m_nonraw = 1 << 5,
		OPT_L_local  = 1 << 6,
		OPT_F_noflow = 1 << 7,
	};

	INIT_G();

	/* Parse command line options */
	opt = getopt32(argc, argv, "p:s:c:ehmLF", &proto, &baud_str, &extcmd);
	/*argc -= optind;*/
	argv += optind;

	if (!*argv)
		bb_show_usage();

	encap = index_in_str_array(proto_names, proto);

	if (encap < 0)
		bb_error_msg_and_die("invalid protocol %s", proto);
	if (encap > 3)
		encap = 8;

	/* We want to know if the baud rate is valid before we start touching the ttys */
	if (opt & OPT_s_baud) {
		baud_code = tty_value_to_baud(xatoi(baud_str));
		if (baud_code < 0)
			bb_error_msg_and_die("invalid baud rate");
	}

	/* Trap signals in order to restore tty states upon exit */
	if (!(opt & OPT_e_quit)) {
		signal(SIGHUP, sig_handler);
		signal(SIGINT, sig_handler);
		signal(SIGQUIT, sig_handler);
		signal(SIGTERM, sig_handler);
	}

	/* Open tty */ 
	handle = open(*argv, O_RDWR | O_NDELAY);
	if (handle < 0) {
		char *buf = xasprintf("/dev/%s", *argv);
		handle = xopen(buf, O_RDWR | O_NDELAY);
		/* maybe if (ENABLE_FEATURE_CLEAN_UP) ?? */
		free(buf);
	}

	/* Save current tty state */
	save_state();

	/* Confgure tty */
	memcpy(&state, &saved_state, sizeof(state));
	if (!(opt & OPT_m_nonraw)) { /* raw not suppressed */
		memset(&state.c_cc, 0, sizeof(state.c_cc));
		state.c_cc[VMIN] = 1;
		state.c_iflag = IGNBRK | IGNPAR;
		state.c_oflag = 0;
		state.c_lflag = 0;
		state.c_cflag = CS8 | HUPCL | CREAD
		              | ((opt & OPT_L_local) ? CLOCAL : 0)
		              | ((opt & OPT_F_noflow) ? 0 : CRTSCTS);
	}

	if (opt & OPT_s_baud) {
		cfsetispeed(&state, baud_code);
		cfsetospeed(&state, baud_code);
	}

	set_state(&state, encap);

	/* Exit now if option -e was passed */
	if (opt & OPT_e_quit)
		return 0;

	/* If we're not requested to watch, just keep descriptor open
	 * till we are killed */
	if (!(opt & OPT_h_watch))
		while (1)
			sleep(24*60*60);

	/* Watch line for hangup */
	while (1) {
		if (ioctl(handle, TIOCMGET, &i) < 0 || !(i & TIOCM_CAR))
			goto no_carrier;
		sleep(15);
	}

 no_carrier:

	/* Execute command on hangup */
	if (opt & OPT_c_extcmd)
		system(extcmd);

	/* Restore states and exit */
	restore_state_and_exit(0);
}
