/*
 * This is an implementation of wcwidth() and wcswidth() (defined in
 * IEEE Std 1002.1-2001) for Unicode.
 *
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcwidth.html
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcswidth.html
 *
 * In fixed-width output devices, Latin characters all occupy a single
 * "cell" position of equal width, whereas ideographic CJK characters
 * occupy two such cells. Interoperability between terminal-line
 * applications and (teletype-style) character terminals using the
 * UTF-8 encoding requires agreement on which character should advance
 * the cursor by how many cell positions. No established formal
 * standards exist at present on which Unicode character shall occupy
 * how many cell positions on character terminals. These routines are
 * a first attempt of defining such behavior based on simple rules
 * applied to data provided by the Unicode Consortium.
 *
 * For some graphical characters, the Unicode standard explicitly
 * defines a character-cell width via the definition of the East Asian
 * FullWidth (F), Wide (W), Half-width (H), and Narrow (Na) classes.
 * In all these cases, there is no ambiguity about which width a
 * terminal shall use. For characters in the East Asian Ambiguous (A)
 * class, the width choice depends purely on a preference of backward
 * compatibility with either historic CJK or Western practice.
 * Choosing single-width for these characters is easy to justify as
 * the appropriate long-term solution, as the CJK practice of
 * displaying these characters as double-width comes from historic
 * implementation simplicity (8-bit encoded characters were displayed
 * single-width and 16-bit ones double-width, even for Greek,
 * Cyrillic, etc.) and not any typographic considerations.
 *
 * Much less clear is the choice of width for the Not East Asian
 * (Neutral) class. Existing practice does not dictate a width for any
 * of these characters. It would nevertheless make sense
 * typographically to allocate two character cells to characters such
 * as for instance EM SPACE or VOLUME INTEGRAL, which cannot be
 * represented adequately with a single-width glyph. The following
 * routines at present merely assign a single-cell width to all
 * neutral characters, in the interest of simplicity. This is not
 * entirely satisfactory and should be reconsidered before
 * establishing a formal standard in this area. At the moment, the
 * decision which Not East Asian (Neutral) characters should be
 * represented by double-width glyphs cannot yet be answered by
 * applying a simple rule from the Unicode database content. Setting
 * up a proper standard for the behavior of UTF-8 character terminals
 * will require a careful analysis not only of each Unicode character,
 * but also of each presentation form, something the author of these
 * routines has avoided to do so far.
 *
 * http://www.unicode.org/unicode/reports/tr11/
 *
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct interval {
	uint16_t first;
	uint16_t last;
};

/* auxiliary function for binary search in interval table */
static int in_interval_table(unsigned ucs, const struct interval *table, unsigned max)
{
	unsigned min;
	unsigned mid;

	if (ucs < table[0].first || ucs > table[max].last)
		return 0;

	min = 0;
	while (max >= min) {
		mid = (min + max) / 2;
		if (ucs > table[mid].last)
			min = mid + 1;
		else if (ucs < table[mid].first)
			max = mid - 1;
		else
			return 1;
	}
	return 0;
}

static int in_uint16_table(unsigned ucs, const uint16_t *table, unsigned max)
{
	unsigned min;
	unsigned mid;
	unsigned first, last;

	first = table[0] >> 2;
	last = first + (table[0] & 3);
	if (ucs < first || ucs > last)
		return 0;

	min = 0;
	while (max >= min) {
		mid = (min + max) / 2;
		first = table[mid] >> 2;
		last = first + (table[mid] & 3);
		if (ucs > last)
			min = mid + 1;
		else if (ucs < first)
			max = mid - 1;
		else
			return 1;
	}
	return 0;
}


/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */
static int wcwidth(unsigned ucs)
{
	/* sorted list of non-overlapping intervals of non-spacing characters */
	/* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
	static const struct interval combining[] = {
#define BIG_(a,b) { a, b },
#define PAIR(a,b)
		/* PAIR if < 0x4000 and no more than 4 chars big */
		BIG_(0x0300, 0x036F)
		PAIR(0x0483, 0x0486)
		PAIR(0x0488, 0x0489)
		BIG_(0x0591, 0x05BD)
		PAIR(0x05BF, 0x05BF)
		PAIR(0x05C1, 0x05C2)
		PAIR(0x05C4, 0x05C5)
		PAIR(0x05C7, 0x05C7)
		PAIR(0x0600, 0x0603)
		BIG_(0x0610, 0x0615)
		BIG_(0x064B, 0x065E)
		PAIR(0x0670, 0x0670)
		BIG_(0x06D6, 0x06E4)
		PAIR(0x06E7, 0x06E8)
		PAIR(0x06EA, 0x06ED)
		PAIR(0x070F, 0x070F)
		PAIR(0x0711, 0x0711)
		BIG_(0x0730, 0x074A)
		BIG_(0x07A6, 0x07B0)
		BIG_(0x07EB, 0x07F3)
		PAIR(0x0901, 0x0902)
		PAIR(0x093C, 0x093C)
		BIG_(0x0941, 0x0948)
		PAIR(0x094D, 0x094D)
		PAIR(0x0951, 0x0954)
		PAIR(0x0962, 0x0963)
		PAIR(0x0981, 0x0981)
		PAIR(0x09BC, 0x09BC)
		PAIR(0x09C1, 0x09C4)
		PAIR(0x09CD, 0x09CD)
		PAIR(0x09E2, 0x09E3)
		PAIR(0x0A01, 0x0A02)
		PAIR(0x0A3C, 0x0A3C)
		PAIR(0x0A41, 0x0A42)
		PAIR(0x0A47, 0x0A48)
		PAIR(0x0A4B, 0x0A4D)
		PAIR(0x0A70, 0x0A71)
		PAIR(0x0A81, 0x0A82)
		PAIR(0x0ABC, 0x0ABC)
		BIG_(0x0AC1, 0x0AC5)
		PAIR(0x0AC7, 0x0AC8)
		PAIR(0x0ACD, 0x0ACD)
		PAIR(0x0AE2, 0x0AE3)
		PAIR(0x0B01, 0x0B01)
		PAIR(0x0B3C, 0x0B3C)
		PAIR(0x0B3F, 0x0B3F)
		PAIR(0x0B41, 0x0B43)
		PAIR(0x0B4D, 0x0B4D)
		PAIR(0x0B56, 0x0B56)
		PAIR(0x0B82, 0x0B82)
		PAIR(0x0BC0, 0x0BC0)
		PAIR(0x0BCD, 0x0BCD)
		PAIR(0x0C3E, 0x0C40)
		PAIR(0x0C46, 0x0C48)
		PAIR(0x0C4A, 0x0C4D)
		PAIR(0x0C55, 0x0C56)
		PAIR(0x0CBC, 0x0CBC)
		PAIR(0x0CBF, 0x0CBF)
		PAIR(0x0CC6, 0x0CC6)
		PAIR(0x0CCC, 0x0CCD)
		PAIR(0x0CE2, 0x0CE3)
		PAIR(0x0D41, 0x0D43)
		PAIR(0x0D4D, 0x0D4D)
		PAIR(0x0DCA, 0x0DCA)
		PAIR(0x0DD2, 0x0DD4)
		PAIR(0x0DD6, 0x0DD6)
		PAIR(0x0E31, 0x0E31)
		BIG_(0x0E34, 0x0E3A)
		BIG_(0x0E47, 0x0E4E)
		PAIR(0x0EB1, 0x0EB1)
		BIG_(0x0EB4, 0x0EB9)
		PAIR(0x0EBB, 0x0EBC)
		BIG_(0x0EC8, 0x0ECD)
		PAIR(0x0F18, 0x0F19)
		PAIR(0x0F35, 0x0F35)
		PAIR(0x0F37, 0x0F37)
		PAIR(0x0F39, 0x0F39)
		BIG_(0x0F71, 0x0F7E)
		BIG_(0x0F80, 0x0F84)
		PAIR(0x0F86, 0x0F87)
		PAIR(0x0FC6, 0x0FC6)
		BIG_(0x0F90, 0x0F97)
		BIG_(0x0F99, 0x0FBC)
		PAIR(0x102D, 0x1030)
		PAIR(0x1032, 0x1032)
		PAIR(0x1036, 0x1037)
		PAIR(0x1039, 0x1039)
		PAIR(0x1058, 0x1059)
		BIG_(0x1160, 0x11FF)
		PAIR(0x135F, 0x135F)
		PAIR(0x1712, 0x1714)
		PAIR(0x1732, 0x1734)
		PAIR(0x1752, 0x1753)
		PAIR(0x1772, 0x1773)
		PAIR(0x17B4, 0x17B5)
		BIG_(0x17B7, 0x17BD)
		PAIR(0x17C6, 0x17C6)
		BIG_(0x17C9, 0x17D3)
		PAIR(0x17DD, 0x17DD)
		PAIR(0x180B, 0x180D)
		PAIR(0x18A9, 0x18A9)
		PAIR(0x1920, 0x1922)
		PAIR(0x1927, 0x1928)
		PAIR(0x1932, 0x1932)
		PAIR(0x1939, 0x193B)
		PAIR(0x1A17, 0x1A18)
		PAIR(0x1B00, 0x1B03)
		PAIR(0x1B34, 0x1B34)
		BIG_(0x1B36, 0x1B3A)
		PAIR(0x1B3C, 0x1B3C)
		PAIR(0x1B42, 0x1B42)
		BIG_(0x1B6B, 0x1B73)
		BIG_(0x1DC0, 0x1DCA)
		PAIR(0x1DFE, 0x1DFF)
		BIG_(0x200B, 0x200F)
		BIG_(0x202A, 0x202E)
		PAIR(0x2060, 0x2063)
		BIG_(0x206A, 0x206F)
		BIG_(0x20D0, 0x20EF)
		BIG_(0x302A, 0x302F)
		PAIR(0x3099, 0x309A)
		/* Too big to be packed in PAIRs: */
		{ 0xA806, 0xA806 },
		{ 0xA80B, 0xA80B },
		{ 0xA825, 0xA826 },
		{ 0xFB1E, 0xFB1E },
		{ 0xFE00, 0xFE0F },
		{ 0xFE20, 0xFE23 },
		{ 0xFEFF, 0xFEFF },
		{ 0xFFF9, 0xFFFB }
#undef BIG_
#undef PAIR
	};
	static const uint16_t combining1[] = {
#define BIG_(a,b)
#define PAIR(a,b) (a << 2) | (b-a),
		/* Exact copy-n-paste of the above: */
		BIG_(0x0300, 0x036F)
		PAIR(0x0483, 0x0486)
		PAIR(0x0488, 0x0489)
		BIG_(0x0591, 0x05BD)
		PAIR(0x05BF, 0x05BF)
		PAIR(0x05C1, 0x05C2)
		PAIR(0x05C4, 0x05C5)
		PAIR(0x05C7, 0x05C7)
		PAIR(0x0600, 0x0603)
		BIG_(0x0610, 0x0615)
		BIG_(0x064B, 0x065E)
		PAIR(0x0670, 0x0670)
		BIG_(0x06D6, 0x06E4)
		PAIR(0x06E7, 0x06E8)
		PAIR(0x06EA, 0x06ED)
		PAIR(0x070F, 0x070F)
		PAIR(0x0711, 0x0711)
		BIG_(0x0730, 0x074A)
		BIG_(0x07A6, 0x07B0)
		BIG_(0x07EB, 0x07F3)
		PAIR(0x0901, 0x0902)
		PAIR(0x093C, 0x093C)
		BIG_(0x0941, 0x0948)
		PAIR(0x094D, 0x094D)
		PAIR(0x0951, 0x0954)
		PAIR(0x0962, 0x0963)
		PAIR(0x0981, 0x0981)
		PAIR(0x09BC, 0x09BC)
		PAIR(0x09C1, 0x09C4)
		PAIR(0x09CD, 0x09CD)
		PAIR(0x09E2, 0x09E3)
		PAIR(0x0A01, 0x0A02)
		PAIR(0x0A3C, 0x0A3C)
		PAIR(0x0A41, 0x0A42)
		PAIR(0x0A47, 0x0A48)
		PAIR(0x0A4B, 0x0A4D)
		PAIR(0x0A70, 0x0A71)
		PAIR(0x0A81, 0x0A82)
		PAIR(0x0ABC, 0x0ABC)
		BIG_(0x0AC1, 0x0AC5)
		PAIR(0x0AC7, 0x0AC8)
		PAIR(0x0ACD, 0x0ACD)
		PAIR(0x0AE2, 0x0AE3)
		PAIR(0x0B01, 0x0B01)
		PAIR(0x0B3C, 0x0B3C)
		PAIR(0x0B3F, 0x0B3F)
		PAIR(0x0B41, 0x0B43)
		PAIR(0x0B4D, 0x0B4D)
		PAIR(0x0B56, 0x0B56)
		PAIR(0x0B82, 0x0B82)
		PAIR(0x0BC0, 0x0BC0)
		PAIR(0x0BCD, 0x0BCD)
		PAIR(0x0C3E, 0x0C40)
		PAIR(0x0C46, 0x0C48)
		PAIR(0x0C4A, 0x0C4D)
		PAIR(0x0C55, 0x0C56)
		PAIR(0x0CBC, 0x0CBC)
		PAIR(0x0CBF, 0x0CBF)
		PAIR(0x0CC6, 0x0CC6)
		PAIR(0x0CCC, 0x0CCD)
		PAIR(0x0CE2, 0x0CE3)
		PAIR(0x0D41, 0x0D43)
		PAIR(0x0D4D, 0x0D4D)
		PAIR(0x0DCA, 0x0DCA)
		PAIR(0x0DD2, 0x0DD4)
		PAIR(0x0DD6, 0x0DD6)
		PAIR(0x0E31, 0x0E31)
		BIG_(0x0E34, 0x0E3A)
		BIG_(0x0E47, 0x0E4E)
		PAIR(0x0EB1, 0x0EB1)
		BIG_(0x0EB4, 0x0EB9)
		PAIR(0x0EBB, 0x0EBC)
		BIG_(0x0EC8, 0x0ECD)
		PAIR(0x0F18, 0x0F19)
		PAIR(0x0F35, 0x0F35)
		PAIR(0x0F37, 0x0F37)
		PAIR(0x0F39, 0x0F39)
		BIG_(0x0F71, 0x0F7E)
		BIG_(0x0F80, 0x0F84)
		PAIR(0x0F86, 0x0F87)
		PAIR(0x0FC6, 0x0FC6)
		BIG_(0x0F90, 0x0F97)
		BIG_(0x0F99, 0x0FBC)
		PAIR(0x102D, 0x1030)
		PAIR(0x1032, 0x1032)
		PAIR(0x1036, 0x1037)
		PAIR(0x1039, 0x1039)
		PAIR(0x1058, 0x1059)
		BIG_(0x1160, 0x11FF)
		PAIR(0x135F, 0x135F)
		PAIR(0x1712, 0x1714)
		PAIR(0x1732, 0x1734)
		PAIR(0x1752, 0x1753)
		PAIR(0x1772, 0x1773)
		PAIR(0x17B4, 0x17B5)
		BIG_(0x17B7, 0x17BD)
		PAIR(0x17C6, 0x17C6)
		BIG_(0x17C9, 0x17D3)
		PAIR(0x17DD, 0x17DD)
		PAIR(0x180B, 0x180D)
		PAIR(0x18A9, 0x18A9)
		PAIR(0x1920, 0x1922)
		PAIR(0x1927, 0x1928)
		PAIR(0x1932, 0x1932)
		PAIR(0x1939, 0x193B)
		PAIR(0x1A17, 0x1A18)
		PAIR(0x1B00, 0x1B03)
		PAIR(0x1B34, 0x1B34)
		BIG_(0x1B36, 0x1B3A)
		PAIR(0x1B3C, 0x1B3C)
		PAIR(0x1B42, 0x1B42)
		BIG_(0x1B6B, 0x1B73)
		BIG_(0x1DC0, 0x1DCA)
		PAIR(0x1DFE, 0x1DFF)
		BIG_(0x200B, 0x200F)
		BIG_(0x202A, 0x202E)
		PAIR(0x2060, 0x2063)
		BIG_(0x206A, 0x206F)
		BIG_(0x20D0, 0x20EF)
		BIG_(0x302A, 0x302F)
		PAIR(0x3099, 0x309A)
#undef BIG_
#undef PAIR
	};
	struct CHECK {
#define BIG_(a,b) char big##a[b-a <= 3 ? -1 : 1];
#define PAIR(a,b) char pair##a[b-a > 3 ? -1 : 1];
		/* Copy-n-paste it here again to verify correctness */
#undef BIG_
#undef PAIR
	};
	static const struct interval combining0x10000[] = {
		{ 0x0A01, 0x0A03 }, { 0x0A05, 0x0A06 }, { 0x0A0C, 0x0A0F },
		{ 0x0A38, 0x0A3A }, { 0x0A3F, 0x0A3F }, { 0xD167, 0xD169 },
		{ 0xD173, 0xD182 }, { 0xD185, 0xD18B }, { 0xD1AA, 0xD1AD },
		{ 0xD242, 0xD244 }
	};

	if (ucs == 0)
		return 0;
	/* test for 8-bit control characters (00-1f, 80-9f, 7f) */
	if ((ucs & ~0x80) < 0x20 || ucs == 0x7f)
		return -1;
	if (ucs < 0x0300) /* optimization */
		return 1;

	/* binary search in table of non-spacing characters */
	if (in_interval_table(ucs, combining, ARRAY_SIZE(combining) - 1))
		return 0;
	if (in_uint16_table(ucs, combining1, ARRAY_SIZE(combining1) - 1))
		return 0;

	if (ucs < 0x1100) /* optimization */
		return 1;

	/* binary search in table of non-spacing characters, cont. */
	if (in_interval_table(ucs ^ 0x10000, combining0x10000, ARRAY_SIZE(combining0x10000) - 1))
		return 0;
	if (ucs == 0xE0001
	 || (ucs >= 0xE0020 && ucs <= 0xE007F)
	 || (ucs >= 0xE0100 && ucs <= 0xE01EF)
	) {
		return 0;
	}

	/* if we arrive here, ucs is not a combining or C0/C1 control character */

	return 1 +
		(  (/*ucs >= 0x1100 &&*/ ucs <= 0x115f) /* Hangul Jamo init. consonants */
		|| ucs == 0x2329
		|| ucs == 0x232a
		|| (ucs >= 0x2e80 && ucs <= 0xa4cf && ucs != 0x303f) /* CJK ... Yi */
		|| (ucs >= 0xac00 && ucs <= 0xd7a3) /* Hangul Syllables */
		|| (ucs >= 0xf900 && ucs <= 0xfaff) /* CJK Compatibility Ideographs */
		|| (ucs >= 0xfe10 && ucs <= 0xfe19) /* Vertical forms */
		|| (ucs >= 0xfe30 && ucs <= 0xfe6f) /* CJK Compatibility Forms */
		|| (ucs >= 0xff00 && ucs <= 0xff60) /* Fullwidth Forms */
		|| (ucs >= 0xffe0 && ucs <= 0xffe6)
		|| (ucs >= 0x20000 && ucs <= 0x2fffd)
		|| (ucs >= 0x30000 && ucs <= 0x3fffd)
		);
}
