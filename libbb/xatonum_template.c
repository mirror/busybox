/*
You need to define the following (example):

#define type long
#define xstrtou(rest) xstrtoul##rest
#define xstrto(rest) xstrtol##rest
#define xatou(rest) xatoul##rest
#define xato(rest) xatol##rest
#define XSTR_UTYPE_MAX ULONG_MAX
#define XSTR_TYPE_MAX LONG_MAX
#define XSTR_TYPE_MIN LONG_MIN
#define XSTR_STRTOU strtoul
*/

unsigned type xstrtou(_range_sfx)(const char *numstr, int base,
		unsigned type lower,
		unsigned type upper,
		const struct suffix_mult *suffixes)
{
	unsigned type r;
	int old_errno;
	char *e;

	/* Disallow '-' and any leading whitespace.  Speed isn't critical here
	 * since we're parsing commandline args.  So make sure we get the
	 * actual isspace function rather than a lnumstrer macro implementaion. */
	if (*numstr == '-' || *numstr == '+' || (isspace)(*numstr))
		goto inval;

	/* Since this is a lib function, we're not allowed to reset errno to 0.
	 * Doing so could break an app that is deferring checking of errno.
	 * So, save the old value so that we can restore it if successful. */
	old_errno = errno;
	errno = 0;
	r = XSTR_STRTOU(numstr, &e, base);
	/* Do the initial validity check.  Note: The standards do not
	 * guarantee that errno is set if no digits were found.  So we
	 * must test for this explicitly. */
	if (errno || numstr == e)
		goto inval; /* error / no digits / illegal trailing chars */

	errno = old_errno;	/* Ok.  So restore errno. */

	/* Do optional suffix parsing.  Allow 'empty' suffix tables.
	 * Note that we also allow nul suffixes with associated multipliers,
	 * to allow for scaling of the numstr by some default multiplier. */
	if (suffixes) {
		while (suffixes->suffix) {
			if (strcmp(suffixes->suffix, e) == 0) {
				if (XSTR_UTYPE_MAX / suffixes->mult < r)
					goto range; /* overflow! */
				++e;
				r *= suffixes->mult;
				break;
			}
			++suffixes;
		}
	}

	/* Note: trailing space is an error.
	   It would be easy enough to allow though if desired. */
	if (*e)
		goto inval;
	/* Finally, check for range limits. */
	if (r >= lower && r <= upper)
		return r;
 range:
	bb_error_msg_and_die("number %s is not in %llu..%llu range",
		numstr, (unsigned long long)lower,
		(unsigned long long)upper);
 inval:
	bb_error_msg_and_die("invalid number '%s'", numstr);
}

unsigned type xstrtou(_range)(const char *numstr, int base,
		unsigned type lower,
		unsigned type upper)
{
	return xstrtou(_range_sfx)(numstr, base, lower, upper, NULL);
}

unsigned type xstrtou(_sfx)(const char *numstr, int base,
		const struct suffix_mult *suffixes)
{
	return xstrtou(_range_sfx)(numstr, base, 0, XSTR_UTYPE_MAX, suffixes);
}

unsigned type xstrtou()(const char *numstr, int base)
{
	return xstrtou(_range_sfx)(numstr, base, 0, XSTR_UTYPE_MAX, NULL);
}

unsigned type xatou(_range_sfx)(const char *numstr,
		unsigned type lower,
		unsigned type upper,
		const struct suffix_mult *suffixes)
{
	return xstrtou(_range_sfx)(numstr, 10, lower, upper, suffixes);
}

unsigned type xatou(_range)(const char *numstr,
		unsigned type lower,
		unsigned type upper)
{
	return xstrtou(_range_sfx)(numstr, 10, lower, upper, NULL);
}

unsigned type xatou(_sfx)(const char *numstr,
		const struct suffix_mult *suffixes)
{
	return xstrtou(_range_sfx)(numstr, 10, 0, XSTR_UTYPE_MAX, suffixes);
}

unsigned type xatou()(const char *numstr)
{
	return xatou(_sfx)(numstr, NULL);
}

/* Signed ones */

type xstrto(_range_sfx)(const char *numstr, int base,
		type lower,
		type upper,
		const struct suffix_mult *suffixes)
{
	unsigned type u = XSTR_TYPE_MAX;
	type r;
	const char *p = numstr;

	if (p[0] == '-') {
		++p;
		++u;	/* two's complement */
	}

	r = xstrtou(_range_sfx)(p, base, 0, u, suffixes);

	if (*numstr == '-') {
		r = -r;
	}

	if (r < lower || r > upper) {
		bb_error_msg_and_die("number %s is not in %lld..%lld range",
				numstr, (long long)lower, (long long)upper);
	}

	return r;
}

type xstrto(_range)(const char *numstr, int base, type lower, type upper)
{
	return xstrto(_range_sfx)(numstr, base, lower, upper, NULL);
}

type xato(_range_sfx)(const char *numstr,
		type lower,
		type upper,
		const struct suffix_mult *suffixes)
{
	return xstrto(_range_sfx)(numstr, 10, lower, upper, suffixes);
}

type xato(_range)(const char *numstr, type lower, type upper)
{
	return xstrto(_range_sfx)(numstr, 10, lower, upper, NULL);
}

type xato(_sfx)(const char *numstr, const struct suffix_mult *suffixes)
{
	return xstrto(_range_sfx)(numstr, 10, XSTR_TYPE_MIN, XSTR_TYPE_MAX, suffixes);
}

type xato()(const char *numstr)
{
	return xstrto(_range_sfx)(numstr, 10, XSTR_TYPE_MIN, XSTR_TYPE_MAX, NULL);
}

#undef type
#undef xstrtou
#undef xstrto
#undef xatou
#undef xato
#undef XSTR_UTYPE_MAX
#undef XSTR_TYPE_MAX
#undef XSTR_TYPE_MIN
#undef XSTR_STRTOU
