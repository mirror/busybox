/* vi: set sw=4 ts=4: */
/*
 * Copyright 1989 - 1994, Julianne Frances Haugh <jockgrrl@austin.rr.com>
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
 * 3. Neither the name of Julianne F. Haugh nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JULIE HAUGH AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JULIE HAUGH OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This version of obscure.c contains modifications to support "cracklib"
 * by Alec Muffet (alec.muffett@uk.sun.com).  You must obtain the Cracklib
 * library source code for this function to operate.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "libbb.h"

/*
 * can't be a palindrome - like `R A D A R' or `M A D A M'
 */

static int palindrome(const char *newval)
{
	int i, j;

	i = strlen(newval);

	for (j = 0; j < i; j++)
		if (newval[i - j - 1] != newval[j])
			return 0;

	return 1;
}

/*
 * more than half of the characters are different ones.
 */

static int similiar(const char *old, const char *newval)
{
	int i, j;

	for (i = j = 0; newval[i] && old[i]; i++)
		if (strchr(newval, old[i]))
			j++;

	if (i >= j * 2)
		return 0;

	return 1;
}

/*
 * a nice mix of characters.
 */

static int simple(const char *newval)
{
	int digits = 0;
	int uppers = 0;
	int lowers = 0;
	int others = 0;
	int c;
	int size;
	int i;

	for (i = 0; (c = *newval++) != 0; i++) {
		if (isdigit(c))
			digits = c;
		else if (isupper(c))
			uppers = c;
		else if (islower(c))
			lowers = c;
		else
			others = c;
	}

	/*
	 * The scam is this - a password of only one character type
	 * must be 8 letters long.  Two types, 7, and so on.
	 */

	size = 9;
	if (digits)
		size--;
	if (uppers)
		size--;
	if (lowers)
		size--;
	if (others)
		size--;

	if (size <= i)
		return 0;

	return 1;
}

static char *str_lower(char *string)
{
	char *cp;

	for (cp = string; *cp; cp++)
		*cp = tolower(*cp);
	return string;
}

static const char *
password_check(const char *old, const char *newval, const struct passwd *pwdp)
{
	const char *msg;
	char *newmono, *wrapped;
	int lenwrap;

	if (strcmp(newval, old) == 0)
		return "no change";
	if (simple(newval))
		return "too simple";

	msg = NULL;
	newmono = str_lower(bb_xstrdup(newval));
	lenwrap = strlen(old);
	wrapped = (char *) xmalloc(lenwrap * 2 + 1);
	str_lower(strcpy(wrapped, old));

	if (palindrome(newmono))
		msg = "a palindrome";

	else if (strcmp(wrapped, newmono) == 0)
		msg = "case changes only";

	else if (similiar(wrapped, newmono))
		msg = "too similiar";

	else {
		safe_strncpy(wrapped + lenwrap, wrapped, lenwrap + 1);
		if (strstr(wrapped, newmono))
			msg = "rotated";
	}

	bzero(newmono, strlen(newmono));
	bzero(wrapped, lenwrap * 2);
	free(newmono);
	free(wrapped);

	return msg;
}

static const char *
obscure_msg(const char *old, const char *newval, const struct passwd *pwdp)
{
	int maxlen, oldlen, newlen;
	char *new1, *old1;
	const char *msg;

	oldlen = strlen(old);
	newlen = strlen(newval);

#if 0							/* why not check the password when set for the first time?  --marekm */
	if (old[0] == '\0')
		/* return (1); */
		return NULL;
#endif

	if (newlen < 5)
		return "too short";

	/*
	 * Remaining checks are optional.
	 */
	/* Not for us -- Sean
	 *if (!getdef_bool("OBSCURE_CHECKS_ENAB"))
	 *      return NULL;
	 */
	msg = password_check(old, newval, pwdp);
	if (msg)
		return msg;

	/* The traditional crypt() truncates passwords to 8 chars.  It is
	   possible to circumvent the above checks by choosing an easy
	   8-char password and adding some random characters to it...
	   Example: "password$%^&*123".  So check it again, this time
	   truncated to the maximum length.  Idea from npasswd.  --marekm */

	maxlen = 8;
	if (oldlen <= maxlen && newlen <= maxlen)
		return NULL;

	new1 = (char *) bb_xstrdup(newval);
	old1 = (char *) bb_xstrdup(old);
	if (newlen > maxlen)
		new1[maxlen] = '\0';
	if (oldlen > maxlen)
		old1[maxlen] = '\0';

	msg = password_check(old1, new1, pwdp);

	bzero(new1, newlen);
	bzero(old1, oldlen);
	free(new1);
	free(old1);

	return msg;
}

/*
 * Obscure - see if password is obscure enough.
 *
 *	The programmer is encouraged to add as much complexity to this
 *	routine as desired.  Included are some of my favorite ways to
 *	check passwords.
 */

extern int obscure(const char *old, const char *newval, const struct passwd *pwdp)
{
	const char *msg = obscure_msg(old, newval, pwdp);

	/*  if (msg) { */
	if (msg != NULL) {
		printf("Bad password: %s.\n", msg);
		/* return 0; */
		return 1;
	}
	/* return 1; */
	return 0;
}
