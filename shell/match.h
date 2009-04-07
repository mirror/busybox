/* match.h - interface to shell ##/%% matching code */

typedef char *(*scan_t)(char *string, char *match, bool zero);

char *scanleft(char *string, char *match, bool zero);
char *scanright(char *string, char *match, bool zero);

static inline scan_t pick_scan(char op1, char op2, bool *zero)
{
	/* #  - scanleft
	 * ## - scanright
	 * %  - scanright
	 * %% - scanleft
	 */
	if (op1 == '#') {
		*zero = true;
		return op1 == op2 ? scanright : scanleft;
	} else {
		*zero = false;
		return op1 == op2 ? scanleft : scanright;
	}
}
