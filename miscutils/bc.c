/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 * Copyright (c) 2018 Gavin D. Howard and contributors.
 */
//config:config BC
//config:	bool "bc (45 kb; 49 kb when combined with dc)"
//config:	default y
//config:	help
//config:	bc is a command-line, arbitrary-precision calculator with a
//config:	Turing-complete language. See the GNU bc manual
//config:	(https://www.gnu.org/software/bc/manual/bc.html) and bc spec
//config:	(http://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html).
//config:
//config:	This bc has five differences to the GNU bc:
//config:	  1) The period (.) is a shortcut for "last", as in the BSD bc.
//config:	  2) Arrays are copied before being passed as arguments to
//config:	     functions. This behavior is required by the bc spec.
//config:	  3) Arrays can be passed to the builtin "length" function to get
//config:	     the number of elements in the array. This prints "1":
//config:		a[0] = 0; length(a[])
//config:	  4) The precedence of the boolean "not" operator (!) is equal to
//config:	     that of the unary minus (-) negation operator. This still
//config:	     allows POSIX-compliant scripts to work while somewhat
//config:	     preserving expected behavior (versus C) and making parsing
//config:	     easier.
//config:	  5) "read()" accepts expressions, not only numeric literals.
//config:
//config:	Options:
//config:	  -i  --interactive  force interactive mode
//config:	  -q  --quiet        don't print version and copyright
//config:	  -s  --standard     error if any non-POSIX extensions are used
//config:	  -w  --warn         warn if any non-POSIX extensions are used
//config:	  -l  --mathlib      use predefined math routines:
//config:		s(expr) sine in radians
//config:		c(expr) cosine in radians
//config:		a(expr) arctangent, returning radians
//config:		l(expr) natural log
//config:		e(expr) raises e to the power of expr
//config:		j(n, x) Bessel function of integer order n of x
//config:
//config:config DC
//config:	bool "dc (38 kb; 49 kb when combined with bc)"
//config:	default y
//config:	help
//config:	dc is a reverse-polish notation command-line calculator which
//config:	supports unlimited precision arithmetic. See the FreeBSD man page
//config:	(https://www.unix.com/man-page/FreeBSD/1/dc/) and GNU dc manual
//config:	(https://www.gnu.org/software/bc/manual/dc-1.05/html_mono/dc.html).
//config:
//config:	This dc has a few differences from the two above:
//config:	  1) When printing a byte stream (command "P"), this dc follows what
//config:	     the FreeBSD dc does.
//config:	  2) Implements the GNU extensions for divmod ("~") and
//config:	     modular exponentiation ("|").
//config:	  3) Implements all FreeBSD extensions, except for "J" and "M".
//config:	  4) Like the FreeBSD dc, this dc supports extended registers.
//config:	     However, they are implemented differently. When it encounters
//config:	     whitespace where a register should be, it skips the whitespace.
//config:	     If the character following is not a lowercase letter, an error
//config:	     is issued. Otherwise, the register name is parsed by the
//config:	     following regex:
//config:		[a-z][a-z0-9_]*
//config:	     This generally means that register names will be surrounded by
//config:	     whitespace. Examples:
//config:		l idx s temp L index S temp2 < do_thing
//config:	     Also note that, like the FreeBSD dc, extended registers are not
//config:	     allowed unless the "-x" option is given.
//config:
//config:config FEATURE_DC_SMALL
//config:	bool "Minimal dc implementation (4.2 kb), not using bc code base"
//config:	depends on DC && !BC
//config:	default n
//config:
//config:config FEATURE_DC_LIBM
//config:	bool "Enable power and exp functions (requires libm)"
//config:	default y
//config:	depends on FEATURE_DC_SMALL
//config:	help
//config:	Enable power and exp functions.
//config:	NOTE: This will require libm to be present for linking.
//config:
//config:config FEATURE_BC_SIGNALS
//config:	bool "Interactive mode (+4kb)"
//config:	default y
//config:	depends on (BC || DC) && !FEATURE_DC_SMALL
//config:	help
//config:	Enable interactive mode: when started on a tty,
//config:	^C interrupts execution and returns to command line,
//config:	errors also return to command line instead of exiting,
//config:	line editing with history is available.
//config:
//config:	With this option off, input can still be taken from tty,
//config:	but all errors are fatal, ^C is fatal,
//config:	tty is treated exactly the same as any other
//config:	standard input (IOW: no line editing).
//config:
//config:config FEATURE_BC_LONG_OPTIONS
//config:	bool "Enable bc/dc long options"
//config:	default y
//config:	depends on (BC || DC) && !FEATURE_DC_SMALL
//config:	help
//config:	Enable long options for bc and dc.

//applet:IF_BC(APPLET(bc, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_DC(APPLET(dc, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_BC) += bc.o
//kbuild:lib-$(CONFIG_DC) += bc.o

//See www.gnu.org/software/bc/manual/bc.html
//usage:#define bc_trivial_usage
//usage:       "[-sqlw] FILE..."
//usage:
//usage:#define bc_full_usage "\n"
//usage:     "\nArbitrary precision calculator"
//usage:     "\n"
///////:     "\n	-i	Interactive" - has no effect for now
//usage:     "\n	-q	Quiet"
//usage:     "\n	-l	Load standard math library"
//usage:     "\n	-s	Be POSIX compatible"
//usage:     "\n	-w	Warn if extensions are used"
///////:     "\n	-v	Version"
//usage:     "\n"
//usage:     "\n$BC_LINE_LENGTH changes output width"
//usage:
//usage:#define bc_example_usage
//usage:       "3 + 4.129\n"
//usage:       "1903 - 2893\n"
//usage:       "-129 * 213.28935\n"
//usage:       "12 / -1932\n"
//usage:       "12 % 12\n"
//usage:       "34 ^ 189\n"
//usage:       "scale = 13\n"
//usage:       "ibase = 2\n"
//usage:       "obase = A\n"
//usage:
//usage:#define dc_trivial_usage
//usage:       IF_NOT_FEATURE_DC_SMALL("[-x] ")"[-eSCRIPT]... [-fFILE]... [FILE]..."
//usage:
//usage:#define dc_full_usage "\n"
//usage:     "\nTiny RPN calculator. Operations:"
//usage:     "\n+, -, *, /, %, ~, ^," IF_NOT_FEATURE_DC_SMALL(" |,")
//usage:     "\np - print top of the stack without popping"
//usage:     "\nf - print entire stack"
//usage:     "\nk - pop the value and set the precision"
//usage:     "\ni - pop the value and set input radix"
//usage:     "\no - pop the value and set output radix"
//usage:     "\nExamples: dc -e'2 2 + p' -> 4, dc -e'8 8 * 2 2 + / p' -> 16"
//usage:
//usage:#define dc_example_usage
//usage:       "$ dc -e'2 2 + p'\n"
//usage:       "4\n"
//usage:       "$ dc -e'8 8 \\* 2 2 + / p'\n"
//usage:       "16\n"
//usage:       "$ dc -e'0 1 & p'\n"
//usage:       "0\n"
//usage:       "$ dc -e'0 1 | p'\n"
//usage:       "1\n"
//usage:       "$ echo '72 9 / 8 * p' | dc\n"
//usage:       "64\n"

#include "libbb.h"
#include "common_bufsiz.h"

#if ENABLE_FEATURE_DC_SMALL
# include "dc.c"
#else

#define DEBUG_LEXER   0
#define DEBUG_COMPILE 0
#define DEBUG_EXEC    0
// This can be left enabled for production as well:
#define SANITY_CHECKS 1

#if DEBUG_LEXER
static uint8_t lex_indent;
#define dbg_lex(...) \
	do { \
		fprintf(stderr, "%*s", lex_indent, ""); \
		bb_error_msg(__VA_ARGS__); \
	} while (0)
#define dbg_lex_enter(...) \
	do { \
		dbg_lex(__VA_ARGS__); \
		lex_indent++; \
	} while (0)
#define dbg_lex_done(...) \
	do { \
		lex_indent--; \
		dbg_lex(__VA_ARGS__); \
	} while (0)
#else
# define dbg_lex(...)       ((void)0)
# define dbg_lex_enter(...) ((void)0)
# define dbg_lex_done(...)  ((void)0)
#endif

#if DEBUG_COMPILE
# define dbg_compile(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg_compile(...) ((void)0)
#endif

#if DEBUG_EXEC
# define dbg_exec(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg_exec(...) ((void)0)
#endif

typedef enum BcStatus {
	BC_STATUS_SUCCESS = 0,
	BC_STATUS_FAILURE = 1,
	BC_STATUS_PARSE_EMPTY_EXP = 2, // bc_parse_expr_empty_ok() uses this
} BcStatus;

#define BC_VEC_INVALID_IDX ((size_t) -1)
#define BC_VEC_START_CAP (1 << 5)

typedef void (*BcVecFree)(void *) FAST_FUNC;

typedef struct BcVec {
	char *v;
	size_t len;
	size_t cap;
	size_t size;
	BcVecFree dtor;
} BcVec;

typedef signed char BcDig;

typedef struct BcNum {
	BcDig *restrict num;
	size_t rdx;
	size_t len;
	size_t cap;
	bool neg;
} BcNum;

#define BC_NUM_MAX_IBASE        ((unsigned long) 16)
// larger value might speed up BIGNUM calculations a bit:
#define BC_NUM_DEF_SIZE         (16)
#define BC_NUM_PRINT_WIDTH      (69)

#define BC_NUM_KARATSUBA_LEN    (32)

typedef enum BcInst {
#if ENABLE_BC
	BC_INST_INC_PRE,
	BC_INST_DEC_PRE,
	BC_INST_INC_POST,
	BC_INST_DEC_POST,
#endif
	XC_INST_NEG,            // order

	XC_INST_REL_EQ,         // should
	XC_INST_REL_LE,         // match
	XC_INST_REL_GE,         // LEX
	XC_INST_REL_NE,         // constants
	XC_INST_REL_LT,         // for
	XC_INST_REL_GT,         // these

	XC_INST_POWER,          // operations
	XC_INST_MULTIPLY,       // |
	XC_INST_DIVIDE,         // |
	XC_INST_MODULUS,        // |
	XC_INST_PLUS,           // |
	XC_INST_MINUS,          // |

	XC_INST_BOOL_NOT,       // |
	XC_INST_BOOL_OR,        // |
	XC_INST_BOOL_AND,       // |
#if ENABLE_BC
	BC_INST_ASSIGN_POWER,   // |
	BC_INST_ASSIGN_MULTIPLY,// |
	BC_INST_ASSIGN_DIVIDE,  // |
	BC_INST_ASSIGN_MODULUS, // |
	BC_INST_ASSIGN_PLUS,    // |
	BC_INST_ASSIGN_MINUS,   // |
#endif
	XC_INST_ASSIGN,         // V

	XC_INST_NUM,
	XC_INST_VAR,
	XC_INST_ARRAY_ELEM,
	XC_INST_ARRAY,
	XC_INST_SCALE_FUNC,

	XC_INST_IBASE,       // order of these constans should match other enums
	XC_INST_OBASE,       // order of these constans should match other enums
	XC_INST_SCALE,       // order of these constans should match other enums
	IF_BC(BC_INST_LAST,) // order of these constans should match other enums
	XC_INST_LENGTH,
	XC_INST_READ,
	XC_INST_SQRT,

	XC_INST_PRINT,
	XC_INST_PRINT_POP,
	XC_INST_STR,
	XC_INST_PRINT_STR,

#if ENABLE_BC
	BC_INST_HALT,
	BC_INST_JUMP,
	BC_INST_JUMP_ZERO,

	BC_INST_CALL,
	BC_INST_RET0,
#endif
	XC_INST_RET,

	XC_INST_POP,
#if ENABLE_DC
	DC_INST_POP_EXEC,

	DC_INST_MODEXP,
	DC_INST_DIVMOD,

	DC_INST_EXECUTE,
	DC_INST_EXEC_COND,

	DC_INST_ASCIIFY,
	DC_INST_PRINT_STREAM,

	DC_INST_PRINT_STACK,
	DC_INST_CLEAR_STACK,
	DC_INST_STACK_LEN,
	DC_INST_DUPLICATE,
	DC_INST_SWAP,

	DC_INST_LOAD,
	DC_INST_PUSH_VAR,
	DC_INST_PUSH_TO_VAR,

	DC_INST_QUIT,
	DC_INST_NQUIT,

	DC_INST_INVALID = -1,
#endif
} BcInst;

typedef struct BcId {
	char *name;
	size_t idx;
} BcId;

typedef struct BcFunc {
	BcVec code;
	IF_BC(BcVec labels;)
	IF_BC(BcVec autos;)
	IF_BC(BcVec strs;)
	IF_BC(BcVec consts;)
	IF_BC(size_t nparams;)
} BcFunc;

typedef enum BcResultType {
	BC_RESULT_TEMP,

	BC_RESULT_VAR,
	BC_RESULT_ARRAY_ELEM,
	BC_RESULT_ARRAY,

	BC_RESULT_STR,

	//code uses "inst - XC_INST_IBASE + BC_RESULT_IBASE" construct,
	BC_RESULT_IBASE,       // relative order should match for: XC_INST_IBASE
	BC_RESULT_OBASE,       // relative order should match for: XC_INST_OBASE
	BC_RESULT_SCALE,       // relative order should match for: XC_INST_SCALE
	IF_BC(BC_RESULT_LAST,) // relative order should match for: BC_INST_LAST
	BC_RESULT_CONSTANT,
	BC_RESULT_ONE,
} BcResultType;

typedef union BcResultData {
	BcNum n;
	BcVec v;
	BcId id;
} BcResultData;

typedef struct BcResult {
	BcResultType t;
	BcResultData d;
} BcResult;

typedef struct BcInstPtr {
	size_t func;
	size_t inst_idx;
	IF_BC(size_t results_len_before_call;)
} BcInstPtr;

typedef enum BcLexType {
	XC_LEX_EOF,
	XC_LEX_INVALID,

	XC_LEX_NLINE,
	XC_LEX_WHITESPACE,
	XC_LEX_STR,
	XC_LEX_NAME,
	XC_LEX_NUMBER,

	XC_LEX_1st_op,
	XC_LEX_NEG = XC_LEX_1st_op,     // order

	XC_LEX_OP_REL_EQ,               // should
	XC_LEX_OP_REL_LE,               // match
	XC_LEX_OP_REL_GE,               // INST
	XC_LEX_OP_REL_NE,               // constants
	XC_LEX_OP_REL_LT,               // for
	XC_LEX_OP_REL_GT,               // these

	XC_LEX_OP_POWER,                // operations
	XC_LEX_OP_MULTIPLY,             // |
	XC_LEX_OP_DIVIDE,               // |
	XC_LEX_OP_MODULUS,              // |
	XC_LEX_OP_PLUS,                 // |
	XC_LEX_OP_MINUS,                // |
	XC_LEX_OP_last = XC_LEX_OP_MINUS,
#if ENABLE_BC
	BC_LEX_OP_BOOL_NOT,             // |
	BC_LEX_OP_BOOL_OR,              // |
	BC_LEX_OP_BOOL_AND,             // |

	BC_LEX_OP_ASSIGN_POWER,         // |
	BC_LEX_OP_ASSIGN_MULTIPLY,      // |
	BC_LEX_OP_ASSIGN_DIVIDE,        // |
	BC_LEX_OP_ASSIGN_MODULUS,       // |
	BC_LEX_OP_ASSIGN_PLUS,          // |
	BC_LEX_OP_ASSIGN_MINUS,         // |

	BC_LEX_OP_ASSIGN,               // V

	BC_LEX_OP_INC,
	BC_LEX_OP_DEC,

	BC_LEX_LPAREN, // () are 0x28 and 0x29
	BC_LEX_RPAREN, // must be LPAREN+1: code uses (c - '(' + BC_LEX_LPAREN)

	BC_LEX_LBRACKET, // [] are 0x5B and 5D
	BC_LEX_COMMA,
	BC_LEX_RBRACKET, // must be LBRACKET+2: code uses (c - '[' + BC_LEX_LBRACKET)

	BC_LEX_LBRACE, // {} are 0x7B and 0x7D
	BC_LEX_SCOLON,
	BC_LEX_RBRACE, // must be LBRACE+2: code uses (c - '{' + BC_LEX_LBRACE)

	BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_AUTO = BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_BREAK,
	BC_LEX_KEY_CONTINUE,
	BC_LEX_KEY_DEFINE,
	BC_LEX_KEY_ELSE,
	BC_LEX_KEY_FOR,
	BC_LEX_KEY_HALT,
	// code uses "type - BC_LEX_KEY_IBASE + XC_INST_IBASE" construct,
	BC_LEX_KEY_IBASE,       // relative order should match for: XC_INST_IBASE
	BC_LEX_KEY_OBASE,       // relative order should match for: XC_INST_OBASE
	BC_LEX_KEY_IF,
	IF_BC(BC_LEX_KEY_LAST,) // relative order should match for: BC_INST_LAST
	BC_LEX_KEY_LENGTH,
	BC_LEX_KEY_LIMITS,
	BC_LEX_KEY_PRINT,
	BC_LEX_KEY_QUIT,
	BC_LEX_KEY_READ,
	BC_LEX_KEY_RETURN,
	BC_LEX_KEY_SCALE,
	BC_LEX_KEY_SQRT,
	BC_LEX_KEY_WHILE,
#endif // ENABLE_BC

#if ENABLE_DC
	DC_LEX_OP_BOOL_NOT = XC_LEX_OP_last + 1,
	DC_LEX_OP_ASSIGN,

	DC_LEX_LPAREN,
	DC_LEX_SCOLON,
	DC_LEX_READ,
	DC_LEX_IBASE,
	DC_LEX_SCALE,
	DC_LEX_OBASE,
	DC_LEX_LENGTH,
	DC_LEX_PRINT,
	DC_LEX_QUIT,
	DC_LEX_SQRT,
	DC_LEX_LBRACE,

	DC_LEX_EQ_NO_REG,
	DC_LEX_OP_MODEXP,
	DC_LEX_OP_DIVMOD,

	DC_LEX_COLON,
	DC_LEX_ELSE,
	DC_LEX_EXECUTE,
	DC_LEX_PRINT_STACK,
	DC_LEX_CLEAR_STACK,
	DC_LEX_STACK_LEVEL,
	DC_LEX_DUPLICATE,
	DC_LEX_SWAP,
	DC_LEX_POP,

	DC_LEX_ASCIIFY,
	DC_LEX_PRINT_STREAM,

	// code uses "t - DC_LEX_STORE_IBASE + XC_INST_IBASE" construct,
	DC_LEX_STORE_IBASE,  // relative order should match for: XC_INST_IBASE
	DC_LEX_STORE_OBASE,  // relative order should match for: XC_INST_OBASE
	DC_LEX_STORE_SCALE,  // relative order should match for: XC_INST_SCALE
	DC_LEX_LOAD,
	DC_LEX_LOAD_POP,
	DC_LEX_STORE_PUSH,
	DC_LEX_PRINT_POP,
	DC_LEX_NQUIT,
	DC_LEX_SCALE_FACTOR,
#endif
} BcLexType;
// must match order of BC_LEX_KEY_foo etc above
#if ENABLE_BC
struct BcLexKeyword {
	char name8[8];
};
#define BC_LEX_KW_ENTRY(a, b) \
	{ .name8 = a /*, .posix = b */ }
static const struct BcLexKeyword bc_lex_kws[20] = {
	BC_LEX_KW_ENTRY("auto"    , 1), // 0
	BC_LEX_KW_ENTRY("break"   , 1), // 1
	BC_LEX_KW_ENTRY("continue", 0), // 2 note: this one has no terminating NUL
	BC_LEX_KW_ENTRY("define"  , 1), // 3
	BC_LEX_KW_ENTRY("else"    , 0), // 4
	BC_LEX_KW_ENTRY("for"     , 1), // 5
	BC_LEX_KW_ENTRY("halt"    , 0), // 6
	BC_LEX_KW_ENTRY("ibase"   , 1), // 7
	BC_LEX_KW_ENTRY("obase"   , 1), // 8
	BC_LEX_KW_ENTRY("if"      , 1), // 9
	BC_LEX_KW_ENTRY("last"    , 0), // 10
	BC_LEX_KW_ENTRY("length"  , 1), // 11
	BC_LEX_KW_ENTRY("limits"  , 0), // 12
	BC_LEX_KW_ENTRY("print"   , 0), // 13
	BC_LEX_KW_ENTRY("quit"    , 1), // 14
	BC_LEX_KW_ENTRY("read"    , 0), // 15
	BC_LEX_KW_ENTRY("return"  , 1), // 16
	BC_LEX_KW_ENTRY("scale"   , 1), // 17
	BC_LEX_KW_ENTRY("sqrt"    , 1), // 18
	BC_LEX_KW_ENTRY("while"   , 1), // 19
};
#undef BC_LEX_KW_ENTRY
#define STRING_else  (bc_lex_kws[4].name8)
#define STRING_for   (bc_lex_kws[5].name8)
#define STRING_if    (bc_lex_kws[9].name8)
#define STRING_while (bc_lex_kws[19].name8)
enum {
	POSIX_KWORD_MASK = 0
		| (1 << 0)  // 0
		| (1 << 1)  // 1
		| (0 << 2)  // 2
		| (1 << 3)  // 3
		| (0 << 4)  // 4
		| (1 << 5)  // 5
		| (0 << 6)  // 6
		| (1 << 7)  // 7
		| (1 << 8)  // 8
		| (1 << 9)  // 9
		| (0 << 10) // 10
		| (1 << 11) // 11
		| (0 << 12) // 12
		| (0 << 13) // 13
		| (1 << 14) // 14
		| (0 << 15) // 15
		| (1 << 16) // 16
		| (1 << 17) // 17
		| (1 << 18) // 18
		| (1 << 19) // 19
};
#define bc_lex_kws_POSIX(i) ((1 << (i)) & POSIX_KWORD_MASK)

// This is a bit array that corresponds to token types. An entry is
// true if the token is valid in an expression, false otherwise.
// Used to figure out when expr parsing should stop *without error message*
// - 0 element indicates this condition. 1 means "this token is to be eaten
// as part of the expression", it can then still be determined to be invalid
// by later processing.
enum {
#define EXBITS(a,b,c,d,e,f,g,h) \
	((uint64_t)((a << 0)+(b << 1)+(c << 2)+(d << 3)+(e << 4)+(f << 5)+(g << 6)+(h << 7)))
	BC_PARSE_EXPRS_BITS = 0              // corresponding BC_LEX_xyz:
	+ (EXBITS(0,0,0,0,0,1,1,1) << (0*8)) //  0: EOF    INVAL  NL     WS     STR    NAME   NUM    -
	+ (EXBITS(1,1,1,1,1,1,1,1) << (1*8)) //  8: ==     <=     >=     !=     <      >      ^      *
	+ (EXBITS(1,1,1,1,1,1,1,1) << (2*8)) // 16: /      %      +      -      !      ||     &&     ^=
	+ (EXBITS(1,1,1,1,1,1,1,1) << (3*8)) // 24: *=     /=     %=     +=     -=     =      ++     --
	+ (EXBITS(1,1,0,0,0,0,0,0) << (4*8)) // 32: (      )      [      ,      ]      {      ;      }
	+ (EXBITS(0,0,0,0,0,0,0,1) << (5*8)) // 40: auto   break  cont   define else   for    halt   ibase
	+ (EXBITS(1,0,1,1,0,0,0,1) << (6*8)) // 48: obase  if     last   length limits print  quit   read
	+ (EXBITS(0,1,1,0,0,0,0,0) << (7*8)) // 56: return scale  sqrt   while
#undef EXBITS
};
static ALWAYS_INLINE long bc_parse_exprs(unsigned i)
{
#if ULONG_MAX > 0xffffffff
	// 64-bit version (will not work correctly for 32-bit longs!)
	return BC_PARSE_EXPRS_BITS & (1UL << i);
#else
	// 32-bit version
	unsigned long m = (uint32_t)BC_PARSE_EXPRS_BITS;
	if (i >= 32) {
		m = (uint32_t)(BC_PARSE_EXPRS_BITS >> 32);
		i &= 31;
	}
	return m & (1UL << i);
#endif
}

// This is an array of data for operators that correspond to
// [XC_LEX_1st_op...] token types.
static const uint8_t bc_parse_ops[] = {
#define OP(p,l) ((int)(l) * 0x10 + (p))
	OP(1, false), // neg
	OP(6, true ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true ), // == <= >= != < >
	OP(2, false), // pow
	OP(3, true ), OP( 3, true  ), OP( 3, true  ), // mul div mod
	OP(4, true ), OP( 4, true  ), // + -
	OP(1, false), // not
	OP(7, true ), OP( 7, true  ), // or and
	OP(5, false), OP( 5, false ), OP( 5, false ), OP( 5, false ), OP( 5, false ), // ^= *= /= %= +=
	OP(5, false), OP( 5, false ), // -= =
	OP(0, false), OP( 0, false ), // inc dec
#undef OP
};
#define bc_parse_op_PREC(i) (bc_parse_ops[i] & 0x0f)
#define bc_parse_op_LEFT(i) (bc_parse_ops[i] & 0x10)
#endif // ENABLE_BC

#if ENABLE_DC
static const //BcLexType - should be this type
uint8_t
dc_char_to_LEX[] = {
	/* %&'( */
	XC_LEX_OP_MODULUS, XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_LPAREN,
	/* )*+, */
	XC_LEX_INVALID, XC_LEX_OP_MULTIPLY, XC_LEX_OP_PLUS, XC_LEX_INVALID,
	/* -./ */
	XC_LEX_OP_MINUS, XC_LEX_INVALID, XC_LEX_OP_DIVIDE,
	/* 0123456789 */
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID,
	/* :;<=>?@ */
	DC_LEX_COLON, DC_LEX_SCOLON, XC_LEX_OP_REL_GT, XC_LEX_OP_REL_EQ,
	XC_LEX_OP_REL_LT, DC_LEX_READ, XC_LEX_INVALID,
	/* ABCDEFGH */
	XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_EQ_NO_REG, XC_LEX_INVALID,
	/* IJKLMNOP */
	DC_LEX_IBASE, XC_LEX_INVALID, DC_LEX_SCALE, DC_LEX_LOAD_POP,
	XC_LEX_INVALID, DC_LEX_OP_BOOL_NOT, DC_LEX_OBASE, DC_LEX_PRINT_STREAM,
	/* QRSTUVWXY */
	DC_LEX_NQUIT, DC_LEX_POP, DC_LEX_STORE_PUSH, XC_LEX_INVALID, XC_LEX_INVALID,
	XC_LEX_INVALID, XC_LEX_INVALID, DC_LEX_SCALE_FACTOR, XC_LEX_INVALID,
	/* Z[\] */
	DC_LEX_LENGTH, XC_LEX_INVALID, XC_LEX_INVALID, XC_LEX_INVALID,
	/* ^_` */
	XC_LEX_OP_POWER, XC_LEX_NEG, XC_LEX_INVALID,
	/* abcdefgh */
	DC_LEX_ASCIIFY, XC_LEX_INVALID, DC_LEX_CLEAR_STACK, DC_LEX_DUPLICATE,
	DC_LEX_ELSE, DC_LEX_PRINT_STACK, XC_LEX_INVALID, XC_LEX_INVALID,
	/* ijklmnop */
	DC_LEX_STORE_IBASE, XC_LEX_INVALID, DC_LEX_STORE_SCALE, DC_LEX_LOAD,
	XC_LEX_INVALID, DC_LEX_PRINT_POP, DC_LEX_STORE_OBASE, DC_LEX_PRINT,
	/* qrstuvwx */
	DC_LEX_QUIT, DC_LEX_SWAP, DC_LEX_OP_ASSIGN, XC_LEX_INVALID,
	XC_LEX_INVALID, DC_LEX_SQRT, XC_LEX_INVALID, DC_LEX_EXECUTE,
	/* yz */
	XC_LEX_INVALID, DC_LEX_STACK_LEVEL,
	/* {|}~ */
	DC_LEX_LBRACE, DC_LEX_OP_MODEXP, XC_LEX_INVALID, DC_LEX_OP_DIVMOD,
};
static const //BcInst - should be this type. Using signed narrow type since DC_INST_INVALID is -1
int8_t
dc_LEX_to_INST[] = { // starts at XC_LEX_OP_POWER       // corresponding XC/DC_LEX_xyz:
	XC_INST_POWER,       XC_INST_MULTIPLY,          // OP_POWER     OP_MULTIPLY
	XC_INST_DIVIDE,      XC_INST_MODULUS,           // OP_DIVIDE    OP_MODULUS
	XC_INST_PLUS,        XC_INST_MINUS,             // OP_PLUS      OP_MINUS
	XC_INST_BOOL_NOT,                               // DC_LEX_OP_BOOL_NOT
	DC_INST_INVALID,                                // DC_LEX_OP_ASSIGN
	XC_INST_REL_GT,                                 // DC_LEX_LPAREN
	DC_INST_INVALID,                                // DC_LEX_SCOLON
	DC_INST_INVALID,                                // DC_LEX_READ
	XC_INST_IBASE,                                  // DC_LEX_IBASE
	XC_INST_SCALE,                                  // DC_LEX_SCALE
	XC_INST_OBASE,                                  // DC_LEX_OBASE
	XC_INST_LENGTH,                                 // DC_LEX_LENGTH
	XC_INST_PRINT,                                  // DC_LEX_PRINT
	DC_INST_QUIT,                                   // DC_LEX_QUIT
	XC_INST_SQRT,                                   // DC_LEX_SQRT
	XC_INST_REL_GE,                                 // DC_LEX_LBRACE
	XC_INST_REL_EQ,                                 // DC_LEX_EQ_NO_REG
	DC_INST_MODEXP,      DC_INST_DIVMOD,            // OP_MODEXP    OP_DIVMOD
	DC_INST_INVALID,     DC_INST_INVALID,           // COLON        ELSE
	DC_INST_EXECUTE,                                // EXECUTE
	DC_INST_PRINT_STACK, DC_INST_CLEAR_STACK,       // PRINT_STACK  CLEAR_STACK
	DC_INST_STACK_LEN,   DC_INST_DUPLICATE,         // STACK_LEVEL  DUPLICATE
	DC_INST_SWAP,        XC_INST_POP,               // SWAP         POP
	DC_INST_ASCIIFY,     DC_INST_PRINT_STREAM,      // ASCIIFY      PRINT_STREAM
	DC_INST_INVALID,     DC_INST_INVALID,           // STORE_IBASE  STORE_OBASE
	DC_INST_INVALID,     DC_INST_INVALID,           // STORE_SCALE  LOAD
	DC_INST_INVALID,     DC_INST_INVALID,           // LOAD_POP     STORE_PUSH
	XC_INST_PRINT,       DC_INST_NQUIT,             // PRINT_POP    NQUIT
	XC_INST_SCALE_FUNC,                             // SCALE_FACTOR
	// DC_INST_INVALID in this table either means that corresponding LEX
	// is not possible for dc, or that it does not compile one-to-one
	// to a single INST.
};
#endif // ENABLE_DC

typedef struct BcLex {
	const char *buf;
	size_t i;
	size_t line;
	size_t len;
	bool   newline;
	smallint lex;      // was BcLexType
	smallint lex_last; // was BcLexType
	BcVec  lex_buf;
} BcLex;

#define BC_PARSE_STREND         (0xff)

#if ENABLE_BC
# define BC_PARSE_REL           (1 << 0)
# define BC_PARSE_PRINT         (1 << 1)
# define BC_PARSE_ARRAY         (1 << 2)
# define BC_PARSE_NOCALL        (1 << 3)
#endif

typedef struct BcParse {
	BcLex l;

	IF_BC(BcVec exits;)
	IF_BC(BcVec conds;)
	IF_BC(BcVec ops;)

	BcFunc *func;
	size_t fidx;

	IF_BC(size_t in_funcdef;)
} BcParse;

typedef struct BcProgram {
	size_t len;
	size_t nchars;

	size_t scale;
	size_t ib_t;
	size_t ob_t;

	BcVec results;
	BcVec exestack;

	BcVec fns;
	IF_BC(BcVec fn_map;)

	BcVec vars;
	BcVec var_map;

	BcVec arrs;
	BcVec arr_map;

	IF_DC(BcVec strs;)
	IF_DC(BcVec consts;)

	const char *file;

	BcNum zero;
	IF_BC(BcNum one;)
	IF_BC(BcNum last;)
} BcProgram;

#define BC_PROG_MAIN (0)
#define BC_PROG_READ (1)
#if ENABLE_DC
#define BC_PROG_REQ_FUNCS (2)
#endif

#define BC_PROG_STR(n) (!(n)->num && !(n)->cap)
#define BC_PROG_NUM(r, n) \
	((r)->t != BC_RESULT_ARRAY && (r)->t != BC_RESULT_STR && !BC_PROG_STR(n))

#define BC_FLAG_W (1 << 0)
#define BC_FLAG_V (1 << 1)
#define BC_FLAG_S (1 << 2)
#define BC_FLAG_Q (1 << 3)
#define BC_FLAG_L (1 << 4)
#define BC_FLAG_I (1 << 5)
#define DC_FLAG_X (1 << 6)

#define BC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BC_MIN(a, b) ((a) < (b) ? (a) : (b))

#define BC_MAX_OBASE    ((unsigned) 999)
#define BC_MAX_DIM      ((unsigned) INT_MAX)
#define BC_MAX_SCALE    ((unsigned) UINT_MAX)
#define BC_MAX_STRING   ((unsigned) UINT_MAX - 1)
#define BC_MAX_NUM      BC_MAX_STRING
// Unused apart from "limits" message. Just show a "biggish number" there.
//#define BC_MAX_NAME     BC_MAX_STRING
//#define BC_MAX_EXP      ((unsigned long) LONG_MAX)
//#define BC_MAX_VARS     ((unsigned long) SIZE_MAX - 1)
#define BC_MAX_NAME_STR "999999999"
#define BC_MAX_EXP_STR  "999999999"
#define BC_MAX_VARS_STR "999999999"

#define BC_MAX_OBASE_STR "999"

#if INT_MAX == 2147483647
# define BC_MAX_DIM_STR "2147483647"
#elif INT_MAX == 9223372036854775807
# define BC_MAX_DIM_STR "9223372036854775807"
#else
# error Strange INT_MAX
#endif

#if UINT_MAX == 4294967295
# define BC_MAX_SCALE_STR  "4294967295"
# define BC_MAX_STRING_STR "4294967294"
#elif UINT_MAX == 18446744073709551615
# define BC_MAX_SCALE_STR  "18446744073709551615"
# define BC_MAX_STRING_STR "18446744073709551614"
#else
# error Strange UINT_MAX
#endif
#define BC_MAX_NUM_STR BC_MAX_STRING_STR

struct globals {
	IF_FEATURE_BC_SIGNALS(smallint ttyin;)
	IF_FEATURE_CLEAN_UP(smallint exiting;)

	BcParse prs;
	BcProgram prog;

	// For error messages. Can be set to current parsed line,
	// or [TODO] to current executing line (can be before last parsed one)
	unsigned err_line;

	BcVec files;
	BcVec input_buffer;
	FILE *input_fp;

	char *env_args;

#if ENABLE_FEATURE_EDITING
	line_input_t *line_input_state;
#endif
} FIX_ALIASING;
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)
#define FREE_G() do { \
	FREE_PTR_TO_GLOBALS(); \
} while (0)
#define G_posix (ENABLE_BC && (option_mask32 & BC_FLAG_S))
#define G_warn  (ENABLE_BC && (option_mask32 & BC_FLAG_W))
#define G_exreg (ENABLE_DC && (option_mask32 & DC_FLAG_X))
#if ENABLE_FEATURE_BC_SIGNALS
# define G_interrupt bb_got_signal
# define G_ttyin     G.ttyin
#else
# define G_interrupt 0
# define G_ttyin     0
#endif
#if ENABLE_FEATURE_CLEAN_UP
# define G_exiting G.exiting
#else
# define G_exiting 0
#endif
#define IS_BC (ENABLE_BC && (!ENABLE_DC || applet_name[0] == 'b'))
#define IS_DC (ENABLE_DC && (!ENABLE_BC || applet_name[0] != 'b'))

// In configurations where errors abort instead of propagating error
// return code up the call chain, functions returning BC_STATUS
// actually don't return anything, they always succeed and return "void".
// A macro wrapper is provided, which makes this statement work:
//  s = zbc_func(...)
// and makes it visible to the compiler that s is always zero,
// allowing compiler to optimize dead code after the statement.
//
// To make code more readable, each such function has a "z"
// ("always returning zero") prefix, i.e. zbc_foo or zdc_foo.
//
#if ENABLE_FEATURE_BC_SIGNALS || ENABLE_FEATURE_CLEAN_UP
# define ERRORS_ARE_FATAL 0
# define ERRORFUNC        /*nothing*/
# define IF_ERROR_RETURN_POSSIBLE(a)  a
# define BC_STATUS        BcStatus
# define RETURN_STATUS(v) return (v)
# define COMMA_SUCCESS    /*nothing*/
#else
# define ERRORS_ARE_FATAL 1
# define ERRORFUNC        NORETURN
# define IF_ERROR_RETURN_POSSIBLE(a)  /*nothing*/
# define BC_STATUS        void
# define RETURN_STATUS(v) do { ((void)(v)); return; } while (0)
# define COMMA_SUCCESS    ,BC_STATUS_SUCCESS
#endif

//
// Utility routines
//

static void fflush_and_check(void)
{
	fflush_all();
	if (ferror(stdout) || ferror(stderr))
		bb_perror_msg_and_die("output error");
}

#if ENABLE_FEATURE_CLEAN_UP
#define QUIT_OR_RETURN_TO_MAIN \
do { \
	IF_FEATURE_BC_SIGNALS(G_ttyin = 0;) /* do not loop in main loop anymore */ \
	G_exiting = 1; \
	return BC_STATUS_FAILURE; \
} while (0)
#else
#define QUIT_OR_RETURN_TO_MAIN quit()
#endif

static void quit(void) NORETURN;
static void quit(void)
{
	if (ferror(stdin))
		bb_perror_msg_and_die("input error");
	fflush_and_check();
	dbg_exec("quit(): exiting with exitcode SUCCESS");
	exit(0);
}

static void bc_verror_msg(const char *fmt, va_list p)
{
	const char *sv = sv; // for compiler
	if (G.prog.file) {
		sv = applet_name;
		applet_name = xasprintf("%s: %s:%u", applet_name, G.prog.file, G.err_line);
	}
	bb_verror_msg(fmt, p, NULL);
	if (G.prog.file) {
		free((char*)applet_name);
		applet_name = sv;
	}
}

static NOINLINE ERRORFUNC int bc_error_fmt(const char *fmt, ...)
{
	va_list p;

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	if (ENABLE_FEATURE_CLEAN_UP || G_ttyin)
		IF_ERROR_RETURN_POSSIBLE(return BC_STATUS_FAILURE);
	exit(1);
}

#if ENABLE_BC
static NOINLINE BC_STATUS zbc_posix_error_fmt(const char *fmt, ...)
{
	va_list p;

	// Are non-POSIX constructs totally ok?
	if (!(option_mask32 & (BC_FLAG_S|BC_FLAG_W)))
		RETURN_STATUS(BC_STATUS_SUCCESS); // yes

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	// Do we treat non-POSIX constructs as errors?
	if (!(option_mask32 & BC_FLAG_S))
		RETURN_STATUS(BC_STATUS_SUCCESS); // no, it's a warning

	if (ENABLE_FEATURE_CLEAN_UP || G_ttyin)
		RETURN_STATUS(BC_STATUS_FAILURE);
	exit(1);
}
#define zbc_posix_error_fmt(...) (zbc_posix_error_fmt(__VA_ARGS__) COMMA_SUCCESS)
#endif

// We use error functions with "return bc_error(FMT[, PARAMS])" idiom.
// This idiom begs for tail-call optimization, but for it to work,
// function must not have caller-cleaned parameters on stack.
// Unfortunately, vararg function API does exactly that on most arches.
// Thus, use these shims for the cases when we have no vararg PARAMS:
static ERRORFUNC int bc_error(const char *msg)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt("%s", msg);
}
static ERRORFUNC int bc_error_bad_character(char c)
{
	if (!c)
		IF_ERROR_RETURN_POSSIBLE(return) bc_error("NUL character");
	IF_ERROR_RETURN_POSSIBLE(return) bc_error_fmt("bad character '%c'", c);
}
static ERRORFUNC int bc_error_bad_expression(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("bad expression");
}
static ERRORFUNC int bc_error_bad_token(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("bad token");
}
static ERRORFUNC int bc_error_stack_has_too_few_elements(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("stack has too few elements");
}
static ERRORFUNC int bc_error_variable_is_wrong_type(void)
{
	IF_ERROR_RETURN_POSSIBLE(return) bc_error("variable is wrong type");
}
#if ENABLE_BC
static BC_STATUS zbc_POSIX_requires(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("POSIX requires %s", msg));
}
#define zbc_POSIX_requires(...) (zbc_POSIX_requires(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%s%s", "POSIX does not allow ", msg));
}
#define zbc_POSIX_does_not_allow(...) (zbc_POSIX_does_not_allow(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow_bool_ops_this_is_bad(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%s%s %s", "POSIX does not allow ", "boolean operators; the following is bad:", msg));
}
#define zbc_POSIX_does_not_allow_bool_ops_this_is_bad(...) (zbc_POSIX_does_not_allow_bool_ops_this_is_bad(__VA_ARGS__) COMMA_SUCCESS)
static BC_STATUS zbc_POSIX_does_not_allow_empty_X_expression_in_for(const char *msg)
{
	RETURN_STATUS(zbc_posix_error_fmt("%san empty %s expression in a for loop", "POSIX does not allow ", msg));
}
#define zbc_POSIX_does_not_allow_empty_X_expression_in_for(...) (zbc_POSIX_does_not_allow_empty_X_expression_in_for(__VA_ARGS__) COMMA_SUCCESS)
#endif

static void bc_vec_grow(BcVec *v, size_t n)
{
	size_t cap = v->cap * 2;
	while (cap < v->len + n) cap *= 2;
	v->v = xrealloc(v->v, v->size * cap);
	v->cap = cap;
}

static void bc_vec_init(BcVec *v, size_t esize, BcVecFree dtor)
{
	v->size = esize;
	v->cap = BC_VEC_START_CAP;
	v->len = 0;
	v->dtor = dtor;
	v->v = xmalloc(esize * BC_VEC_START_CAP);
}

static void bc_char_vec_init(BcVec *v)
{
	bc_vec_init(v, sizeof(char), NULL);
}

static void bc_vec_expand(BcVec *v, size_t req)
{
	if (v->cap < req) {
		v->v = xrealloc(v->v, v->size * req);
		v->cap = req;
	}
}

static void bc_vec_pop(BcVec *v)
{
	v->len--;
	if (v->dtor)
		v->dtor(v->v + (v->size * v->len));
}

static void bc_vec_npop(BcVec *v, size_t n)
{
	if (!v->dtor)
		v->len -= n;
	else {
		size_t len = v->len - n;
		while (v->len > len) v->dtor(v->v + (v->size * --v->len));
	}
}

static void bc_vec_pop_all(BcVec *v)
{
	bc_vec_npop(v, v->len);
}

static size_t bc_vec_push(BcVec *v, const void *data)
{
	size_t len = v->len;
	if (len >= v->cap) bc_vec_grow(v, 1);
	memmove(v->v + (v->size * len), data, v->size);
	v->len++;
	return len;
}

// G.prog.results often needs "pop old operand, push result" idiom.
// Can do this without a few extra ops
static size_t bc_result_pop_and_push(const void *data)
{
	BcVec *v = &G.prog.results;
	char *last;
	size_t len = v->len - 1;

	last = v->v + (v->size * len);
	if (v->dtor)
		v->dtor(last);
	memmove(last, data, v->size);
	return len;
}

static size_t bc_vec_pushByte(BcVec *v, char data)
{
	return bc_vec_push(v, &data);
}

static size_t bc_vec_pushZeroByte(BcVec *v)
{
	//return bc_vec_pushByte(v, '\0');
	// better:
	return bc_vec_push(v, &const_int_0);
}

static void bc_vec_pushAt(BcVec *v, const void *data, size_t idx)
{
	if (idx == v->len)
		bc_vec_push(v, data);
	else {
		char *ptr;

		if (v->len == v->cap) bc_vec_grow(v, 1);

		ptr = v->v + v->size * idx;

		memmove(ptr + v->size, ptr, v->size * (v->len++ - idx));
		memmove(ptr, data, v->size);
	}
}

static void bc_vec_string(BcVec *v, size_t len, const char *str)
{
	bc_vec_pop_all(v);
	bc_vec_expand(v, len + 1);
	memcpy(v->v, str, len);
	v->len = len;

	bc_vec_pushZeroByte(v);
}

#if ENABLE_FEATURE_BC_SIGNALS && ENABLE_FEATURE_EDITING
static void bc_vec_concat(BcVec *v, const char *str)
{
	size_t len, slen;

	if (v->len == 0) bc_vec_pushZeroByte(v);

	slen = strlen(str);
	len = v->len + slen;

	if (v->cap < len) bc_vec_grow(v, slen);
	strcpy(v->v + v->len - 1, str);

	v->len = len;
}
#endif

static void *bc_vec_item(const BcVec *v, size_t idx)
{
	return v->v + v->size * idx;
}

static void *bc_vec_item_rev(const BcVec *v, size_t idx)
{
	return v->v + v->size * (v->len - idx - 1);
}

static void *bc_vec_top(const BcVec *v)
{
	return v->v + v->size * (v->len - 1);
}

static FAST_FUNC void bc_vec_free(void *vec)
{
	BcVec *v = (BcVec *) vec;
	bc_vec_pop_all(v);
	free(v->v);
}

static BcFunc* bc_program_func(size_t idx)
{
	return bc_vec_item(&G.prog.fns, idx);
}
// BC_PROG_MAIN is zeroth element, so:
#define bc_program_func_BC_PROG_MAIN() ((BcFunc*)(G.prog.fns.v))

#if ENABLE_BC
static BcFunc* bc_program_current_func(void)
{
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);
	BcFunc *func = bc_program_func(ip->func);
	return func;
}
#endif

static char** bc_program_str(size_t idx)
{
#if ENABLE_BC
	if (IS_BC) {
		BcFunc *func = bc_program_current_func();
		return bc_vec_item(&func->strs, idx);
	}
#endif
	IF_DC(return bc_vec_item(&G.prog.strs, idx);)
}

static char** bc_program_const(size_t idx)
{
#if ENABLE_BC
	if (IS_BC) {
		BcFunc *func = bc_program_current_func();
		return bc_vec_item(&func->consts, idx);
	}
#endif
	IF_DC(return bc_vec_item(&G.prog.consts, idx);)
}

static int bc_id_cmp(const void *e1, const void *e2)
{
	return strcmp(((const BcId *) e1)->name, ((const BcId *) e2)->name);
}

static FAST_FUNC void bc_id_free(void *id)
{
	free(((BcId *) id)->name);
}

static size_t bc_map_find_ge(const BcVec *v, const void *ptr)
{
	size_t low = 0, high = v->len;

	while (low < high) {
		size_t mid = (low + high) / 2;
		BcId *id = bc_vec_item(v, mid);
		int result = bc_id_cmp(ptr, id);

		if (result == 0)
			return mid;
		if (result < 0)
			high = mid;
		else
			low = mid + 1;
	}

	return low;
}

static int bc_map_insert(BcVec *v, const void *ptr, size_t *i)
{
	size_t n = *i = bc_map_find_ge(v, ptr);

	if (n == v->len)
		bc_vec_push(v, ptr);
	else if (!bc_id_cmp(ptr, bc_vec_item(v, n)))
		return 0; // "was not inserted"
	else
		bc_vec_pushAt(v, ptr, n);
	return 1; // "was inserted"
}

#if ENABLE_BC
static size_t bc_map_find_exact(const BcVec *v, const void *ptr)
{
	size_t i = bc_map_find_ge(v, ptr);
	if (i >= v->len) return BC_VEC_INVALID_IDX;
	return bc_id_cmp(ptr, bc_vec_item(v, i)) ? BC_VEC_INVALID_IDX : i;
}
#endif

static void bc_num_setToZero(BcNum *n, size_t scale)
{
	n->len = 0;
	n->neg = false;
	n->rdx = scale;
}

static void bc_num_zero(BcNum *n)
{
	bc_num_setToZero(n, 0);
}

static void bc_num_one(BcNum *n)
{
	bc_num_setToZero(n, 0);
	n->len = 1;
	n->num[0] = 1;
}

// Note: this also sets BcNum to zero
static void bc_num_init(BcNum *n, size_t req)
{
	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
	//memset(n, 0, sizeof(BcNum)); - cleared by assignments below
	n->num = xmalloc(req);
	n->cap = req;
	n->rdx = 0;
	n->len = 0;
	n->neg = false;
}

static void bc_num_init_DEF_SIZE(BcNum *n)
{
	bc_num_init(n, BC_NUM_DEF_SIZE);
}

static void bc_num_expand(BcNum *n, size_t req)
{
	req = req >= BC_NUM_DEF_SIZE ? req : BC_NUM_DEF_SIZE;
	if (req > n->cap) {
		n->num = xrealloc(n->num, req);
		n->cap = req;
	}
}

static FAST_FUNC void bc_num_free(void *num)
{
	free(((BcNum *) num)->num);
}

static void bc_num_copy(BcNum *d, BcNum *s)
{
	if (d != s) {
		bc_num_expand(d, s->cap);
		d->len = s->len;
		d->neg = s->neg;
		d->rdx = s->rdx;
		memcpy(d->num, s->num, sizeof(BcDig) * d->len);
	}
}

static BC_STATUS zbc_num_ulong_abs(BcNum *n, unsigned long *result_p)
{
	size_t i;
	unsigned long result;

	result = 0;
	i = n->len;
	while (i > n->rdx) {
		unsigned long prev = result;
		result = result * 10 + n->num[--i];
		// Even overflowed N*10 can still satisfy N*10>=N. For example,
		//    0x1ff00000 * 10 is 0x13f600000,
		// or 0x3f600000 truncated to 32 bits. Which is larger.
		// However, (N*10)/8 < N check is always correct.
		if ((result / 8) < prev)
			RETURN_STATUS(bc_error("overflow"));
	}
	*result_p = result;

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_num_ulong_abs(...) (zbc_num_ulong_abs(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_ulong(BcNum *n, unsigned long *result_p)
{
	if (n->neg) RETURN_STATUS(bc_error("negative number"));

	RETURN_STATUS(zbc_num_ulong_abs(n, result_p));
}
#define zbc_num_ulong(...) (zbc_num_ulong(__VA_ARGS__) COMMA_SUCCESS)

#if ULONG_MAX == 0xffffffffUL // 10 digits: 4294967295
# define ULONG_NUM_BUFSIZE (10 > BC_NUM_DEF_SIZE ? 10 : BC_NUM_DEF_SIZE)
#elif ULONG_MAX == 0xffffffffffffffffULL // 20 digits: 18446744073709551615
# define ULONG_NUM_BUFSIZE (20 > BC_NUM_DEF_SIZE ? 20 : BC_NUM_DEF_SIZE)
#endif
// minimum BC_NUM_DEF_SIZE, so that bc_num_expand() in bc_num_ulong2num()
// would not hit realloc() code path - not good if num[] is not malloced

static void bc_num_ulong2num(BcNum *n, unsigned long val)
{
	BcDig *ptr;

	bc_num_zero(n);

	if (val == 0) return;

	bc_num_expand(n, ULONG_NUM_BUFSIZE);

	ptr = n->num;
	for (;;) {
		n->len++;
		*ptr++ = val % 10;
		val /= 10;
		if (val == 0) break;
	}
}

static void bc_num_subArrays(BcDig *restrict a, BcDig *restrict b, size_t len)
{
	size_t i, j;
	for (i = 0; i < len; ++i) {
		a[i] -= b[i];
		for (j = i; a[j] < 0;) {
			a[j++] += 10;
			a[j] -= 1;
		}
	}
}

static ssize_t bc_num_compare(BcDig *restrict a, BcDig *restrict b, size_t len)
{
	size_t i = len;
	for (;;) {
		int c;
		if (i == 0)
			return 0;
		i--;
		c = a[i] - b[i];
		if (c != 0) {
			i++;
			if (c < 0)
				return -i;
			return i;
		}
	}
}

#define BC_NUM_NEG(n, neg)      ((((ssize_t)(n)) ^ -((ssize_t)(neg))) + (neg))
#define BC_NUM_ONE(n)           ((n)->len == 1 && (n)->rdx == 0 && (n)->num[0] == 1)
#define BC_NUM_INT(n)           ((n)->len - (n)->rdx)
//#define BC_NUM_AREQ(a, b)       (BC_MAX((a)->rdx, (b)->rdx) + BC_MAX(BC_NUM_INT(a), BC_NUM_INT(b)) + 1)
static /*ALWAYS_INLINE*/ size_t BC_NUM_AREQ(BcNum *a, BcNum *b)
{
	return BC_MAX(a->rdx, b->rdx) + BC_MAX(BC_NUM_INT(a), BC_NUM_INT(b)) + 1;
}
//#define BC_NUM_MREQ(a, b, scale) (BC_NUM_INT(a) + BC_NUM_INT(b) + BC_MAX((scale), (a)->rdx + (b)->rdx) + 1)
static /*ALWAYS_INLINE*/ size_t BC_NUM_MREQ(BcNum *a, BcNum *b, size_t scale)
{
	return BC_NUM_INT(a) + BC_NUM_INT(b) + BC_MAX(scale, a->rdx + b->rdx) + 1;
}

static ssize_t bc_num_cmp(BcNum *a, BcNum *b)
{
	size_t i, min, a_int, b_int, diff;
	BcDig *max_num, *min_num;
	bool a_max, neg;
	ssize_t cmp;

	if (a == b) return 0;
	if (a->len == 0) return BC_NUM_NEG(!!b->len, !b->neg);
	if (b->len == 0) return BC_NUM_NEG(1, a->neg);

	if (a->neg != b->neg) // signs of a and b differ
		// +a,-b = a>b = 1 or -a,+b = a<b = -1
		return (int)b->neg - (int)a->neg;
	neg = a->neg; // 1 if both negative, 0 if both positive

	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);
	a_int -= b_int;

	if (a_int != 0) return (ssize_t) a_int;

	a_max = (a->rdx > b->rdx);
	if (a_max) {
		min = b->rdx;
		diff = a->rdx - b->rdx;
		max_num = a->num + diff;
		min_num = b->num;
		// neg = (a_max == neg); - NOP (maps 1->1 and 0->0)
	} else {
		min = a->rdx;
		diff = b->rdx - a->rdx;
		max_num = b->num + diff;
		min_num = a->num;
		neg = !neg; // same as "neg = (a_max == neg)"
	}

	cmp = bc_num_compare(max_num, min_num, b_int + min);
	if (cmp != 0) return BC_NUM_NEG(cmp, neg);

	for (max_num -= diff, i = diff - 1; i < diff; --i) {
		if (max_num[i]) return BC_NUM_NEG(1, neg);
	}

	return 0;
}

static void bc_num_truncate(BcNum *n, size_t places)
{
	if (places == 0) return;

	n->rdx -= places;

	if (n->len != 0) {
		n->len -= places;
		memmove(n->num, n->num + places, n->len * sizeof(BcDig));
	}
}

static void bc_num_extend(BcNum *n, size_t places)
{
	size_t len = n->len + places;

	if (places != 0) {
		if (n->cap < len) bc_num_expand(n, len);

		memmove(n->num + places, n->num, sizeof(BcDig) * n->len);
		memset(n->num, 0, sizeof(BcDig) * places);

		n->len += places;
		n->rdx += places;
	}
}

static void bc_num_clean(BcNum *n)
{
	while (n->len > 0 && n->num[n->len - 1] == 0) --n->len;
	if (n->len == 0)
		n->neg = false;
	else if (n->len < n->rdx)
		n->len = n->rdx;
}

static void bc_num_retireMul(BcNum *n, size_t scale, bool neg1, bool neg2)
{
	if (n->rdx < scale)
		bc_num_extend(n, scale - n->rdx);
	else
		bc_num_truncate(n, n->rdx - scale);

	bc_num_clean(n);
	if (n->len != 0) n->neg = !neg1 != !neg2;
}

static void bc_num_split(BcNum *restrict n, size_t idx, BcNum *restrict a,
                         BcNum *restrict b)
{
	if (idx < n->len) {
		b->len = n->len - idx;
		a->len = idx;
		a->rdx = b->rdx = 0;

		memcpy(b->num, n->num + idx, b->len * sizeof(BcDig));
		memcpy(a->num, n->num, idx * sizeof(BcDig));
	} else {
		bc_num_zero(b);
		bc_num_copy(a, n);
	}

	bc_num_clean(a);
	bc_num_clean(b);
}

static BC_STATUS zbc_num_shift(BcNum *n, size_t places)
{
	if (places == 0 || n->len == 0) RETURN_STATUS(BC_STATUS_SUCCESS);

	// This check makes sense only if size_t is (much) larger than BC_MAX_NUM.
	if (SIZE_MAX > (BC_MAX_NUM | 0xff)) {
		if (places + n->len > BC_MAX_NUM)
			RETURN_STATUS(bc_error("number too long: must be [1,"BC_MAX_NUM_STR"]"));
	}

	if (n->rdx >= places)
		n->rdx -= places;
	else {
		bc_num_extend(n, places - n->rdx);
		n->rdx = 0;
	}

	bc_num_clean(n);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_num_shift(...) (zbc_num_shift(__VA_ARGS__) COMMA_SUCCESS)

typedef BC_STATUS (*BcNumBinaryOp)(BcNum *, BcNum *, BcNum *, size_t) FAST_FUNC;

static BC_STATUS zbc_num_binary(BcNum *a, BcNum *b, BcNum *c, size_t scale,
                              BcNumBinaryOp op, size_t req)
{
	BcStatus s;
	BcNum num2, *ptr_a, *ptr_b;
	bool init = false;

	if (c == a) {
		ptr_a = &num2;
		memcpy(ptr_a, c, sizeof(BcNum));
		init = true;
	} else
		ptr_a = a;

	if (c == b) {
		ptr_b = &num2;
		if (c != a) {
			memcpy(ptr_b, c, sizeof(BcNum));
			init = true;
		}
	} else
		ptr_b = b;

	if (init)
		bc_num_init(c, req);
	else
		bc_num_expand(c, req);

	s = BC_STATUS_SUCCESS;
	IF_ERROR_RETURN_POSSIBLE(s =) op(ptr_a, ptr_b, c, scale);

	if (init) bc_num_free(&num2);

	RETURN_STATUS(s);
}
#define zbc_num_binary(...) (zbc_num_binary(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_a(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_s(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_p(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_m(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_d(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);
static FAST_FUNC BC_STATUS zbc_num_rem(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale);

static FAST_FUNC BC_STATUS zbc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? zbc_num_a : zbc_num_s;
	(void) scale;
	RETURN_STATUS(zbc_num_binary(a, b, c, false, op, BC_NUM_AREQ(a, b)));
}

static FAST_FUNC BC_STATUS zbc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? zbc_num_s : zbc_num_a;
	(void) scale;
	RETURN_STATUS(zbc_num_binary(a, b, c, true, op, BC_NUM_AREQ(a, b)));
}

static FAST_FUNC BC_STATUS zbc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_m, req));
}

static FAST_FUNC BC_STATUS zbc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_d, req));
}

static FAST_FUNC BC_STATUS zbc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_rem, req));
}

static FAST_FUNC BC_STATUS zbc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	RETURN_STATUS(zbc_num_binary(a, b, c, scale, zbc_num_p, a->len * b->len + 1));
}

static const BcNumBinaryOp zbc_program_ops[] = {
	zbc_num_pow, zbc_num_mul, zbc_num_div, zbc_num_mod, zbc_num_add, zbc_num_sub,
};
#define zbc_num_add(...) (zbc_num_add(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_sub(...) (zbc_num_sub(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_mul(...) (zbc_num_mul(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_div(...) (zbc_num_div(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_mod(...) (zbc_num_mod(__VA_ARGS__) COMMA_SUCCESS)
#define zbc_num_pow(...) (zbc_num_pow(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_inv(BcNum *a, BcNum *b, size_t scale)
{
	BcNum one;
	BcDig num[2];

	one.cap = 2;
	one.num = num;
	bc_num_one(&one);

	RETURN_STATUS(zbc_num_div(&one, a, b, scale));
}
#define zbc_num_inv(...) (zbc_num_inv(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_a(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
{
	BcDig *ptr, *ptr_a, *ptr_b, *ptr_c;
	size_t i, max, min_rdx, min_int, diff, a_int, b_int;
	unsigned carry;

	// Because this function doesn't need to use scale (per the bc spec),
	// I am hijacking it to say whether it's doing an add or a subtract.

	if (a->len == 0) {
		bc_num_copy(c, b);
		if (sub && c->len) c->neg = !c->neg;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (b->len == 0) {
		bc_num_copy(c, a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	c->neg = a->neg;
	c->rdx = BC_MAX(a->rdx, b->rdx);
	min_rdx = BC_MIN(a->rdx, b->rdx);
	c->len = 0;

	if (a->rdx > b->rdx) {
		diff = a->rdx - b->rdx;
		ptr = a->num;
		ptr_a = a->num + diff;
		ptr_b = b->num;
	} else {
		diff = b->rdx - a->rdx;
		ptr = b->num;
		ptr_a = a->num;
		ptr_b = b->num + diff;
	}

	ptr_c = c->num;
	for (i = 0; i < diff; ++i, ++c->len)
		ptr_c[i] = ptr[i];

	ptr_c += diff;
	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);

	if (a_int > b_int) {
		min_int = b_int;
		max = a_int;
		ptr = ptr_a;
	} else {
		min_int = a_int;
		max = b_int;
		ptr = ptr_b;
	}

	carry = 0;
	for (i = 0; i < min_rdx + min_int; ++i) {
		unsigned in = (unsigned)ptr_a[i] + (unsigned)ptr_b[i] + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}
	for (; i < max + min_rdx; ++i) {
		unsigned in = (unsigned)ptr[i] + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}
	c->len += i;

	if (carry != 0) c->num[c->len++] = (BcDig) carry;

	RETURN_STATUS(BC_STATUS_SUCCESS); // can't make void, see zbc_num_binary()
}

static FAST_FUNC BC_STATUS zbc_num_s(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
{
	ssize_t cmp;
	BcNum *minuend, *subtrahend;
	size_t start;
	bool aneg, bneg, neg;

	// Because this function doesn't need to use scale (per the bc spec),
	// I am hijacking it to say whether it's doing an add or a subtract.

	if (a->len == 0) {
		bc_num_copy(c, b);
		if (sub && c->len) c->neg = !c->neg;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (b->len == 0) {
		bc_num_copy(c, a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	aneg = a->neg;
	bneg = b->neg;
	a->neg = b->neg = false;

	cmp = bc_num_cmp(a, b);

	a->neg = aneg;
	b->neg = bneg;

	if (cmp == 0) {
		bc_num_setToZero(c, BC_MAX(a->rdx, b->rdx));
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (cmp > 0) {
		neg = a->neg;
		minuend = a;
		subtrahend = b;
	} else {
		neg = b->neg;
		if (sub) neg = !neg;
		minuend = b;
		subtrahend = a;
	}

	bc_num_copy(c, minuend);
	c->neg = neg;

	if (c->rdx < subtrahend->rdx) {
		bc_num_extend(c, subtrahend->rdx - c->rdx);
		start = 0;
	} else
		start = c->rdx - subtrahend->rdx;

	bc_num_subArrays(c->num + start, subtrahend->num, subtrahend->len);

	bc_num_clean(c);

	RETURN_STATUS(BC_STATUS_SUCCESS); // can't make void, see zbc_num_binary()
}

static FAST_FUNC BC_STATUS zbc_num_k(BcNum *restrict a, BcNum *restrict b,
                         BcNum *restrict c)
#define zbc_num_k(...) (zbc_num_k(__VA_ARGS__) COMMA_SUCCESS)
{
	BcStatus s;
	size_t max = BC_MAX(a->len, b->len), max2 = (max + 1) / 2;
	BcNum l1, h1, l2, h2, m2, m1, z0, z1, z2, temp;
	bool aone;

	if (a->len == 0 || b->len == 0) {
		bc_num_zero(c);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	aone = BC_NUM_ONE(a);
	if (aone || BC_NUM_ONE(b)) {
		bc_num_copy(c, aone ? b : a);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	if (a->len + b->len < BC_NUM_KARATSUBA_LEN
	 || a->len < BC_NUM_KARATSUBA_LEN
	 || b->len < BC_NUM_KARATSUBA_LEN
	) {
		size_t i, j, len;

		bc_num_expand(c, a->len + b->len + 1);

		memset(c->num, 0, sizeof(BcDig) * c->cap);
		c->len = len = 0;

		for (i = 0; i < b->len; ++i) {
			unsigned carry = 0;
			for (j = 0; j < a->len; ++j) {
				unsigned in = c->num[i + j];
				in += (unsigned)a->num[j] * (unsigned)b->num[i] + carry;
				// note: compilers prefer _unsigned_ div/const
				carry = in / 10;
				c->num[i + j] = (BcDig)(in % 10);
			}

			c->num[i + j] += (BcDig) carry;
			len = BC_MAX(len, i + j + !!carry);

#if ENABLE_FEATURE_BC_SIGNALS
			// a=2^1000000
			// a*a <- without check below, this will not be interruptible
			if (G_interrupt) return BC_STATUS_FAILURE;
#endif
		}

		c->len = len;

		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_num_init(&l1, max);
	bc_num_init(&h1, max);
	bc_num_init(&l2, max);
	bc_num_init(&h2, max);
	bc_num_init(&m1, max);
	bc_num_init(&m2, max);
	bc_num_init(&z0, max);
	bc_num_init(&z1, max);
	bc_num_init(&z2, max);
	bc_num_init(&temp, max + max);

	bc_num_split(a, max2, &l1, &h1);
	bc_num_split(b, max2, &l2, &h2);

	s = zbc_num_add(&h1, &l1, &m1, 0);
	if (s) goto err;
	s = zbc_num_add(&h2, &l2, &m2, 0);
	if (s) goto err;

	s = zbc_num_k(&h1, &h2, &z0);
	if (s) goto err;
	s = zbc_num_k(&m1, &m2, &z1);
	if (s) goto err;
	s = zbc_num_k(&l1, &l2, &z2);
	if (s) goto err;

	s = zbc_num_sub(&z1, &z0, &temp, 0);
	if (s) goto err;
	s = zbc_num_sub(&temp, &z2, &z1, 0);
	if (s) goto err;

	s = zbc_num_shift(&z0, max2 * 2);
	if (s) goto err;
	s = zbc_num_shift(&z1, max2);
	if (s) goto err;
	s = zbc_num_add(&z0, &z1, &temp, 0);
	if (s) goto err;
	s = zbc_num_add(&temp, &z2, c, 0);
 err:
	bc_num_free(&temp);
	bc_num_free(&z2);
	bc_num_free(&z1);
	bc_num_free(&z0);
	bc_num_free(&m2);
	bc_num_free(&m1);
	bc_num_free(&h2);
	bc_num_free(&l2);
	bc_num_free(&h1);
	bc_num_free(&l1);
	RETURN_STATUS(s);
}

static FAST_FUNC BC_STATUS zbc_num_m(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	BcNum cpa, cpb;
	size_t maxrdx = BC_MAX(a->rdx, b->rdx);

	scale = BC_MAX(scale, a->rdx);
	scale = BC_MAX(scale, b->rdx);
	scale = BC_MIN(a->rdx + b->rdx, scale);
	maxrdx = BC_MAX(maxrdx, scale);

	bc_num_init(&cpa, a->len);
	bc_num_init(&cpb, b->len);

	bc_num_copy(&cpa, a);
	bc_num_copy(&cpb, b);
	cpa.neg = cpb.neg = false;

	s = zbc_num_shift(&cpa, maxrdx);
	if (s) goto err;
	s = zbc_num_shift(&cpb, maxrdx);
	if (s) goto err;
	s = zbc_num_k(&cpa, &cpb, c);
	if (s) goto err;

	maxrdx += scale;
	bc_num_expand(c, c->len + maxrdx);

	if (c->len < maxrdx) {
		memset(c->num + c->len, 0, (c->cap - c->len) * sizeof(BcDig));
		c->len += maxrdx;
	}

	c->rdx = maxrdx;
	bc_num_retireMul(c, scale, a->neg, b->neg);
 err:
	bc_num_free(&cpb);
	bc_num_free(&cpa);
	RETURN_STATUS(s);
}
#define zbc_num_m(...) (zbc_num_m(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_d(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	size_t len, end, i;
	BcNum cp;

	if (b->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));
	if (a->len == 0) {
		bc_num_setToZero(c, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (BC_NUM_ONE(b)) {
		bc_num_copy(c, a);
		bc_num_retireMul(c, scale, a->neg, b->neg);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_num_init(&cp, BC_NUM_MREQ(a, b, scale));
	bc_num_copy(&cp, a);
	len = b->len;

	if (len > cp.len) {
		bc_num_expand(&cp, len + 2);
		bc_num_extend(&cp, len - cp.len);
	}

	if (b->rdx > cp.rdx) bc_num_extend(&cp, b->rdx - cp.rdx);
	cp.rdx -= b->rdx;
	if (scale > cp.rdx) bc_num_extend(&cp, scale - cp.rdx);

	if (b->rdx == b->len) {
		for (;;) {
			if (len == 0) break;
			len--;
			if (b->num[len] != 0)
				break;
		}
		len++;
	}

	if (cp.cap == cp.len) bc_num_expand(&cp, cp.len + 1);

	// We want an extra zero in front to make things simpler.
	cp.num[cp.len++] = 0;
	end = cp.len - len;

	bc_num_expand(c, cp.len);

	bc_num_zero(c);
	memset(c->num + end, 0, (c->cap - end) * sizeof(BcDig));
	c->rdx = cp.rdx;
	c->len = cp.len;

	s = BC_STATUS_SUCCESS;
	for (i = end - 1; i < end; --i) {
		BcDig *n, q;
		n = cp.num + i;
		for (q = 0; n[len] != 0 || bc_num_compare(n, b->num, len) >= 0; ++q)
			bc_num_subArrays(n, b->num, len);
		c->num[i] = q;
#if ENABLE_FEATURE_BC_SIGNALS
		// a=2^100000
		// scale=40000
		// 1/a <- without check below, this will not be interruptible
		if (G_interrupt) {
			s = BC_STATUS_FAILURE;
			break;
		}
#endif
	}

	bc_num_retireMul(c, scale, a->neg, b->neg);
	bc_num_free(&cp);

	RETURN_STATUS(s);
}
#define zbc_num_d(...) (zbc_num_d(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_r(BcNum *a, BcNum *b, BcNum *restrict c,
                         BcNum *restrict d, size_t scale, size_t ts)
{
	BcStatus s;
	BcNum temp;
	bool neg;

	if (b->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));

	if (a->len == 0) {
		bc_num_setToZero(d, ts);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_num_init(&temp, d->cap);
	s = zbc_num_d(a, b, c, scale);
	if (s) goto err;

	if (scale != 0) scale = ts;

	s = zbc_num_m(c, b, &temp, scale);
	if (s) goto err;
	s = zbc_num_sub(a, &temp, d, scale);
	if (s) goto err;

	if (ts > d->rdx && d->len) bc_num_extend(d, ts - d->rdx);

	neg = d->neg;
	bc_num_retireMul(d, ts, a->neg, b->neg);
	d->neg = neg;
 err:
	bc_num_free(&temp);
	RETURN_STATUS(s);
}
#define zbc_num_r(...) (zbc_num_r(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_rem(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	BcNum c1;
	size_t ts = BC_MAX(scale + b->rdx, a->rdx), len = BC_NUM_MREQ(a, b, ts);

	bc_num_init(&c1, len);
	s = zbc_num_r(a, b, &c1, c, scale, ts);
	bc_num_free(&c1);

	RETURN_STATUS(s);
}
#define zbc_num_rem(...) (zbc_num_rem(__VA_ARGS__) COMMA_SUCCESS)

static FAST_FUNC BC_STATUS zbc_num_p(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcNum copy;
	unsigned long pow;
	size_t i, powrdx, resrdx;
	bool neg;

	// GNU bc does not allow 2^2.0 - we do
	for (i = 0; i < b->rdx; i++)
		if (b->num[i] != 0)
			RETURN_STATUS(bc_error("not an integer"));

	if (b->len == 0) {
		bc_num_one(c);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (a->len == 0) {
		bc_num_setToZero(c, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (BC_NUM_ONE(b)) {
		if (!b->neg)
			bc_num_copy(c, a);
		else
			s = zbc_num_inv(a, c, scale);
		RETURN_STATUS(s);
	}

	neg = b->neg;
	s = zbc_num_ulong_abs(b, &pow);
	if (s) RETURN_STATUS(s);
	// b is not used beyond this point

	bc_num_init(&copy, a->len);
	bc_num_copy(&copy, a);

	if (!neg) {
		if (a->rdx > scale)
			scale = a->rdx;
		if (a->rdx * pow < scale)
			scale = a->rdx * pow;
	}


	for (powrdx = a->rdx; !(pow & 1); pow >>= 1) {
		powrdx <<= 1;
		s = zbc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;
		// Not needed: zbc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	bc_num_copy(c, &copy);

	for (resrdx = powrdx, pow >>= 1; pow != 0; pow >>= 1) {
		powrdx <<= 1;
		s = zbc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;

		if (pow & 1) {
			resrdx += powrdx;
			s = zbc_num_mul(c, &copy, c, resrdx);
			if (s) goto err;
		}
		// Not needed: zbc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	if (neg) {
		s = zbc_num_inv(c, c, scale);
		if (s) goto err;
	}

	if (c->rdx > scale) bc_num_truncate(c, c->rdx - scale);

	// We can't use bc_num_clean() here.
	for (i = 0; i < c->len; ++i)
		if (c->num[i] != 0)
			goto skip;
	bc_num_setToZero(c, scale);
 skip:

 err:
	bc_num_free(&copy);
	RETURN_STATUS(s);
}
#define zbc_num_p(...) (zbc_num_p(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_sqrt(BcNum *a, BcNum *restrict b, size_t scale)
{
	BcStatus s;
	BcNum num1, num2, half, f, fprime, *x0, *x1, *temp;
	size_t pow, len, digs, digs1, resrdx, req, times = 0;
	ssize_t cmp = 1, cmp1 = SSIZE_MAX, cmp2 = SSIZE_MAX;

	req = BC_MAX(scale, a->rdx) + ((BC_NUM_INT(a) + 1) >> 1) + 1;
	bc_num_expand(b, req);

	if (a->len == 0) {
		bc_num_setToZero(b, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}
	if (a->neg) {
		RETURN_STATUS(bc_error("negative number"));
	}
	if (BC_NUM_ONE(a)) {
		bc_num_one(b);
		bc_num_extend(b, scale);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	scale = BC_MAX(scale, a->rdx) + 1;
	len = a->len + scale;

	bc_num_init(&num1, len);
	bc_num_init(&num2, len);
	bc_num_init_DEF_SIZE(&half);

	bc_num_one(&half);
	half.num[0] = 5;
	half.rdx = 1;

	bc_num_init(&f, len);
	bc_num_init(&fprime, len);

	x0 = &num1;
	x1 = &num2;

	bc_num_one(x0);
	pow = BC_NUM_INT(a);

	if (pow) {
		if (pow & 1)
			x0->num[0] = 2;
		else
			x0->num[0] = 6;

		pow -= 2 - (pow & 1);

		bc_num_extend(x0, pow);

		// Make sure to move the radix back.
		x0->rdx -= pow;
	}

	x0->rdx = digs = digs1 = 0;
	resrdx = scale + 2;
	len = BC_NUM_INT(x0) + resrdx - 1;

	while (cmp != 0 || digs < len) {
		s = zbc_num_div(a, x0, &f, resrdx);
		if (s) goto err;
		s = zbc_num_add(x0, &f, &fprime, resrdx);
		if (s) goto err;
		s = zbc_num_mul(&fprime, &half, x1, resrdx);
		if (s) goto err;

		cmp = bc_num_cmp(x1, x0);
		digs = x1->len - (unsigned long long) llabs(cmp);

		if (cmp == cmp2 && digs == digs1)
			times += 1;
		else
			times = 0;

		resrdx += times > 4;

		cmp2 = cmp1;
		cmp1 = cmp;
		digs1 = digs;

		temp = x0;
		x0 = x1;
		x1 = temp;
	}

	bc_num_copy(b, x0);
	scale -= 1;
	if (b->rdx > scale) bc_num_truncate(b, b->rdx - scale);
 err:
	bc_num_free(&fprime);
	bc_num_free(&f);
	bc_num_free(&half);
	bc_num_free(&num2);
	bc_num_free(&num1);
	RETURN_STATUS(s);
}
#define zbc_num_sqrt(...) (zbc_num_sqrt(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d,
                              size_t scale)
{
	BcStatus s;
	BcNum num2, *ptr_a;
	bool init = false;
	size_t ts = BC_MAX(scale + b->rdx, a->rdx), len = BC_NUM_MREQ(a, b, ts);

	if (c == a) {
		memcpy(&num2, c, sizeof(BcNum));
		ptr_a = &num2;
		bc_num_init(c, len);
		init = true;
	} else {
		ptr_a = a;
		bc_num_expand(c, len);
	}

	s = zbc_num_r(ptr_a, b, c, d, scale, ts);

	if (init) bc_num_free(&num2);

	RETURN_STATUS(s);
}
#define zbc_num_divmod(...) (zbc_num_divmod(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_num_modexp(BcNum *a, BcNum *b, BcNum *c, BcNum *restrict d)
{
	BcStatus s;
	BcNum base, exp, two, temp;
	BcDig two_digs[2];

	if (c->len == 0)
		RETURN_STATUS(bc_error("divide by zero"));
	if (a->rdx || b->rdx || c->rdx)
		RETURN_STATUS(bc_error("not an integer"));
	if (b->neg)
		RETURN_STATUS(bc_error("negative number"));

	bc_num_expand(d, c->len);
	bc_num_init(&base, c->len);
	bc_num_init(&exp, b->len);
	bc_num_init(&temp, b->len);

	two.cap = ARRAY_SIZE(two_digs);
	two.num = two_digs;
	bc_num_one(&two);
	two_digs[0] = 2;

	bc_num_one(d);

	s = zbc_num_rem(a, c, &base, 0);
	if (s) goto err;
	bc_num_copy(&exp, b);

	while (exp.len != 0) {
		s = zbc_num_divmod(&exp, &two, &exp, &temp, 0);
		if (s) goto err;

		if (BC_NUM_ONE(&temp)) {
			s = zbc_num_mul(d, &base, &temp, 0);
			if (s) goto err;
			s = zbc_num_rem(&temp, c, d, 0);
			if (s) goto err;
		}

		s = zbc_num_mul(&base, &base, &temp, 0);
		if (s) goto err;
		s = zbc_num_rem(&temp, c, &base, 0);
		if (s) goto err;
	}
 err:
	bc_num_free(&temp);
	bc_num_free(&exp);
	bc_num_free(&base);
	RETURN_STATUS(s);
}
#define zdc_num_modexp(...) (zdc_num_modexp(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static FAST_FUNC void bc_string_free(void *string)
{
	free(*(char**)string);
}

static void bc_func_init(BcFunc *f)
{
	bc_char_vec_init(&f->code);
	IF_BC(bc_vec_init(&f->labels, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&f->autos, sizeof(BcId), bc_id_free);)
	IF_BC(bc_vec_init(&f->strs, sizeof(char *), bc_string_free);)
	IF_BC(bc_vec_init(&f->consts, sizeof(char *), bc_string_free);)
	IF_BC(f->nparams = 0;)
}

static FAST_FUNC void bc_func_free(void *func)
{
	BcFunc *f = (BcFunc *) func;
	bc_vec_free(&f->code);
	IF_BC(bc_vec_free(&f->labels);)
	IF_BC(bc_vec_free(&f->autos);)
	IF_BC(bc_vec_free(&f->strs);)
	IF_BC(bc_vec_free(&f->consts);)
}

static void bc_array_expand(BcVec *a, size_t len);

static void bc_array_init(BcVec *a, bool nums)
{
	if (nums)
		bc_vec_init(a, sizeof(BcNum), bc_num_free);
	else
		bc_vec_init(a, sizeof(BcVec), bc_vec_free);
	bc_array_expand(a, 1);
}

static void bc_array_expand(BcVec *a, size_t len)
{
	if (a->dtor == bc_num_free
	 // && a->size == sizeof(BcNum) - always true
	) {
		BcNum n;
		while (len > a->len) {
			bc_num_init_DEF_SIZE(&n);
			bc_vec_push(a, &n);
		}
	} else {
		BcVec v;
		while (len > a->len) {
			bc_array_init(&v, true);
			bc_vec_push(a, &v);
		}
	}
}

static void bc_array_copy(BcVec *d, const BcVec *s)
{
	BcNum *dnum, *snum;
	size_t i;

	bc_vec_pop_all(d);
	bc_vec_expand(d, s->cap);
	d->len = s->len;

	dnum = (void*)d->v;
	snum = (void*)s->v;
	for (i = 0; i < s->len; i++, dnum++, snum++) {
		bc_num_init(dnum, snum->len);
		bc_num_copy(dnum, snum);
	}
}

#if ENABLE_DC
static void dc_result_copy(BcResult *d, BcResult *src)
{
	d->t = src->t;

	switch (d->t) {
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
			bc_num_init(&d->d.n, src->d.n.len);
			bc_num_copy(&d->d.n, &src->d.n);
			break;
		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
			d->d.id.name = xstrdup(src->d.id.name);
			break;
		case BC_RESULT_CONSTANT:
		IF_BC(case BC_RESULT_LAST:)
		case BC_RESULT_ONE:
		case BC_RESULT_STR:
			memcpy(&d->d.n, &src->d.n, sizeof(BcNum));
			break;
	}
}
#endif // ENABLE_DC

static FAST_FUNC void bc_result_free(void *result)
{
	BcResult *r = (BcResult *) result;

	switch (r->t) {
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
			bc_num_free(&r->d.n);
			break;
		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
			free(r->d.id.name);
			break;
		default:
			// Do nothing.
			break;
	}
}

static int bad_input_byte(char c)
{
	if ((c < ' ' && c != '\t' && c != '\r' && c != '\n') // also allow '\v' '\f'?
	 || c > 0x7e
	) {
		bc_error_fmt("illegal character 0x%02x", c);
		return 1;
	}
	return 0;
}

// Note: it _appends_ data from fp to vec.
static void bc_read_line(BcVec *vec, FILE *fp)
{
 again:
	fflush_and_check();

#if ENABLE_FEATURE_BC_SIGNALS
	if (G_interrupt) { // ^C was pressed
 intr:
		if (fp != stdin) {
			// ^C while running a script (bc SCRIPT): die.
			// We do not return to interactive prompt:
			// user might be running us from a shell,
			// and SCRIPT might be intended to terminate
			// (e.g. contain a "halt" stmt).
			// ^C dropping user into a bc prompt instead of
			// the shell would be unexpected.
			xfunc_die();
		}
		// ^C while interactive input
		G_interrupt = 0;
		// GNU bc says "interrupted execution."
		// GNU dc says "Interrupt!"
		fputs("\ninterrupted execution\n", stderr);
	}

# if ENABLE_FEATURE_EDITING
	if (G_ttyin && fp == stdin) {
		int n, i;
#  define line_buf bb_common_bufsiz1
		n = read_line_input(G.line_input_state, "", line_buf, COMMON_BUFSIZE);
		if (n <= 0) { // read errors or EOF, or ^D, or ^C
			if (n == 0) // ^C
				goto intr;
			bc_vec_pushZeroByte(vec); // ^D or EOF (or error)
			return;
		}
		i = 0;
		for (;;) {
			char c = line_buf[i++];
			if (!c) break;
			if (bad_input_byte(c)) goto again;
		}
		bc_vec_concat(vec, line_buf);
#  undef line_buf
	} else
# endif
#endif
	{
		int c;
		bool bad_chars = 0;
		size_t len = vec->len;

		do {
#if ENABLE_FEATURE_BC_SIGNALS
			if (G_interrupt) {
				// ^C was pressed: ignore entire line, get another one
				vec->len = len;
				goto intr;
			}
#endif
			do c = fgetc(fp); while (c == '\0');
			if (c == EOF) {
				if (ferror(fp))
					bb_perror_msg_and_die("input error");
				// Note: EOF does not append '\n'
				break;
			}
			bad_chars |= bad_input_byte(c);
			bc_vec_pushByte(vec, (char)c);
		} while (c != '\n');

		if (bad_chars) {
			// Bad chars on this line
			if (!G.prog.file) { // stdin
				// ignore entire line, get another one
				vec->len = len;
				goto again;
			}
			bb_perror_msg_and_die("file '%s' is not text", G.prog.file);
		}
		bc_vec_pushZeroByte(vec);
	}
}

//
// Parsing routines
//

static bool bc_num_strValid(const char *val, size_t base)
{
	BcDig b;
	bool radix;

	b = (BcDig)(base <= 10 ? base + '0' : base - 10 + 'A');
	radix = false;
	for (;;) {
		BcDig c = *val++;
		if (c == '\0')
			break;
		if (c == '.') {
			if (radix) return false;
			radix = true;
			continue;
		}
		if (c < '0' || c >= b || (c > '9' && c < 'A'))
			return false;
	}
	return true;
}

// Note: n is already "bc_num_zero()"ed,
// leading zeroes in "val" are removed
static void bc_num_parseDecimal(BcNum *n, const char *val)
{
	size_t len, i;
	const char *ptr;

	len = strlen(val);
	if (len == 0)
		return;

	bc_num_expand(n, len);

	ptr = strchr(val, '.');

	n->rdx = 0;
	if (ptr != NULL)
		n->rdx = (size_t)((val + len) - (ptr + 1));

	for (i = 0; val[i]; ++i) {
		if (val[i] != '0' && val[i] != '.') {
			// Not entirely zero value - convert it, and exit
			i = len - 1;
			for (;;) {
				n->num[n->len] = val[i] - '0';
				++n->len;
 skip_dot:
				if (i == 0) break;
				if (val[--i] == '.') goto skip_dot;
			}
			break;
		}
	}
	// if for() exits without hitting if(), the value is entirely zero
}

// Note: n is already "bc_num_zero()"ed,
// leading zeroes in "val" are removed
static void bc_num_parseBase(BcNum *n, const char *val, unsigned base_t)
{
	BcStatus s;
	BcNum temp, mult, result;
	BcNum base;
	BcDig base_digs[ULONG_NUM_BUFSIZE];
	BcDig c = '\0';
	unsigned long v;
	size_t i, digits;

	for (i = 0; ; ++i) {
		if (val[i] == '\0')
			return;
		if (val[i] != '.' && val[i] != '0')
			break;
	}

	bc_num_init_DEF_SIZE(&temp);
	bc_num_init_DEF_SIZE(&mult);
	base.cap = ARRAY_SIZE(base_digs);
	base.num = base_digs;
	bc_num_ulong2num(&base, base_t);

	for (;;) {
		c = *val++;
		if (c == '\0') goto int_err;
		if (c == '.') break;

		v = (unsigned long) (c <= '9' ? c - '0' : c - 'A' + 10);

		s = zbc_num_mul(n, &base, &mult, 0);
		if (s) goto int_err;
		bc_num_ulong2num(&temp, v);
		s = zbc_num_add(&mult, &temp, n, 0);
		if (s) goto int_err;
	}

	bc_num_init(&result, base.len);
	//bc_num_zero(&result); - already is
	bc_num_one(&mult);

	digits = 0;
	for (;;) {
		c = *val++;
		if (c == '\0') break;
		digits++;

		v = (unsigned long) (c <= '9' ? c - '0' : c - 'A' + 10);

		s = zbc_num_mul(&result, &base, &result, 0);
		if (s) goto err;
		bc_num_ulong2num(&temp, v);
		s = zbc_num_add(&result, &temp, &result, 0);
		if (s) goto err;
		s = zbc_num_mul(&mult, &base, &mult, 0);
		if (s) goto err;
	}

	s = zbc_num_div(&result, &mult, &result, digits);
	if (s) goto err;
	s = zbc_num_add(n, &result, n, digits);
	if (s) goto err;

	if (n->len != 0) {
		if (n->rdx < digits)
			bc_num_extend(n, digits - n->rdx);
	} else
		bc_num_zero(n);
 err:
	bc_num_free(&result);
 int_err:
	bc_num_free(&mult);
	bc_num_free(&temp);
}

static BC_STATUS zbc_num_parse(BcNum *n, const char *val, unsigned base_t)
{
	if (!bc_num_strValid(val, base_t))
		RETURN_STATUS(bc_error("bad number string"));

	bc_num_zero(n);
	while (*val == '0') val++;

	if (base_t == 10)
		bc_num_parseDecimal(n, val);
	else
		bc_num_parseBase(n, val, base_t);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_num_parse(...) (zbc_num_parse(__VA_ARGS__) COMMA_SUCCESS)

static void bc_lex_lineComment(BcLex *l)
{
	// Try: echo -n '#foo' | bc
	size_t i;
	l->lex = XC_LEX_WHITESPACE;
	i = l->i;
	while (i < l->len && l->buf[i] != '\n')
		i++;
	l->i = i;
}

static void bc_lex_whitespace(BcLex *l)
{
	l->lex = XC_LEX_WHITESPACE;
	for (;;) {
		char c = l->buf[l->i];
		if (c == '\n') // this is XC_LEX_NLINE, not XC_LEX_WHITESPACE
			break;
		if (!isspace(c))
			break;
		l->i++;
	}
}

static BC_STATUS zbc_lex_number(BcLex *l, char start)
{
	const char *buf = l->buf + l->i;
	size_t len, i, ccnt;
	bool pt;

	pt = (start == '.');
	l->lex = XC_LEX_NUMBER;
	ccnt = i = 0;
	for (;;) {
		char c = buf[i];
		if (c == '\0')
			break;
		if (c == '\\' && buf[i + 1] == '\n') {
			i += 2;
			//number_of_backslashes++ - see comment below
			continue;
		}
		if (!isdigit(c) && (c < 'A' || c > 'F')) {
			if (c != '.') break;
			// if '.' was already seen, stop on second one:
			if (pt) break;
			pt = true;
		}
		// buf[i] is one of "0-9A-F."
		i++;
		if (c != '.')
			ccnt = i;
	}
	//ccnt is the number of chars in the number string, excluding possible
	//trailing "[\<newline>].[\<newline>]" (with any number of \<NL> repetitions).
	//i is buf[i] index of the first not-yet-parsed char after that.
	l->i += i;

	// This might overestimate the size, if there are "\<NL>"'s
	// in the number. Subtracting number_of_backslashes*2 correctly
	// is not that easy: consider that in the case of "NNN.\<NL>"
	// loop above will count "\<NL>" before it realizes it is not
	// in fact *inside* the number:
	len = ccnt + 1; // +1 byte for NUL termination

	// This check makes sense only if size_t is (much) larger than BC_MAX_NUM.
	if (SIZE_MAX > (BC_MAX_NUM | 0xff)) {
		if (len > BC_MAX_NUM)
			RETURN_STATUS(bc_error("number too long: must be [1,"BC_MAX_NUM_STR"]"));
	}

	bc_vec_pop_all(&l->lex_buf);
	bc_vec_expand(&l->lex_buf, 1 + len);
	bc_vec_push(&l->lex_buf, &start);

	while (ccnt != 0) {
		// If we have hit a backslash, skip it. We don't have
		// to check for a newline because it's guaranteed.
		if (*buf == '\\') {
			buf += 2;
			ccnt -= 2;
			continue;
		}
		bc_vec_push(&l->lex_buf, buf);
		buf++;
		ccnt--;
	}

	bc_vec_pushZeroByte(&l->lex_buf);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_number(...) (zbc_lex_number(__VA_ARGS__) COMMA_SUCCESS)

static void bc_lex_name(BcLex *l)
{
	size_t i;
	const char *buf;

	l->lex = XC_LEX_NAME;

	i = 0;
	buf = l->buf + l->i - 1;
	for (;;) {
		char c = buf[i];
		if ((c < 'a' || c > 'z') && !isdigit(c) && c != '_') break;
		i++;
	}

#if 0 // We do not protect against people with gigabyte-long names
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (i > BC_MAX_STRING)
			return bc_error("name too long: must be [1,"BC_MAX_STRING_STR"]");
	}
#endif
	bc_vec_string(&l->lex_buf, i, buf);

	// Increment the index. We minus 1 because it has already been incremented.
	l->i += i - 1;

	//return BC_STATUS_SUCCESS;
}

static void bc_lex_init(BcLex *l)
{
	bc_char_vec_init(&l->lex_buf);
}

static void bc_lex_free(BcLex *l)
{
	bc_vec_free(&l->lex_buf);
}

static void bc_lex_file(BcLex *l)
{
	G.err_line = l->line = 1;
	l->newline = false;
}

static bool bc_lex_more_input(BcLex *l)
{
	size_t str;
	bool comment;

	bc_vec_pop_all(&G.input_buffer);

	// This loop is complex because the vm tries not to send any lines that end
	// with a backslash to the parser. The reason for that is because the parser
	// treats a backslash+newline combo as whitespace, per the bc spec. In that
	// case, and for strings and comments, the parser will expect more stuff.
	comment = false;
	str = 0;
	for (;;) {
		size_t prevlen = G.input_buffer.len;
		char *string;

		bc_read_line(&G.input_buffer, G.input_fp);
		// No more input means EOF
		if (G.input_buffer.len <= prevlen + 1) // (we expect +1 for NUL byte)
			break;

		string = G.input_buffer.v + prevlen;
		while (*string) {
			char c = *string;
			if (string == G.input_buffer.v || string[-1] != '\\') {
				if (IS_BC)
					str ^= (c == '"');
				else {
					if (c == ']')
						str -= 1;
					else if (c == '[')
						str += 1;
				}
			}
			string++;
			if (c == '/' && *string == '*') {
				comment = true;
				string++;
				continue;
			}
			if (c == '*' && *string == '/') {
				comment = false;
				string++;
			}
		}
		if (str != 0 || comment) {
			G.input_buffer.len--; // backstep over the trailing NUL byte
			continue;
		}

		// Check for backslash+newline.
		// we do not check that last char is '\n' -
		// if it is not, then it's EOF, and looping back
		// to bc_read_line() will detect it:
		string -= 2;
		if (string >= G.input_buffer.v && *string == '\\') {
			G.input_buffer.len--;
			continue;
		}

		break;
	}

	l->buf = G.input_buffer.v;
	l->i = 0;
//	bb_error_msg("G.input_buffer.len:%d '%s'", G.input_buffer.len, G.input_buffer.v);
	l->len = G.input_buffer.len - 1; // do not include NUL

	return l->len != 0;
}

IF_BC(static BC_STATUS zbc_lex_token(BcLex *l);)
IF_DC(static BC_STATUS zdc_lex_token(BcLex *l);)
#define zbc_lex_token(...) (zbc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#define zdc_lex_token(...) (zdc_lex_token(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_next(BcLex *l)
{
	BcStatus s;

	l->lex_last = l->lex;
	if (l->lex_last == XC_LEX_EOF) RETURN_STATUS(bc_error("end of file"));

	l->line += l->newline;
	G.err_line = l->line;
	l->newline = false;

	// Loop until failure or we don't have whitespace. This
	// is so the parser doesn't get inundated with whitespace.
	// Comments are also XC_LEX_WHITESPACE tokens and eaten here.
	s = BC_STATUS_SUCCESS;
	do {
		if (l->i == l->len) {
			l->lex = XC_LEX_EOF;
			if (!G.input_fp)
				RETURN_STATUS(BC_STATUS_SUCCESS);
			if (!bc_lex_more_input(l)) {
				G.input_fp = NULL;
				RETURN_STATUS(BC_STATUS_SUCCESS);
			}
			// here it's guaranteed that l->i is below l->len
		}
		dbg_lex("next string to parse:'%.*s'",
			(int)(strchrnul(l->buf + l->i, '\n') - (l->buf + l->i)),
			l->buf + l->i);
		if (IS_BC) {
			IF_BC(s = zbc_lex_token(l));
		} else {
			IF_DC(s = zdc_lex_token(l));
		}
	} while (!s && l->lex == XC_LEX_WHITESPACE);
	dbg_lex("l->lex from string:%d", l->lex);

	RETURN_STATUS(s);
}
#define zbc_lex_next(...) (zbc_lex_next(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static BC_STATUS zbc_lex_skip_if_at_NLINE(BcLex *l)
{
	if (l->lex == XC_LEX_NLINE)
		RETURN_STATUS(zbc_lex_next(l));
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_skip_if_at_NLINE(...) (zbc_lex_skip_if_at_NLINE(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_next_and_skip_NLINE(BcLex *l)
{
	BcStatus s;
	s = zbc_lex_next(l);
	if (s) RETURN_STATUS(s);
	// if(cond)<newline>stmt is accepted too (but not 2+ newlines)
	s = zbc_lex_skip_if_at_NLINE(l);
	RETURN_STATUS(s);
}
#define zbc_lex_next_and_skip_NLINE(...) (zbc_lex_next_and_skip_NLINE(__VA_ARGS__) COMMA_SUCCESS)
#endif

static BC_STATUS zbc_lex_text_init(BcLex *l, const char *text)
{
	l->buf = text;
	l->i = 0;
	l->len = strlen(text);
	l->lex = l->lex_last = XC_LEX_INVALID;
	RETURN_STATUS(zbc_lex_next(l));
}
#define zbc_lex_text_init(...) (zbc_lex_text_init(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static BC_STATUS zbc_lex_identifier(BcLex *l)
{
	BcStatus s;
	unsigned i;
	const char *buf = l->buf + l->i - 1;

	for (i = 0; i < ARRAY_SIZE(bc_lex_kws); ++i) {
		const char *keyword8 = bc_lex_kws[i].name8;
		unsigned j = 0;
		while (buf[j] != '\0' && buf[j] == keyword8[j]) {
			j++;
			if (j == 8) goto match;
		}
		if (keyword8[j] != '\0')
			continue;
 match:
		// buf starts with keyword bc_lex_kws[i]
		if (isalnum(buf[j]) || buf[j]=='_')
			continue; // "ifz" does not match "if" keyword, "if." does
		l->lex = BC_LEX_KEY_1st_keyword + i;
		if (!bc_lex_kws_POSIX(i)) {
			s = zbc_posix_error_fmt("%sthe '%.8s' keyword", "POSIX does not allow ", bc_lex_kws[i].name8);
			if (s) RETURN_STATUS(s);
		}

		// We minus 1 because the index has already been incremented.
		l->i += j - 1;
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_lex_name(l);
	s = BC_STATUS_SUCCESS;

	if (l->lex_buf.len > 2) {
		// Prevent this:
		// >>> qwe=1
		// bc: POSIX only allows one character names; the following is bad: 'qwe=1
		// '
		unsigned len = strchrnul(buf, '\n') - buf;
		s = zbc_posix_error_fmt("POSIX only allows one character names; the following is bad: '%.*s'", len, buf);
	}

	RETURN_STATUS(s);
}
#define zbc_lex_identifier(...) (zbc_lex_identifier(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_lex_string(BcLex *l)
{
	size_t len, nls, i;

	l->lex = XC_LEX_STR;

	nls = 0;
	i = l->i;
	for (;;) {
		char c = l->buf[i];
		if (c == '\0') {
			l->i = i;
			RETURN_STATUS(bc_error("string end could not be found"));
		}
		if (c == '"')
			break;
		nls += (c == '\n');
		i++;
	}

	len = i - l->i;
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (len > BC_MAX_STRING)
			RETURN_STATUS(bc_error("string too long: must be [1,"BC_MAX_STRING_STR"]"));
	}
	bc_vec_string(&l->lex_buf, len, l->buf + l->i);

	l->i = i + 1;
	l->line += nls;
	G.err_line = l->line;

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_string(...) (zbc_lex_string(__VA_ARGS__) COMMA_SUCCESS)

static void bc_lex_assign(BcLex *l, unsigned with_and_without)
{
	if (l->buf[l->i] == '=') {
		++l->i;
		with_and_without >>= 8; // store "with" value
	} // else store "without" value
	l->lex = (with_and_without & 0xff);
}
#define bc_lex_assign(l, with, without) \
	bc_lex_assign(l, ((with)<<8)|(without))

static BC_STATUS zbc_lex_comment(BcLex *l)
{
	size_t i, nls = 0;
	const char *buf = l->buf;

	l->lex = XC_LEX_WHITESPACE;
	i = l->i; /* here buf[l->i] is the '*' of opening comment delimiter */
	for (;;) {
		char c = buf[++i];
 check_star:
		if (c == '*') {
			c = buf[++i];
			if (c == '/')
				break;
			goto check_star;
		}
		if (c == '\0') {
			l->i = i;
			RETURN_STATUS(bc_error("comment end could not be found"));
		}
		nls += (c == '\n');
	}

	l->i = i + 1;
	l->line += nls;
	G.err_line = l->line;

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_lex_comment(...) (zbc_lex_comment(__VA_ARGS__) COMMA_SUCCESS)

#undef zbc_lex_token
static BC_STATUS zbc_lex_token(BcLex *l)
{
	BcStatus s = BC_STATUS_SUCCESS;
	char c = l->buf[l->i++], c2;

	// This is the workhorse of the lexer.
	switch (c) {
//		case '\0': // probably never reached
//			l->i--;
//			l->lex = XC_LEX_EOF;
//			l->newline = true;
//			break;
		case '\n':
			l->lex = XC_LEX_NLINE;
			l->newline = true;
			break;
		case '\t':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			bc_lex_whitespace(l);
			break;
		case '!':
			bc_lex_assign(l, XC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);
			if (l->lex == BC_LEX_OP_BOOL_NOT) {
				s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("!");
				if (s) RETURN_STATUS(s);
			}
			break;
		case '"':
			s = zbc_lex_string(l);
			break;
		case '#':
			s = zbc_POSIX_does_not_allow("'#' script comments");
			if (s) RETURN_STATUS(s);
			bc_lex_lineComment(l);
			break;
		case '%':
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MODULUS, XC_LEX_OP_MODULUS);
			break;
		case '&':
			c2 = l->buf[l->i];
			if (c2 == '&') {
				s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("&&");
				if (s) RETURN_STATUS(s);
				++l->i;
				l->lex = BC_LEX_OP_BOOL_AND;
			} else {
				l->lex = XC_LEX_INVALID;
				s = bc_error_bad_character('&');
			}
			break;
		case '(':
		case ')':
			l->lex = (BcLexType)(c - '(' + BC_LEX_LPAREN);
			break;
		case '*':
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MULTIPLY, XC_LEX_OP_MULTIPLY);
			break;
		case '+':
			c2 = l->buf[l->i];
			if (c2 == '+') {
				++l->i;
				l->lex = BC_LEX_OP_INC;
			} else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_PLUS, XC_LEX_OP_PLUS);
			break;
		case ',':
			l->lex = BC_LEX_COMMA;
			break;
		case '-':
			c2 = l->buf[l->i];
			if (c2 == '-') {
				++l->i;
				l->lex = BC_LEX_OP_DEC;
			} else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_MINUS, XC_LEX_OP_MINUS);
			break;
		case '.':
			if (isdigit(l->buf[l->i]))
				s = zbc_lex_number(l, c);
			else {
				l->lex = BC_LEX_KEY_LAST;
				s = zbc_POSIX_does_not_allow("a period ('.') as a shortcut for the last result");
			}
			break;
		case '/':
			c2 = l->buf[l->i];
			if (c2 == '*')
				s = zbc_lex_comment(l);
			else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_DIVIDE, XC_LEX_OP_DIVIDE);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			s = zbc_lex_number(l, c);
			break;
		case ';':
			l->lex = BC_LEX_SCOLON;
			break;
		case '<':
			bc_lex_assign(l, XC_LEX_OP_REL_LE, XC_LEX_OP_REL_LT);
			break;
		case '=':
			bc_lex_assign(l, XC_LEX_OP_REL_EQ, BC_LEX_OP_ASSIGN);
			break;
		case '>':
			bc_lex_assign(l, XC_LEX_OP_REL_GE, XC_LEX_OP_REL_GT);
			break;
		case '[':
		case ']':
			l->lex = (BcLexType)(c - '[' + BC_LEX_LBRACKET);
			break;
		case '\\':
			if (l->buf[l->i] == '\n') {
				l->lex = XC_LEX_WHITESPACE;
				++l->i;
			} else
				s = bc_error_bad_character(c);
			break;
		case '^':
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_POWER, XC_LEX_OP_POWER);
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
			s = zbc_lex_identifier(l);
			break;
		case '{':
		case '}':
			l->lex = (BcLexType)(c - '{' + BC_LEX_LBRACE);
			break;
		case '|':
			c2 = l->buf[l->i];
			if (c2 == '|') {
				s = zbc_POSIX_does_not_allow_bool_ops_this_is_bad("||");
				if (s) RETURN_STATUS(s);
				++l->i;
				l->lex = BC_LEX_OP_BOOL_OR;
			} else {
				l->lex = XC_LEX_INVALID;
				s = bc_error_bad_character(c);
			}
			break;
		default:
			l->lex = XC_LEX_INVALID;
			s = bc_error_bad_character(c);
			break;
	}

	RETURN_STATUS(s);
}
#define zbc_lex_token(...) (zbc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_BC

#if ENABLE_DC
static BC_STATUS zdc_lex_register(BcLex *l)
{
	if (G_exreg && isspace(l->buf[l->i])) {
		bc_lex_whitespace(l); // eats whitespace (but not newline)
		l->i++; // bc_lex_name() expects this
		bc_lex_name(l);
	} else {
		bc_vec_pop_all(&l->lex_buf);
		bc_vec_push(&l->lex_buf, &l->buf[l->i++]);
		bc_vec_pushZeroByte(&l->lex_buf);
		l->lex = XC_LEX_NAME;
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_lex_register(...) (zdc_lex_register(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_lex_string(BcLex *l)
{
	size_t depth, nls, i;

	l->lex = XC_LEX_STR;
	bc_vec_pop_all(&l->lex_buf);

	nls = 0;
	depth = 1;
	i = l->i;
	for (;;) {
		char c = l->buf[i];
		if (c == '\0') {
			l->i = i;
			RETURN_STATUS(bc_error("string end could not be found"));
		}
		nls += (c == '\n');
		if (i == l->i || l->buf[i - 1] != '\\') {
			if (c == '[') depth++;
			if (c == ']')
				if (--depth == 0)
					break;
		}
		bc_vec_push(&l->lex_buf, &l->buf[i]);
		i++;
	}
	i++;

	bc_vec_pushZeroByte(&l->lex_buf);
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (i - l->i > BC_MAX_STRING)
			RETURN_STATUS(bc_error("string too long: must be [1,"BC_MAX_STRING_STR"]"));
	}

	l->i = i;
	l->line += nls;
	G.err_line = l->line;

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_lex_string(...) (zdc_lex_string(__VA_ARGS__) COMMA_SUCCESS)

#undef zdc_lex_token
static BC_STATUS zdc_lex_token(BcLex *l)
{
	static const //BcLexType - should be this type, but narrower type saves size:
	uint8_t
	dc_lex_regs[] = {
		XC_LEX_OP_REL_EQ, XC_LEX_OP_REL_LE, XC_LEX_OP_REL_GE, XC_LEX_OP_REL_NE,
		XC_LEX_OP_REL_LT, XC_LEX_OP_REL_GT, DC_LEX_SCOLON, DC_LEX_COLON,
		DC_LEX_ELSE, DC_LEX_LOAD, DC_LEX_LOAD_POP, DC_LEX_OP_ASSIGN,
		DC_LEX_STORE_PUSH,
	};

	BcStatus s;
	char c, c2;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dc_lex_regs); ++i) {
		if (l->lex_last == dc_lex_regs[i])
			RETURN_STATUS(zdc_lex_register(l));
	}

	s = BC_STATUS_SUCCESS;
	c = l->buf[l->i++];
	if (c >= '%' && c <= '~'
	 && (l->lex = dc_char_to_LEX[c - '%']) != XC_LEX_INVALID
	) {
		RETURN_STATUS(s);
	}

	// This is the workhorse of the lexer.
	switch (c) {
//		case '\0': // probably never reached
//			l->lex = XC_LEX_EOF;
//			break;
		case '\n':
			// '\n' is XC_LEX_NLINE, not XC_LEX_WHITESPACE
			// (and "case '\n':" is not just empty here)
			// only to allow interactive dc have a way to exit
			// "parse" stage of "parse,execute" loop
			// on <enter>, not on _next_ token (which would mean
			// commands are not executed on pressing <enter>).
			// IOW: typing "1p<enter>" should print "1" _at once_,
			// not after some more input.
			l->lex = XC_LEX_NLINE;
			l->newline = true;
			break;
		case '\t':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			l->newline = 0; // was (c == '\n')
			bc_lex_whitespace(l);
			break;
		case '!':
			c2 = l->buf[l->i];
			if (c2 == '=')
				l->lex = XC_LEX_OP_REL_NE;
			else if (c2 == '<')
				l->lex = XC_LEX_OP_REL_LE;
			else if (c2 == '>')
				l->lex = XC_LEX_OP_REL_GE;
			else
				RETURN_STATUS(bc_error_bad_character(c));
			++l->i;
			break;
		case '#':
			bc_lex_lineComment(l);
			break;
		case '.':
			if (isdigit(l->buf[l->i]))
				s = zbc_lex_number(l, c);
			else
				s = bc_error_bad_character(c);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			s = zbc_lex_number(l, c);
			break;
		case '[':
			s = zdc_lex_string(l);
			break;
		default:
			l->lex = XC_LEX_INVALID;
			s = bc_error_bad_character(c);
			break;
	}

	RETURN_STATUS(s);
}
#define zdc_lex_token(...) (zdc_lex_token(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static void bc_parse_push(BcParse *p, char i)
{
	dbg_compile("%s:%d pushing bytecode %zd:%d", __func__, __LINE__, p->func->code.len, i);
	bc_vec_pushByte(&p->func->code, i);
}

static void bc_parse_pushName(BcParse *p, char *name)
{
	while (*name)
		bc_parse_push(p, *name++);
	bc_parse_push(p, BC_PARSE_STREND);
}

static void bc_parse_pushIndex(BcParse *p, size_t idx)
{
	size_t mask;
	unsigned amt;

	dbg_lex("%s:%d pushing index %zd", __func__, __LINE__, idx);
	mask = ((size_t)0xff) << (sizeof(idx) * 8 - 8);
	amt = sizeof(idx);
	do {
		if (idx & mask) break;
		mask >>= 8;
		amt--;
	} while (amt != 0);

	bc_parse_push(p, amt);

	while (idx != 0) {
		bc_parse_push(p, (unsigned char)idx);
		idx >>= 8;
	}
}

#if ENABLE_BC
static void bc_parse_pushJUMP(BcParse *p, size_t idx)
{
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, idx);
}

static void bc_parse_pushJUMP_ZERO(BcParse *p, size_t idx)
{
	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, idx);
}

static BC_STATUS zbc_parse_pushSTR(BcParse *p)
{
	char *str = xstrdup(p->l.lex_buf.v);

	bc_parse_push(p, XC_INST_STR);
	bc_parse_pushIndex(p, p->func->strs.len);
	bc_vec_push(&p->func->strs, &str);

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_pushSTR(...) (zbc_parse_pushSTR(__VA_ARGS__) COMMA_SUCCESS)
#endif

static void bc_parse_pushNUM(BcParse *p)
{
	char *num = xstrdup(p->l.lex_buf.v);
#if ENABLE_BC && ENABLE_DC
	size_t idx = bc_vec_push(IS_BC ? &p->func->consts : &G.prog.consts, &num);
#elif ENABLE_BC
	size_t idx = bc_vec_push(&p->func->consts, &num);
#else // DC
	size_t idx = bc_vec_push(&G.prog.consts, &num);
#endif
	bc_parse_push(p, XC_INST_NUM);
	bc_parse_pushIndex(p, idx);
}

static BC_STATUS zbc_parse_text_init(BcParse *p, const char *text)
{
	p->func = bc_program_func(p->fidx);

	RETURN_STATUS(zbc_lex_text_init(&p->l, text));
}
#define zbc_parse_text_init(...) (zbc_parse_text_init(__VA_ARGS__) COMMA_SUCCESS)

// Called when parsing or execution detects a failure,
// resets execution structures.
static void bc_program_reset(void)
{
	BcFunc *f;
	BcInstPtr *ip;

	bc_vec_npop(&G.prog.exestack, G.prog.exestack.len - 1);
	bc_vec_pop_all(&G.prog.results);

	f = bc_program_func_BC_PROG_MAIN();
	ip = bc_vec_top(&G.prog.exestack);
	ip->inst_idx = f->code.len;
}

// Called when parsing code detects a failure,
// resets parsing structures.
static void bc_parse_reset(BcParse *p)
{
	if (p->fidx != BC_PROG_MAIN) {
		bc_func_free(p->func);
		bc_func_init(p->func);

		p->fidx = BC_PROG_MAIN;
		p->func = bc_program_func_BC_PROG_MAIN();
	}

	p->l.i = p->l.len;
	p->l.lex = XC_LEX_EOF;

	IF_BC(bc_vec_pop_all(&p->exits);)
	IF_BC(bc_vec_pop_all(&p->conds);)
	IF_BC(bc_vec_pop_all(&p->ops);)

	bc_program_reset();
}

static void bc_parse_free(BcParse *p)
{
	IF_BC(bc_vec_free(&p->exits);)
	IF_BC(bc_vec_free(&p->conds);)
	IF_BC(bc_vec_free(&p->ops);)
	bc_lex_free(&p->l);
}

static void bc_parse_create(BcParse *p, size_t fidx)
{
	memset(p, 0, sizeof(BcParse));

	bc_lex_init(&p->l);
	IF_BC(bc_vec_init(&p->exits, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&p->conds, sizeof(size_t), NULL);)
	IF_BC(bc_vec_init(&p->ops, sizeof(BcLexType), NULL);)

	p->fidx = fidx;
	p->func = bc_program_func(fidx);
}

static void bc_program_add_fn(void)
{
	//size_t idx;
	BcFunc f;
	bc_func_init(&f);
	//idx =
	bc_vec_push(&G.prog.fns, &f);
	//return idx;
}

#if ENABLE_BC

// Note: takes ownership of 'name' (must be malloced)
static size_t bc_program_addFunc(char *name)
{
	size_t idx;
	BcId entry, *entry_ptr;
	int inserted;

	entry.name = name;
	entry.idx = G.prog.fns.len;

	inserted = bc_map_insert(&G.prog.fn_map, &entry, &idx);
	if (!inserted) free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, idx);
	idx = entry_ptr->idx;

	if (!inserted) {
		// There is already a function with this name.
		// It'll be redefined now, clear old definition.
		BcFunc *func = bc_program_func(entry_ptr->idx);
		bc_func_free(func);
		bc_func_init(func);
	} else {
		bc_program_add_fn();
	}

	return idx;
}

#define BC_PARSE_TOP_OP(p) (*(BcLexType*)bc_vec_top(&(p)->ops))
// We can calculate the conversion between tokens and exprs by subtracting the
// position of the first operator in the lex enum and adding the position of the
// first in the expr enum. Note: This only works for binary operators.
#define BC_TOKEN_2_INST(t) ((char) ((t) - XC_LEX_OP_POWER + XC_INST_POWER))

static BcStatus bc_parse_expr_empty_ok(BcParse *p, uint8_t flags);

static BC_STATUS zbc_parse_expr(BcParse *p, uint8_t flags)
{
	BcStatus s;

	s = bc_parse_expr_empty_ok(p, flags);
	if (s == BC_STATUS_PARSE_EMPTY_EXP)
		RETURN_STATUS(bc_error("empty expression"));
	RETURN_STATUS(s);
}
#define zbc_parse_expr(...) (zbc_parse_expr(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_possibly_auto(BcParse *p, bool auto_allowed);
#define zbc_parse_stmt_possibly_auto(...) (zbc_parse_stmt_possibly_auto(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt(BcParse *p)
{
	RETURN_STATUS(zbc_parse_stmt_possibly_auto(p, false));
}
#define zbc_parse_stmt(...) (zbc_parse_stmt(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_allow_NLINE_before(BcParse *p, const char *after_X)
{
	// "if(cond)<newline>stmt" is accepted too, but not 2+ newlines.
	// Same for "else", "while()", "for()".
	BcStatus s = zbc_lex_next_and_skip_NLINE(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex == XC_LEX_NLINE)
		RETURN_STATUS(bc_error_fmt("no statement after '%s'", after_X));

	RETURN_STATUS(zbc_parse_stmt(p));
}
#define zbc_parse_stmt_allow_NLINE_before(...) (zbc_parse_stmt_allow_NLINE_before(__VA_ARGS__) COMMA_SUCCESS)

static void bc_parse_operator(BcParse *p, BcLexType type, size_t start,
                                  size_t *nexprs)
{
	char l, r = bc_parse_op_PREC(type - XC_LEX_1st_op);
	bool left = bc_parse_op_LEFT(type - XC_LEX_1st_op);

	while (p->ops.len > start) {
		BcLexType t = BC_PARSE_TOP_OP(p);
		if (t == BC_LEX_LPAREN) break;

		l = bc_parse_op_PREC(t - XC_LEX_1st_op);
		if (l >= r && (l != r || !left)) break;

		bc_parse_push(p, BC_TOKEN_2_INST(t));
		bc_vec_pop(&p->ops);
		*nexprs -= (t != BC_LEX_OP_BOOL_NOT && t != XC_LEX_NEG);
	}

	bc_vec_push(&p->ops, &type);
}

static BC_STATUS zbc_parse_rightParen(BcParse *p, size_t ops_bgn, size_t *nexs)
{
	BcLexType top;

	if (p->ops.len <= ops_bgn)
		RETURN_STATUS(bc_error_bad_expression());
	top = BC_PARSE_TOP_OP(p);

	while (top != BC_LEX_LPAREN) {
		bc_parse_push(p, BC_TOKEN_2_INST(top));

		bc_vec_pop(&p->ops);
		*nexs -= (top != BC_LEX_OP_BOOL_NOT && top != XC_LEX_NEG);

		if (p->ops.len <= ops_bgn)
			RETURN_STATUS(bc_error_bad_expression());
		top = BC_PARSE_TOP_OP(p);
	}

	bc_vec_pop(&p->ops);

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_rightParen(...) (zbc_parse_rightParen(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_params(BcParse *p, uint8_t flags)
{
	BcStatus s;
	size_t nparams;

	dbg_lex("%s:%d p->l.lex:%d", __func__, __LINE__, p->l.lex);
	flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	nparams = 0;
	if (p->l.lex != BC_LEX_RPAREN) {
		for (;;) {
			s = zbc_parse_expr(p, flags);
			if (s) RETURN_STATUS(s);
			nparams++;
			if (p->l.lex != BC_LEX_COMMA) {
				if (p->l.lex == BC_LEX_RPAREN)
					break;
				RETURN_STATUS(bc_error_bad_token());
			}
			s = zbc_lex_next(&p->l);
			if (s) RETURN_STATUS(s);
		}
	}

	bc_parse_push(p, BC_INST_CALL);
	bc_parse_pushIndex(p, nparams);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_params(...) (zbc_parse_params(__VA_ARGS__) COMMA_SUCCESS)

// Note: takes ownership of 'name' (must be malloced)
static BC_STATUS zbc_parse_call(BcParse *p, char *name, uint8_t flags)
{
	BcStatus s;
	BcId entry, *entry_ptr;
	size_t idx;

	entry.name = name;

	s = zbc_parse_params(p, flags);
	if (s) goto err;

	if (p->l.lex != BC_LEX_RPAREN) {
		s = bc_error_bad_token();
		goto err;
	}

	idx = bc_map_find_exact(&G.prog.fn_map, &entry);

	if (idx == BC_VEC_INVALID_IDX) {
		// No such function exists, create an empty one
		bc_program_addFunc(name);
		idx = bc_map_find_exact(&G.prog.fn_map, &entry);
	} else
		free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, idx);
	bc_parse_pushIndex(p, entry_ptr->idx);

	RETURN_STATUS(zbc_lex_next(&p->l));
 err:
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_call(...) (zbc_parse_call(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_name(BcParse *p, BcInst *type, uint8_t flags)
{
	BcStatus s;
	char *name;

	name = xstrdup(p->l.lex_buf.v);
	s = zbc_lex_next(&p->l);
	if (s) goto err;

	if (p->l.lex == BC_LEX_LBRACKET) {
		s = zbc_lex_next(&p->l);
		if (s) goto err;

		if (p->l.lex == BC_LEX_RBRACKET) {
			if (!(flags & BC_PARSE_ARRAY)) {
				s = bc_error_bad_expression();
				goto err;
			}
			*type = XC_INST_ARRAY;
		} else {
			*type = XC_INST_ARRAY_ELEM;
			flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
			s = zbc_parse_expr(p, flags);
			if (s) goto err;
		}
		s = zbc_lex_next(&p->l);
		if (s) goto err;
		bc_parse_push(p, *type);
		bc_parse_pushName(p, name);
		free(name);
	} else if (p->l.lex == BC_LEX_LPAREN) {
		if (flags & BC_PARSE_NOCALL) {
			s = bc_error_bad_token();
			goto err;
		}
		*type = BC_INST_CALL;
		s = zbc_parse_call(p, name, flags);
	} else {
		*type = XC_INST_VAR;
		bc_parse_push(p, XC_INST_VAR);
		bc_parse_pushName(p, name);
		free(name);
	}

	RETURN_STATUS(s);
 err:
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_name(...) (zbc_parse_name(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_read(BcParse *p)
{
	BcStatus s;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	bc_parse_push(p, XC_INST_READ);

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_read(...) (zbc_parse_read(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_builtin(BcParse *p, BcLexType type, uint8_t flags,
                                 BcInst *prev)
{
	BcStatus s;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	s = zbc_parse_expr(p, flags);
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	*prev = (type == BC_LEX_KEY_LENGTH) ? XC_INST_LENGTH : XC_INST_SQRT;
	bc_parse_push(p, *prev);

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_builtin(...) (zbc_parse_builtin(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_scale(BcParse *p, BcInst *type, uint8_t flags)
{
	BcStatus s;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_LPAREN) {
		*type = XC_INST_SCALE;
		bc_parse_push(p, XC_INST_SCALE);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	*type = XC_INST_SCALE_FUNC;
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	s = zbc_parse_expr(p, flags);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_RPAREN)
		RETURN_STATUS(bc_error_bad_token());
	bc_parse_push(p, XC_INST_SCALE_FUNC);

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_scale(...) (zbc_parse_scale(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_incdec(BcParse *p, BcInst *prev, bool *paren_expr,
                                size_t *nexprs, uint8_t flags)
{
	BcStatus s;
	BcLexType type;
	char inst;
	BcInst etype = *prev;

	if (etype == XC_INST_VAR || etype == XC_INST_ARRAY_ELEM
	 || etype == XC_INST_SCALE || etype == BC_INST_LAST
	 || etype == XC_INST_IBASE || etype == XC_INST_OBASE
	) {
		*prev = inst = BC_INST_INC_POST + (p->l.lex != BC_LEX_OP_INC);
		bc_parse_push(p, inst);
		s = zbc_lex_next(&p->l);
	} else {
		*prev = inst = BC_INST_INC_PRE + (p->l.lex != BC_LEX_OP_INC);
		*paren_expr = true;

		s = zbc_lex_next(&p->l);
		if (s) RETURN_STATUS(s);
		type = p->l.lex;

		// Because we parse the next part of the expression
		// right here, we need to increment this.
		*nexprs = *nexprs + 1;

		switch (type) {
			case XC_LEX_NAME:
				s = zbc_parse_name(p, prev, flags | BC_PARSE_NOCALL);
				break;
			case BC_LEX_KEY_IBASE:
			case BC_LEX_KEY_LAST:
			case BC_LEX_KEY_OBASE:
				bc_parse_push(p, type - BC_LEX_KEY_IBASE + XC_INST_IBASE);
				s = zbc_lex_next(&p->l);
				break;
			case BC_LEX_KEY_SCALE:
				s = zbc_lex_next(&p->l);
				if (s) RETURN_STATUS(s);
				if (p->l.lex == BC_LEX_LPAREN)
					s = bc_error_bad_token();
				else
					bc_parse_push(p, XC_INST_SCALE);
				break;
			default:
				s = bc_error_bad_token();
				break;
		}

		if (!s) bc_parse_push(p, inst);
	}

	RETURN_STATUS(s);
}
#define zbc_parse_incdec(...) (zbc_parse_incdec(__VA_ARGS__) COMMA_SUCCESS)

#if 0
#define BC_PARSE_LEAF(p, rparen) \
	((rparen) \
	 || ((p) >= XC_INST_NUM && (p) <= XC_INST_SQRT) \
	 || (p) == BC_INST_INC_POST \
	 || (p) == BC_INST_DEC_POST \
	)
#else
static int ok_in_expr(BcInst p)
{
	return (p >= XC_INST_NUM && p <= XC_INST_SQRT)
		|| p == BC_INST_INC_POST
		|| p == BC_INST_DEC_POST
		;
}
#define BC_PARSE_LEAF(p, rparen) ((rparen) || ok_in_expr(p))
#endif

static BC_STATUS zbc_parse_minus(BcParse *p, BcInst *prev, size_t ops_bgn,
                               bool rparen, size_t *nexprs)
{
	BcStatus s;
	BcLexType type;
	BcInst etype = *prev;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	type = BC_PARSE_LEAF(etype, rparen) ? XC_LEX_OP_MINUS : XC_LEX_NEG;
	*prev = BC_TOKEN_2_INST(type);

	// We can just push onto the op stack because this is the largest
	// precedence operator that gets pushed. Inc/dec does not.
	if (type != XC_LEX_OP_MINUS)
		bc_vec_push(&p->ops, &type);
	else
		bc_parse_operator(p, type, ops_bgn, nexprs);

	RETURN_STATUS(s);
}
#define zbc_parse_minus(...) (zbc_parse_minus(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_print(BcParse *p)
{
	BcStatus s;
	BcLexType type;

	for (;;) {
		s = zbc_lex_next(&p->l);
		if (s) RETURN_STATUS(s);
		type = p->l.lex;
		if (type == XC_LEX_STR) {
			s = zbc_parse_pushSTR(p);
		} else {
			s = zbc_parse_expr(p, 0);
		}
		if (s) RETURN_STATUS(s);
		bc_parse_push(p, XC_INST_PRINT_POP);
		if (p->l.lex != BC_LEX_COMMA)
			break;
	}

	RETURN_STATUS(s);
}
#define zbc_parse_print(...) (zbc_parse_print(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_return(BcParse *p)
{
	BcStatus s;
	BcLexType t;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	t = p->l.lex;
	if (t == XC_LEX_NLINE || t == BC_LEX_SCOLON)
		bc_parse_push(p, BC_INST_RET0);
	else {
		bool paren = (t == BC_LEX_LPAREN);
		s = bc_parse_expr_empty_ok(p, 0);
		if (s == BC_STATUS_PARSE_EMPTY_EXP) {
			bc_parse_push(p, BC_INST_RET0);
			s = zbc_lex_next(&p->l);
		}
		if (s) RETURN_STATUS(s);

		if (!paren || p->l.lex_last != BC_LEX_RPAREN) {
			s = zbc_POSIX_requires("parentheses around return expressions");
			if (s) RETURN_STATUS(s);
		}

		bc_parse_push(p, XC_INST_RET);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_return(...) (zbc_parse_return(__VA_ARGS__) COMMA_SUCCESS)

static void rewrite_label_to_current(BcParse *p, size_t idx)
{
	size_t *label = bc_vec_item(&p->func->labels, idx);
	*label = p->func->code.len;
}

static BC_STATUS zbc_parse_if(BcParse *p)
{
	BcStatus s;
	size_t ip_idx;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	s = zbc_parse_expr(p, BC_PARSE_REL);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	// Encode "if zero, jump to ..."
	// Pushed value (destination of the jump) is uninitialized,
	// will be rewritten to be address of "end of if()" or of "else".
	ip_idx = bc_vec_push(&p->func->labels, &ip_idx);
	bc_parse_pushJUMP_ZERO(p, ip_idx);

	s = zbc_parse_stmt_allow_NLINE_before(p, STRING_if);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d in if after stmt: p->l.lex:%d", __func__, __LINE__, p->l.lex);
	if (p->l.lex == BC_LEX_KEY_ELSE) {
		size_t ip2_idx;

		// Encode "after then_stmt, jump to end of if()"
		ip2_idx = bc_vec_push(&p->func->labels, &ip2_idx);
		dbg_lex("%s:%d after if() then_stmt: BC_INST_JUMP to %zd", __func__, __LINE__, ip2_idx);
		bc_parse_pushJUMP(p, ip2_idx);

		dbg_lex("%s:%d rewriting 'if_zero' label to jump to 'else'-> %zd", __func__, __LINE__, p->func->code.len);
		rewrite_label_to_current(p, ip_idx);

		ip_idx = ip2_idx;

		s = zbc_parse_stmt_allow_NLINE_before(p, STRING_else);
		if (s) RETURN_STATUS(s);
	}

	dbg_lex("%s:%d rewriting label to jump after 'if' body-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(p, ip_idx);

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_if(...) (zbc_parse_if(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_while(BcParse *p)
{
	BcStatus s;
	size_t cond_idx;
	size_t ip_idx;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	cond_idx = bc_vec_push(&p->func->labels, &p->func->code.len);
	ip_idx = cond_idx + 1;
	bc_vec_push(&p->conds, &cond_idx);

	bc_vec_push(&p->exits, &ip_idx);
	bc_vec_push(&p->func->labels, &ip_idx);

	s = zbc_parse_expr(p, BC_PARSE_REL);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());

	bc_parse_pushJUMP_ZERO(p, ip_idx);

	s = zbc_parse_stmt_allow_NLINE_before(p, STRING_while);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d BC_INST_JUMP to %zd", __func__, __LINE__, cond_idx);
	bc_parse_pushJUMP(p, cond_idx);

	dbg_lex("%s:%d rewriting label-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(p, ip_idx);

	bc_vec_pop(&p->exits);
	bc_vec_pop(&p->conds);

	RETURN_STATUS(s);
}
#define zbc_parse_while(...) (zbc_parse_while(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_for(BcParse *p)
{
	BcStatus s;
	size_t cond_idx, exit_idx, body_idx, update_idx;

	dbg_lex("%s:%d p->l.lex:%d", __func__, __LINE__, p->l.lex);
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN) RETURN_STATUS(bc_error_bad_token());
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_SCOLON) {
		s = zbc_parse_expr(p, 0);
		bc_parse_push(p, XC_INST_POP);
		if (s) RETURN_STATUS(s);
	} else {
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("init");
		if (s) RETURN_STATUS(s);
	}

	if (p->l.lex != BC_LEX_SCOLON) RETURN_STATUS(bc_error_bad_token());
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	cond_idx = bc_vec_push(&p->func->labels, &p->func->code.len);
	update_idx = cond_idx + 1;
	body_idx = update_idx + 1;
	exit_idx = body_idx + 1;

	if (p->l.lex != BC_LEX_SCOLON)
		s = zbc_parse_expr(p, BC_PARSE_REL);
	else {
		// Set this for the next call to bc_parse_pushNUM().
		// This is safe to set because the current token is a semicolon,
		// which has no string requirement.
		bc_vec_string(&p->l.lex_buf, 1, "1");
		bc_parse_pushNUM(p);
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("condition");
	}
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_SCOLON) RETURN_STATUS(bc_error_bad_token());

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	bc_parse_pushJUMP_ZERO(p, exit_idx);
	bc_parse_pushJUMP(p, body_idx);

	bc_vec_push(&p->conds, &update_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	if (p->l.lex != BC_LEX_RPAREN) {
		s = zbc_parse_expr(p, 0);
		if (s) RETURN_STATUS(s);
		if (p->l.lex != BC_LEX_RPAREN) RETURN_STATUS(bc_error_bad_token());
		bc_parse_push(p, XC_INST_POP);
	} else {
		s = zbc_POSIX_does_not_allow_empty_X_expression_in_for("update");
		if (s) RETURN_STATUS(s);
	}

	bc_parse_pushJUMP(p, cond_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	bc_vec_push(&p->exits, &exit_idx);
	bc_vec_push(&p->func->labels, &exit_idx);

	s = zbc_parse_stmt_allow_NLINE_before(p, STRING_for);
	if (s) RETURN_STATUS(s);

	dbg_lex("%s:%d BC_INST_JUMP to %zd", __func__, __LINE__, update_idx);
	bc_parse_pushJUMP(p, update_idx);

	dbg_lex("%s:%d rewriting label-> %zd", __func__, __LINE__, p->func->code.len);
	rewrite_label_to_current(p, exit_idx);

	bc_vec_pop(&p->exits);
	bc_vec_pop(&p->conds);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_parse_for(...) (zbc_parse_for(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_break_or_continue(BcParse *p, BcLexType type)
{
	BcStatus s;
	size_t i;

	if (type == BC_LEX_KEY_BREAK) {
		if (p->exits.len == 0) // none of the enclosing blocks is a loop
			RETURN_STATUS(bc_error_bad_token());
		i = *(size_t*)bc_vec_top(&p->exits);
	} else {
		i = *(size_t*)bc_vec_top(&p->conds);
	}

	bc_parse_pushJUMP(p, i);

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_SCOLON && p->l.lex != XC_LEX_NLINE)
		RETURN_STATUS(bc_error_bad_token());

	RETURN_STATUS(zbc_lex_next(&p->l));
}
#define zbc_parse_break_or_continue(...) (zbc_parse_break_or_continue(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_func_insert(BcFunc *f, char *name, bool var)
{
	BcId *autoid;
	BcId a;
	size_t i;

	autoid = (void*)f->autos.v;
	for (i = 0; i < f->autos.len; i++, autoid++) {
		if (strcmp(name, autoid->name) == 0)
			RETURN_STATUS(bc_error("function parameter or auto var has the same name as another"));
	}

	a.idx = var;
	a.name = name;

	bc_vec_push(&f->autos, &a);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_func_insert(...) (zbc_func_insert(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_funcdef(BcParse *p)
{
	BcStatus s;
	bool var, comma = false;
	char *name;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != XC_LEX_NAME)
		RETURN_STATUS(bc_error("bad function definition"));

	name = xstrdup(p->l.lex_buf.v);
	p->fidx = bc_program_addFunc(name);
	p->func = bc_program_func(p->fidx);

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != BC_LEX_LPAREN)
		RETURN_STATUS(bc_error("bad function definition"));
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	while (p->l.lex != BC_LEX_RPAREN) {
		if (p->l.lex != XC_LEX_NAME)
			RETURN_STATUS(bc_error("bad function definition"));

		++p->func->nparams;

		name = xstrdup(p->l.lex_buf.v);
		s = zbc_lex_next(&p->l);
		if (s) goto err;

		var = p->l.lex != BC_LEX_LBRACKET;

		if (!var) {
			s = zbc_lex_next(&p->l);
			if (s) goto err;

			if (p->l.lex != BC_LEX_RBRACKET) {
				s = bc_error("bad function definition");
				goto err;
			}

			s = zbc_lex_next(&p->l);
			if (s) goto err;
		}

		comma = p->l.lex == BC_LEX_COMMA;
		if (comma) {
			s = zbc_lex_next(&p->l);
			if (s) goto err;
		}

		s = zbc_func_insert(p->func, name, var);
		if (s) goto err;
	}

	if (comma) RETURN_STATUS(bc_error("bad function definition"));

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	if (p->l.lex != BC_LEX_LBRACE) {
		s = zbc_POSIX_requires("the left brace be on the same line as the function header");
		if (s) RETURN_STATUS(s);
	}

	// Prevent "define z()<newline>" from being interpreted as function with empty stmt as body
	s = zbc_lex_skip_if_at_NLINE(&p->l);
	if (s) RETURN_STATUS(s);
	//GNU bc requires a {} block even if function body has single stmt, enforce this?
	if (p->l.lex != BC_LEX_LBRACE)
		RETURN_STATUS(bc_error("function { body } expected"));

	p->in_funcdef++; // to determine whether "return" stmt is allowed, and such
	s = zbc_parse_stmt_possibly_auto(p, true);
	p->in_funcdef--;
	if (s) RETURN_STATUS(s);

	bc_parse_push(p, BC_INST_RET0);

	// Subsequent code generation is into main program
	p->fidx = BC_PROG_MAIN;
	p->func = bc_program_func_BC_PROG_MAIN();

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
 err:
	dbg_lex_done("%s:%d done (error)", __func__, __LINE__);
	free(name);
	RETURN_STATUS(s);
}
#define zbc_parse_funcdef(...) (zbc_parse_funcdef(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_auto(BcParse *p)
{
	BcStatus s;
	bool comma, var, one;
	char *name;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	comma = false;
	one = p->l.lex == XC_LEX_NAME;

	while (p->l.lex == XC_LEX_NAME) {
		name = xstrdup(p->l.lex_buf.v);
		s = zbc_lex_next(&p->l);
		if (s) goto err;

		var = p->l.lex != BC_LEX_LBRACKET;
		if (!var) {
			s = zbc_lex_next(&p->l);
			if (s) goto err;

			if (p->l.lex != BC_LEX_RBRACKET) {
				s = bc_error("bad function definition");
				goto err;
			}

			s = zbc_lex_next(&p->l);
			if (s) goto err;
		}

		comma = p->l.lex == BC_LEX_COMMA;
		if (comma) {
			s = zbc_lex_next(&p->l);
			if (s) goto err;
		}

		s = zbc_func_insert(p->func, name, var);
		if (s) goto err;
	}

	if (comma) RETURN_STATUS(bc_error("bad function definition"));
	if (!one) RETURN_STATUS(bc_error("no auto variable found"));

	if (p->l.lex != XC_LEX_NLINE && p->l.lex != BC_LEX_SCOLON)
		RETURN_STATUS(bc_error_bad_token());

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(zbc_lex_next(&p->l));
 err:
	free(name);
	dbg_lex_done("%s:%d done (ERROR)", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_auto(...) (zbc_parse_auto(__VA_ARGS__) COMMA_SUCCESS)

#undef zbc_parse_stmt_possibly_auto
static BC_STATUS zbc_parse_stmt_possibly_auto(BcParse *p, bool auto_allowed)
{
	BcStatus s = BC_STATUS_SUCCESS;

	dbg_lex_enter("%s:%d entered, p->l.lex:%d", __func__, __LINE__, p->l.lex);

	if (p->l.lex == XC_LEX_NLINE) {
		dbg_lex_done("%s:%d done (seen XC_LEX_NLINE)", __func__, __LINE__);
		RETURN_STATUS(zbc_lex_next(&p->l));
	}
	if (p->l.lex == BC_LEX_SCOLON) {
		dbg_lex_done("%s:%d done (seen BC_LEX_SCOLON)", __func__, __LINE__);
		RETURN_STATUS(zbc_lex_next(&p->l));
	}

	if (p->l.lex == BC_LEX_LBRACE) {
		dbg_lex("%s:%d BC_LEX_LBRACE: (auto_allowed:%d)", __func__, __LINE__, auto_allowed);
		do {
			s = zbc_lex_next(&p->l);
			if (s) RETURN_STATUS(s);
		} while (p->l.lex == XC_LEX_NLINE);
		if (auto_allowed && p->l.lex == BC_LEX_KEY_AUTO) {
			dbg_lex("%s:%d calling zbc_parse_auto()", __func__, __LINE__);
			s = zbc_parse_auto(p);
			if (s) RETURN_STATUS(s);
		}
		while (p->l.lex != BC_LEX_RBRACE) {
			dbg_lex("%s:%d block parsing loop", __func__, __LINE__);
			s = zbc_parse_stmt(p);
			if (s) RETURN_STATUS(s);
		}
		s = zbc_lex_next(&p->l);
		dbg_lex_done("%s:%d done (seen BC_LEX_RBRACE)", __func__, __LINE__);
		RETURN_STATUS(s);
	}

	dbg_lex("%s:%d p->l.lex:%d", __func__, __LINE__, p->l.lex);
	switch (p->l.lex) {
		case XC_LEX_OP_MINUS:
		case BC_LEX_OP_INC:
		case BC_LEX_OP_DEC:
		case BC_LEX_OP_BOOL_NOT:
		case BC_LEX_LPAREN:
		case XC_LEX_NAME:
		case XC_LEX_NUMBER:
		case BC_LEX_KEY_IBASE:
		case BC_LEX_KEY_LAST:
		case BC_LEX_KEY_LENGTH:
		case BC_LEX_KEY_OBASE:
		case BC_LEX_KEY_READ:
		case BC_LEX_KEY_SCALE:
		case BC_LEX_KEY_SQRT:
			s = zbc_parse_expr(p, BC_PARSE_PRINT);
			break;
		case XC_LEX_STR:
			s = zbc_parse_pushSTR(p);
			bc_parse_push(p, XC_INST_PRINT_STR);
			break;
		case BC_LEX_KEY_BREAK:
		case BC_LEX_KEY_CONTINUE:
			s = zbc_parse_break_or_continue(p, p->l.lex);
			break;
		case BC_LEX_KEY_FOR:
			s = zbc_parse_for(p);
			break;
		case BC_LEX_KEY_HALT:
			bc_parse_push(p, BC_INST_HALT);
			s = zbc_lex_next(&p->l);
			break;
		case BC_LEX_KEY_IF:
			s = zbc_parse_if(p);
			break;
		case BC_LEX_KEY_LIMITS:
			// "limits" is a compile-time command,
			// the output is produced at _parse time_.
			printf(
				"BC_BASE_MAX     = "BC_MAX_OBASE_STR "\n"
				"BC_DIM_MAX      = "BC_MAX_DIM_STR   "\n"
				"BC_SCALE_MAX    = "BC_MAX_SCALE_STR "\n"
				"BC_STRING_MAX   = "BC_MAX_STRING_STR"\n"
				"BC_NAME_MAX     = "BC_MAX_NAME_STR  "\n"
				"BC_NUM_MAX      = "BC_MAX_NUM_STR   "\n"
				"MAX Exponent    = "BC_MAX_EXP_STR   "\n"
				"Number of vars  = "BC_MAX_VARS_STR  "\n"
			);
			s = zbc_lex_next(&p->l);
			break;
		case BC_LEX_KEY_PRINT:
			s = zbc_parse_print(p);
			break;
		case BC_LEX_KEY_QUIT:
			// "quit" is a compile-time command. For example,
			// "if (0 == 1) quit" terminates when parsing the statement,
			// not when it is executed
			QUIT_OR_RETURN_TO_MAIN;
		case BC_LEX_KEY_RETURN:
			if (!p->in_funcdef)
				RETURN_STATUS(bc_error("'return' not in a function"));
			s = zbc_parse_return(p);
			break;
		case BC_LEX_KEY_WHILE:
			s = zbc_parse_while(p);
			break;
		default:
			s = bc_error_bad_token();
			break;
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_stmt_possibly_auto(...) (zbc_parse_stmt_possibly_auto(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_parse_stmt_or_funcdef(BcParse *p)
{
	BcStatus s;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	if (p->l.lex == XC_LEX_EOF)
		s = bc_error("end of file");
	else if (p->l.lex == BC_LEX_KEY_DEFINE) {
		dbg_lex("%s:%d p->l.lex:BC_LEX_KEY_DEFINE", __func__, __LINE__);
		s = zbc_parse_funcdef(p);
	} else {
		dbg_lex("%s:%d p->l.lex:%d (not BC_LEX_KEY_DEFINE)", __func__, __LINE__, p->l.lex);
		s = zbc_parse_stmt(p);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_parse_stmt_or_funcdef(...) (zbc_parse_stmt_or_funcdef(__VA_ARGS__) COMMA_SUCCESS)

// This is not a "z" function: can also return BC_STATUS_PARSE_EMPTY_EXP
static BcStatus bc_parse_expr_empty_ok(BcParse *p, uint8_t flags)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcInst prev = XC_INST_PRINT;
	BcLexType top, t = p->l.lex;
	size_t nexprs = 0, ops_bgn = p->ops.len;
	unsigned nparens, nrelops;
	bool paren_first, paren_expr, rprn, done, get_token, assign, bin_last;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	paren_first = (p->l.lex == BC_LEX_LPAREN);
	nparens = nrelops = 0;
	paren_expr = rprn = done = get_token = assign = false;
	bin_last = true;

	for (; !G_interrupt && !s && !done && bc_parse_exprs(t); t = p->l.lex) {
		dbg_lex("%s:%d t:%d", __func__, __LINE__, t);
		switch (t) {
			case BC_LEX_OP_INC:
			case BC_LEX_OP_DEC:
				s = zbc_parse_incdec(p, &prev, &paren_expr, &nexprs, flags);
				rprn = get_token = bin_last = false;
				break;
			case XC_LEX_OP_MINUS:
				s = zbc_parse_minus(p, &prev, ops_bgn, rprn, &nexprs);
				rprn = get_token = false;
				bin_last = (prev == XC_INST_MINUS);
				break;
			case BC_LEX_OP_ASSIGN_POWER:
			case BC_LEX_OP_ASSIGN_MULTIPLY:
			case BC_LEX_OP_ASSIGN_DIVIDE:
			case BC_LEX_OP_ASSIGN_MODULUS:
			case BC_LEX_OP_ASSIGN_PLUS:
			case BC_LEX_OP_ASSIGN_MINUS:
			case BC_LEX_OP_ASSIGN:
				if (prev != XC_INST_VAR && prev != XC_INST_ARRAY_ELEM
				 && prev != XC_INST_SCALE && prev != XC_INST_IBASE
				 && prev != XC_INST_OBASE && prev != BC_INST_LAST
				) {
					s = bc_error("bad assignment:"
						" left side must be variable"
						" or array element"
					); // note: shared string
					break;
				}
			// Fallthrough.
			case XC_LEX_OP_POWER:
			case XC_LEX_OP_MULTIPLY:
			case XC_LEX_OP_DIVIDE:
			case XC_LEX_OP_MODULUS:
			case XC_LEX_OP_PLUS:
			case XC_LEX_OP_REL_EQ:
			case XC_LEX_OP_REL_LE:
			case XC_LEX_OP_REL_GE:
			case XC_LEX_OP_REL_NE:
			case XC_LEX_OP_REL_LT:
			case XC_LEX_OP_REL_GT:
			case BC_LEX_OP_BOOL_NOT:
			case BC_LEX_OP_BOOL_OR:
			case BC_LEX_OP_BOOL_AND:
				if (((t == BC_LEX_OP_BOOL_NOT) != bin_last)
				 || (t != BC_LEX_OP_BOOL_NOT && prev == XC_INST_BOOL_NOT)
				) {
					return bc_error_bad_expression();
				}
				nrelops += (t >= XC_LEX_OP_REL_EQ && t <= XC_LEX_OP_REL_GT);
				prev = BC_TOKEN_2_INST(t);
				bc_parse_operator(p, t, ops_bgn, &nexprs);
				s = zbc_lex_next(&p->l);
				rprn = get_token = false;
				bin_last = (t != BC_LEX_OP_BOOL_NOT);
				break;
			case BC_LEX_LPAREN:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				nparens++;
				paren_expr = rprn = bin_last = false;
				get_token = true;
				bc_vec_push(&p->ops, &t);
				break;
			case BC_LEX_RPAREN:
				if (bin_last || prev == XC_INST_BOOL_NOT)
					return bc_error_bad_expression();
				if (nparens == 0) {
					s = BC_STATUS_SUCCESS;
					done = true;
					get_token = false;
					break;
				}
				if (!paren_expr) {
					dbg_lex_done("%s:%d done (returning EMPTY_EXP)", __func__, __LINE__);
					return BC_STATUS_PARSE_EMPTY_EXP;
				}
				nparens--;
				paren_expr = rprn = true;
				get_token = bin_last = false;
				s = zbc_parse_rightParen(p, ops_bgn, &nexprs);
				break;
			case XC_LEX_NAME:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				paren_expr = true;
				rprn = get_token = bin_last = false;
				s = zbc_parse_name(p, &prev, flags & ~BC_PARSE_NOCALL);
				++nexprs;
				break;
			case XC_LEX_NUMBER:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				bc_parse_pushNUM(p);
				nexprs++;
				prev = XC_INST_NUM;
				paren_expr = get_token = true;
				rprn = bin_last = false;
				break;
			case BC_LEX_KEY_IBASE:
			case BC_LEX_KEY_LAST:
			case BC_LEX_KEY_OBASE:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				prev = (char) (t - BC_LEX_KEY_IBASE + XC_INST_IBASE);
				bc_parse_push(p, (char) prev);
				paren_expr = get_token = true;
				rprn = bin_last = false;
				nexprs++;
				break;
			case BC_LEX_KEY_LENGTH:
			case BC_LEX_KEY_SQRT:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				s = zbc_parse_builtin(p, t, flags, &prev);
				paren_expr = true;
				rprn = get_token = bin_last = false;
				nexprs++;
				break;
			case BC_LEX_KEY_READ:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				s = zbc_parse_read(p);
				paren_expr = true;
				rprn = get_token = bin_last = false;
				nexprs++;
				prev = XC_INST_READ;
				break;
			case BC_LEX_KEY_SCALE:
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				s = zbc_parse_scale(p, &prev, flags);
				paren_expr = true;
				rprn = get_token = bin_last = false;
				nexprs++;
				prev = XC_INST_SCALE;
				break;
			default:
				s = bc_error_bad_token();
				break;
		}

		if (!s && get_token) s = zbc_lex_next(&p->l);
	}

	if (s) return s;
	if (G_interrupt) return BC_STATUS_FAILURE; // ^C: stop parsing

	while (p->ops.len > ops_bgn) {
		top = BC_PARSE_TOP_OP(p);
		assign = (top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN);

		if (top == BC_LEX_LPAREN || top == BC_LEX_RPAREN)
			return bc_error_bad_expression();

		bc_parse_push(p, BC_TOKEN_2_INST(top));

		nexprs -= (top != BC_LEX_OP_BOOL_NOT && top != XC_LEX_NEG);
		bc_vec_pop(&p->ops);
	}

	if (prev == XC_INST_BOOL_NOT || nexprs != 1)
		return bc_error_bad_expression();

	if (!(flags & BC_PARSE_REL) && nrelops) {
		s = zbc_POSIX_does_not_allow("comparison operators outside if or loops");
		if (s) return s;
	} else if ((flags & BC_PARSE_REL) && nrelops > 1) {
		s = zbc_POSIX_requires("exactly one comparison operator per condition");
		if (s) return s;
	}

	if (flags & BC_PARSE_PRINT) {
		if (paren_first || !assign) bc_parse_push(p, XC_INST_PRINT);
		bc_parse_push(p, XC_INST_POP);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	return s;
}

#endif // ENABLE_BC

#if ENABLE_DC

static BC_STATUS zdc_parse_register(BcParse *p)
{
	BcStatus s;

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);
	if (p->l.lex != XC_LEX_NAME) RETURN_STATUS(bc_error_bad_token());

	bc_parse_pushName(p, p->l.lex_buf.v);

	RETURN_STATUS(s);
}
#define zdc_parse_register(...) (zdc_parse_register(__VA_ARGS__) COMMA_SUCCESS)

static void dc_parse_string(BcParse *p)
{
	char *str;
	size_t len = G.prog.strs.len;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);

	str = xstrdup(p->l.lex_buf.v);
	bc_parse_push(p, XC_INST_STR);
	bc_parse_pushIndex(p, len);
	bc_vec_push(&G.prog.strs, &str);

	// Explanation needed here
	bc_program_add_fn();
	p->func = bc_program_func(p->fidx);

	dbg_lex_done("%s:%d done", __func__, __LINE__);
}

static BC_STATUS zdc_parse_mem(BcParse *p, uint8_t inst, bool name, bool store)
{
	BcStatus s;

	bc_parse_push(p, inst);
	if (name) {
		s = zdc_parse_register(p);
		if (s) RETURN_STATUS(s);
	}

	if (store) {
		bc_parse_push(p, DC_INST_SWAP);
		bc_parse_push(p, XC_INST_ASSIGN);
		bc_parse_push(p, XC_INST_POP);
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_parse_mem(...) (zdc_parse_mem(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_cond(BcParse *p, uint8_t inst)
{
	BcStatus s;

	bc_parse_push(p, inst);
	bc_parse_push(p, DC_INST_EXEC_COND);

	s = zdc_parse_register(p);
	if (s) RETURN_STATUS(s);

	s = zbc_lex_next(&p->l);
	if (s) RETURN_STATUS(s);

	// Note that 'else' part can not be on the next line:
	// echo -e '[1p]sa [2p]sb 2 1>a eb' | dc - OK, prints "2"
	// echo -e '[1p]sa [2p]sb 2 1>a\neb' | dc - parse error
	if (p->l.lex == DC_LEX_ELSE) {
		s = zdc_parse_register(p);
		if (s) RETURN_STATUS(s);
		s = zbc_lex_next(&p->l);
	} else {
		bc_parse_push(p, BC_PARSE_STREND);
	}

	RETURN_STATUS(s);
}
#define zdc_parse_cond(...) (zdc_parse_cond(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_token(BcParse *p, BcLexType t)
{
	BcStatus s;
	uint8_t inst;
	bool assign, get_token;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = BC_STATUS_SUCCESS;
	get_token = true;
	switch (t) {
		case XC_LEX_OP_REL_EQ:
		case XC_LEX_OP_REL_LE:
		case XC_LEX_OP_REL_GE:
		case XC_LEX_OP_REL_NE:
		case XC_LEX_OP_REL_LT:
		case XC_LEX_OP_REL_GT:
			dbg_lex("%s:%d LEX_OP_REL_xyz", __func__, __LINE__);
			s = zdc_parse_cond(p, t - XC_LEX_OP_REL_EQ + XC_INST_REL_EQ);
			get_token = false;
			break;
		case DC_LEX_SCOLON:
		case DC_LEX_COLON:
			dbg_lex("%s:%d LEX_[S]COLON", __func__, __LINE__);
			s = zdc_parse_mem(p, XC_INST_ARRAY_ELEM, true, t == DC_LEX_COLON);
			break;
		case XC_LEX_STR:
			dbg_lex("%s:%d LEX_STR", __func__, __LINE__);
			dc_parse_string(p);
			break;
		case XC_LEX_NEG:
			dbg_lex("%s:%d LEX_NEG", __func__, __LINE__);
			s = zbc_lex_next(&p->l);
			if (s) RETURN_STATUS(s);
			if (p->l.lex != XC_LEX_NUMBER)
				RETURN_STATUS(bc_error_bad_token());
			bc_parse_pushNUM(p);
			bc_parse_push(p, XC_INST_NEG);
			break;
		case XC_LEX_NUMBER:
			dbg_lex("%s:%d LEX_NUMBER", __func__, __LINE__);
			bc_parse_pushNUM(p);
			break;
		case DC_LEX_READ:
			dbg_lex("%s:%d LEX_KEY_READ", __func__, __LINE__);
			bc_parse_push(p, XC_INST_READ);
			break;
		case DC_LEX_OP_ASSIGN:
		case DC_LEX_STORE_PUSH:
			dbg_lex("%s:%d LEX_OP_ASSIGN/STORE_PUSH", __func__, __LINE__);
			assign = (t == DC_LEX_OP_ASSIGN);
			inst = assign ? XC_INST_VAR : DC_INST_PUSH_TO_VAR;
			s = zdc_parse_mem(p, inst, true, assign);
			break;
		case DC_LEX_LOAD:
		case DC_LEX_LOAD_POP:
			dbg_lex("%s:%d LEX_OP_LOAD[_POP]", __func__, __LINE__);
			inst = t == DC_LEX_LOAD_POP ? DC_INST_PUSH_VAR : DC_INST_LOAD;
			s = zdc_parse_mem(p, inst, true, false);
			break;
		case DC_LEX_STORE_IBASE:
		case DC_LEX_STORE_SCALE:
		case DC_LEX_STORE_OBASE:
			dbg_lex("%s:%d LEX_OP_STORE_I/OBASE/SCALE", __func__, __LINE__);
			inst = t - DC_LEX_STORE_IBASE + XC_INST_IBASE;
			s = zdc_parse_mem(p, inst, false, true);
			break;
		default:
			dbg_lex_done("%s:%d done (bad token)", __func__, __LINE__);
			RETURN_STATUS(bc_error_bad_token());
	}

	if (!s && get_token) s = zbc_lex_next(&p->l);

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zdc_parse_token(...) (zdc_parse_token(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_expr(BcParse *p)
{
	int i;

	i = (int)p->l.lex - (int)XC_LEX_OP_POWER;
	if (i >= 0) {
		BcInst inst = dc_LEX_to_INST[i];
		if (inst != DC_INST_INVALID) {
			bc_parse_push(p, inst);
			RETURN_STATUS(zbc_lex_next(&p->l));
		}
	}
	RETURN_STATUS(zdc_parse_token(p, p->l.lex));
}
#define zdc_parse_expr(...) (zdc_parse_expr(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_parse_exprs_until_eof(BcParse *p)
{
	dbg_lex_enter("%s:%d entered, p->l.lex:%d", __func__, __LINE__, p->l.lex);
	while (p->l.lex != XC_LEX_EOF) {
		BcStatus s = zdc_parse_expr(p);
		if (s) RETURN_STATUS(s);
	}

	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_parse_exprs_until_eof(...) (zdc_parse_exprs_until_eof(__VA_ARGS__) COMMA_SUCCESS)

#endif // ENABLE_DC

//
// Execution engine
//

#define STACK_HAS_MORE_THAN(s, n)          ((s)->len > ((size_t)(n)))
#define STACK_HAS_EQUAL_OR_MORE_THAN(s, n) ((s)->len >= ((size_t)(n)))

static BcVec* bc_program_search(char *id, bool var)
{
	BcId e, *ptr;
	BcVec *v, *map;
	size_t i;
	int new;

	v = var ? &G.prog.vars : &G.prog.arrs;
	map = var ? &G.prog.var_map : &G.prog.arr_map;

	e.name = id;
	e.idx = v->len;
	new = bc_map_insert(map, &e, &i); // 1 if insertion was successful

	if (new) {
		BcVec v2;
		bc_array_init(&v2, var);
		bc_vec_push(v, &v2);
	}

	ptr = bc_vec_item(map, i);
	if (new) ptr->name = xstrdup(e.name);
	return bc_vec_item(v, ptr->idx);
}

// 'num' need not be initialized on entry
static BC_STATUS zbc_program_num(BcResult *r, BcNum **num, bool hex)
{
	switch (r->t) {
		case BC_RESULT_STR:
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
			*num = &r->d.n;
			break;
		case BC_RESULT_CONSTANT: {
			BcStatus s;
			char *str;
			unsigned base_t;
			size_t len;

			str = *bc_program_const(r->d.id.idx);
			len = strlen(str);

			bc_num_init(&r->d.n, len);

			hex = hex && len == 1;
			base_t = hex ? 16 : G.prog.ib_t;
			s = zbc_num_parse(&r->d.n, str, base_t);
			if (s) {
				bc_num_free(&r->d.n);
				RETURN_STATUS(s);
			}
			*num = &r->d.n;
			r->t = BC_RESULT_TEMP;
			break;
		}
		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM: {
			BcVec *v;

			v = bc_program_search(r->d.id.name, r->t == BC_RESULT_VAR);

			if (r->t == BC_RESULT_ARRAY_ELEM) {
				v = bc_vec_top(v);
				if (v->len <= r->d.id.idx) bc_array_expand(v, r->d.id.idx + 1);
				*num = bc_vec_item(v, r->d.id.idx);
			} else
				*num = bc_vec_top(v);
			break;
		}
#if ENABLE_BC
		case BC_RESULT_LAST:
			*num = &G.prog.last;
			break;
		case BC_RESULT_ONE:
			*num = &G.prog.one;
			break;
#endif
#if SANITY_CHECKS
		default:
			// Testing the theory that dc does not reach LAST/ONE
			bb_error_msg_and_die("BUG:%d", r->t);
#endif
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_num(...) (zbc_program_num(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_binOpPrep(BcResult **l, BcNum **ln,
                                     BcResult **r, BcNum **rn, bool assign)
{
	BcStatus s;
	bool hex;
	BcResultType lt, rt;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	*r = bc_vec_item_rev(&G.prog.results, 0);
	*l = bc_vec_item_rev(&G.prog.results, 1);

	lt = (*l)->t;
	rt = (*r)->t;
	hex = assign && (lt == BC_RESULT_IBASE || lt == BC_RESULT_OBASE);

	s = zbc_program_num(*l, ln, false);
	if (s) RETURN_STATUS(s);
	s = zbc_program_num(*r, rn, hex);
	if (s) RETURN_STATUS(s);

	// We run this again under these conditions in case any vector has been
	// reallocated out from under the BcNums or arrays we had.
	if (lt == rt && (lt == BC_RESULT_VAR || lt == BC_RESULT_ARRAY_ELEM)) {
		s = zbc_program_num(*l, ln, false);
		if (s) RETURN_STATUS(s);
	}

	if (!BC_PROG_NUM((*l), (*ln)) && (!assign || (*l)->t != BC_RESULT_VAR))
		RETURN_STATUS(bc_error_variable_is_wrong_type());
	if (!assign && !BC_PROG_NUM((*r), (*ln)))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	RETURN_STATUS(s);
}
#define zbc_program_binOpPrep(...) (zbc_program_binOpPrep(__VA_ARGS__) COMMA_SUCCESS)

static void bc_program_binOpRetire(BcResult *r)
{
	r->t = BC_RESULT_TEMP;
	bc_vec_pop(&G.prog.results);
	bc_result_pop_and_push(r);
}

// Note: *r and *n need not be initialized by caller
static BC_STATUS zbc_program_prep(BcResult **r, BcNum **n)
{
	BcStatus s;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	*r = bc_vec_top(&G.prog.results);

	s = zbc_program_num(*r, n, false);
	if (s) RETURN_STATUS(s);

	if (!BC_PROG_NUM((*r), (*n)))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	RETURN_STATUS(s);
}
#define zbc_program_prep(...) (zbc_program_prep(__VA_ARGS__) COMMA_SUCCESS)

static void bc_program_retire(BcResult *r, BcResultType t)
{
	r->t = t;
	bc_result_pop_and_push(r);
}

static BC_STATUS zbc_program_op(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2;

	s = zbc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);
	bc_num_init_DEF_SIZE(&res.d.n);

	s = BC_STATUS_SUCCESS;
	IF_ERROR_RETURN_POSSIBLE(s =) zbc_program_ops[inst - XC_INST_POWER](n1, n2, &res.d.n, G.prog.scale);
	if (s) goto err;
	bc_program_binOpRetire(&res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zbc_program_op(...) (zbc_program_op(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_read(void)
{
	const char *sv_file;
	BcStatus s;
	BcParse parse;
	BcVec buf;
	BcInstPtr ip;
	BcFunc *f;

	f = bc_program_func(BC_PROG_READ);
	bc_vec_pop_all(&f->code);

	sv_file = G.prog.file;
	G.prog.file = NULL;

	bc_char_vec_init(&buf);
	bc_read_line(&buf, stdin);

	bc_parse_create(&parse, BC_PROG_READ);
	bc_lex_file(&parse.l);

	s = zbc_parse_text_init(&parse, buf.v);
	if (s) goto exec_err;
	if (IS_BC) {
		IF_BC(s = zbc_parse_expr(&parse, 0));
	} else {
		IF_DC(s = zdc_parse_exprs_until_eof(&parse));
	}
	if (s) goto exec_err;

	if (parse.l.lex != XC_LEX_NLINE && parse.l.lex != XC_LEX_EOF) {
		s = bc_error("bad read() expression");
		goto exec_err;
	}

	ip.func = BC_PROG_READ;
	ip.inst_idx = 0;
	IF_BC(ip.results_len_before_call = G.prog.results.len;)

	bc_parse_push(&parse, XC_INST_RET);
	bc_vec_push(&G.prog.exestack, &ip);
 exec_err:
	bc_parse_free(&parse);
	G.prog.file = sv_file;
	bc_vec_free(&buf);
	RETURN_STATUS(s);
}
#define zbc_program_read(...) (zbc_program_read(__VA_ARGS__) COMMA_SUCCESS)

static size_t bc_program_index(char *code, size_t *bgn)
{
	unsigned char *bytes = (void*)(code + *bgn);
	unsigned amt;
	unsigned i;
	size_t res;

	amt = *bytes++;
	*bgn += amt + 1;

	amt *= 8;
	res = 0;
	for (i = 0; i < amt; i += 8)
		res |= (size_t)(*bytes++) << i;

	return res;
}

static char *bc_program_name(char *code, size_t *bgn)
{
	size_t i;
	char *s;

	code += *bgn;
	s = xmalloc(strchr(code, BC_PARSE_STREND) - code + 1);
	i = 0;
	for (;;) {
		char c = *code++;
		if (c == BC_PARSE_STREND)
			break;
		s[i++] = c;
	}
	s[i] = '\0';
	*bgn += i + 1;

	return s;
}

static void bc_program_printString(const char *str)
{
#if ENABLE_DC
	if (!str[0]) {
		// Example: echo '[]ap' | dc
		// should print two bytes: 0x00, 0x0A
		bb_putchar('\0');
		return;
	}
#endif
	while (*str) {
		int c = *str++;
		if (c != '\\' || !*str)
			bb_putchar(c);
		else {
			c = *str++;
			switch (c) {
			case 'a':
				bb_putchar('\a');
				break;
			case 'b':
				bb_putchar('\b');
				break;
			case '\\':
			case 'e':
				bb_putchar('\\');
				break;
			case 'f':
				bb_putchar('\f');
				break;
			case 'n':
				bb_putchar('\n');
				G.prog.nchars = SIZE_MAX;
				break;
			case 'r':
				bb_putchar('\r');
				break;
			case 'q':
				bb_putchar('"');
				break;
			case 't':
				bb_putchar('\t');
				break;
			default:
				// Just print the backslash and following character.
				bb_putchar('\\');
				++G.prog.nchars;
				bb_putchar(c);
				break;
			}
		}
		++G.prog.nchars;
	}
}

static void bc_num_printNewline(void)
{
	if (G.prog.nchars == G.prog.len - 1) {
		bb_putchar('\\');
		bb_putchar('\n');
		G.prog.nchars = 0;
	}
}

#if ENABLE_DC
static FAST_FUNC void dc_num_printChar(size_t num, size_t width, bool radix)
{
	(void) radix;
	bb_putchar((char) num);
	G.prog.nchars += width;
}
#endif

static FAST_FUNC void bc_num_printDigits(size_t num, size_t width, bool radix)
{
	size_t exp, pow;

	bc_num_printNewline();
	bb_putchar(radix ? '.' : ' ');
	++G.prog.nchars;

	bc_num_printNewline();
	for (exp = 0, pow = 1; exp < width - 1; ++exp, pow *= 10)
		continue;

	for (exp = 0; exp < width; pow /= 10, ++G.prog.nchars, ++exp) {
		size_t dig;
		bc_num_printNewline();
		dig = num / pow;
		num -= dig * pow;
		bb_putchar(((char) dig) + '0');
	}
}

static FAST_FUNC void bc_num_printHex(size_t num, size_t width, bool radix)
{
	if (radix) {
		bc_num_printNewline();
		bb_putchar('.');
		G.prog.nchars++;
	}

	bc_num_printNewline();
	bb_putchar(bb_hexdigits_upcase[num]);
	G.prog.nchars += width;
}

static void bc_num_printDecimal(BcNum *n)
{
	size_t i, rdx = n->rdx - 1;

	if (n->neg) {
		bb_putchar('-');
		G.prog.nchars++;
	}

	for (i = n->len - 1; i < n->len; --i)
		bc_num_printHex((size_t) n->num[i], 1, i == rdx);
}

typedef void (*BcNumDigitOp)(size_t, size_t, bool) FAST_FUNC;

static BC_STATUS zbc_num_printNum(BcNum *n, unsigned base_t, size_t width, BcNumDigitOp print)
{
	BcStatus s;
	BcVec stack;
	BcNum base;
	BcDig base_digs[ULONG_NUM_BUFSIZE];
	BcNum intp, fracp, digit, frac_len;
	unsigned long dig, *ptr;
	size_t i;
	bool radix;

	if (n->len == 0) {
		print(0, width, false);
		RETURN_STATUS(BC_STATUS_SUCCESS);
	}

	bc_vec_init(&stack, sizeof(long), NULL);
	bc_num_init(&intp, n->len);
	bc_num_init(&fracp, n->rdx);
	bc_num_init(&digit, width);
	bc_num_init(&frac_len, BC_NUM_INT(n));
	bc_num_copy(&intp, n);
	bc_num_one(&frac_len);
	base.cap = ARRAY_SIZE(base_digs);
	base.num = base_digs;
	bc_num_ulong2num(&base, base_t);

	bc_num_truncate(&intp, intp.rdx);
	s = zbc_num_sub(n, &intp, &fracp, 0);
	if (s) goto err;

	while (intp.len != 0) {
		s = zbc_num_divmod(&intp, &base, &intp, &digit, 0);
		if (s) goto err;
		s = zbc_num_ulong(&digit, &dig);
		if (s) goto err;
		bc_vec_push(&stack, &dig);
	}

	for (i = 0; i < stack.len; ++i) {
		ptr = bc_vec_item_rev(&stack, i);
		print(*ptr, width, false);
	}

	if (!n->rdx) goto err;

	for (radix = true; frac_len.len <= n->rdx; radix = false) {
		s = zbc_num_mul(&fracp, &base, &fracp, n->rdx);
		if (s) goto err;
		s = zbc_num_ulong(&fracp, &dig);
		if (s) goto err;
		bc_num_ulong2num(&intp, dig);
		s = zbc_num_sub(&fracp, &intp, &fracp, 0);
		if (s) goto err;
		print(dig, width, radix);
		s = zbc_num_mul(&frac_len, &base, &frac_len, 0);
		if (s) goto err;
	}
 err:
	bc_num_free(&frac_len);
	bc_num_free(&digit);
	bc_num_free(&fracp);
	bc_num_free(&intp);
	bc_vec_free(&stack);
	RETURN_STATUS(s);
}
#define zbc_num_printNum(...) (zbc_num_printNum(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_printBase(BcNum *n)
{
	BcStatus s;
	size_t width;
	BcNumDigitOp print;
	bool neg = n->neg;

	if (neg) {
		bb_putchar('-');
		G.prog.nchars++;
	}

	n->neg = false;

	if (G.prog.ob_t <= BC_NUM_MAX_IBASE) {
		width = 1;
		print = bc_num_printHex;
	} else {
		unsigned i = G.prog.ob_t - 1;
		width = 0;
		for (;;) {
			width++;
			i /= 10;
			if (i == 0)
				break;
		}
		print = bc_num_printDigits;
	}

	s = zbc_num_printNum(n, G.prog.ob_t, width, print);
	n->neg = neg;

	RETURN_STATUS(s);
}
#define zbc_num_printBase(...) (zbc_num_printBase(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_num_print(BcNum *n, bool newline)
{
	BcStatus s = BC_STATUS_SUCCESS;

	bc_num_printNewline();

	if (n->len == 0) {
		bb_putchar('0');
		++G.prog.nchars;
	} else if (G.prog.ob_t == 10)
		bc_num_printDecimal(n);
	else
		s = zbc_num_printBase(n);

	if (newline) {
		bb_putchar('\n');
		G.prog.nchars = 0;
	}

	RETURN_STATUS(s);
}
#define zbc_num_print(...) (zbc_num_print(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_print(char inst, size_t idx)
{
	BcStatus s;
	BcResult *r;
	BcNum *num;
	bool pop = (inst != XC_INST_PRINT);

	if (!STACK_HAS_MORE_THAN(&G.prog.results, idx))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	r = bc_vec_item_rev(&G.prog.results, idx);
	s = zbc_program_num(r, &num, false);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, num)) {
		s = zbc_num_print(num, !pop);
#if ENABLE_BC
		if (!s && IS_BC) bc_num_copy(&G.prog.last, num);
#endif
	} else {
		char *str;

		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : num->rdx;
		str = *bc_program_str(idx);

		if (inst == XC_INST_PRINT_STR) {
			for (;;) {
				char c = *str++;
				if (c == '\0') break;
				bb_putchar(c);
				++G.prog.nchars;
				if (c == '\n') G.prog.nchars = 0;
			}
		} else {
			bc_program_printString(str);
			if (inst == XC_INST_PRINT) bb_putchar('\n');
		}
	}

	if (!s && pop) bc_vec_pop(&G.prog.results);

	RETURN_STATUS(s);
}
#define zbc_program_print(...) (zbc_program_print(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_negate(void)
{
	BcStatus s;
	BcResult res, *ptr;
	BcNum *num;

	s = zbc_program_prep(&ptr, &num);
	if (s) RETURN_STATUS(s);

	bc_num_init(&res.d.n, num->len);
	bc_num_copy(&res.d.n, num);
	if (res.d.n.len) res.d.n.neg = !res.d.n.neg;

	bc_program_retire(&res, BC_RESULT_TEMP);

	RETURN_STATUS(s);
}
#define zbc_program_negate(...) (zbc_program_negate(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_logical(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2;
	ssize_t cond;

	s = zbc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);

	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == XC_INST_BOOL_AND)
		cond = bc_num_cmp(n1, &G.prog.zero) && bc_num_cmp(n2, &G.prog.zero);
	else if (inst == XC_INST_BOOL_OR)
		cond = bc_num_cmp(n1, &G.prog.zero) || bc_num_cmp(n2, &G.prog.zero);
	else {
		cond = bc_num_cmp(n1, n2);
		switch (inst) {
		case XC_INST_REL_EQ:
			cond = (cond == 0);
			break;
		case XC_INST_REL_LE:
			cond = (cond <= 0);
			break;
		case XC_INST_REL_GE:
			cond = (cond >= 0);
			break;
		case XC_INST_REL_LT:
			cond = (cond < 0);
			break;
		case XC_INST_REL_GT:
			cond = (cond > 0);
			break;
		default: // = case XC_INST_REL_NE:
			//cond = (cond != 0); - not needed
			break;
		}
	}

	if (cond) bc_num_one(&res.d.n);
	//else bc_num_zero(&res.d.n); - already is

	bc_program_binOpRetire(&res);

	RETURN_STATUS(s);
}
#define zbc_program_logical(...) (zbc_program_logical(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_program_assignStr(BcResult *r, BcVec *v, bool push)
{
	BcNum n2;
	BcResult res;

	memset(&n2, 0, sizeof(BcNum));
	n2.rdx = res.d.id.idx = r->d.id.idx;
	res.t = BC_RESULT_STR;

	if (!push) {
		if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
			RETURN_STATUS(bc_error_stack_has_too_few_elements());
		bc_vec_pop(v);
		bc_vec_pop(&G.prog.results);
	}

	bc_result_pop_and_push(&res);
	bc_vec_push(v, &n2);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zdc_program_assignStr(...) (zdc_program_assignStr(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static BC_STATUS zbc_program_copyToVar(char *name, bool var)
{
	BcStatus s;
	BcResult *ptr, r;
	BcVec *v;
	BcNum *n;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	ptr = bc_vec_top(&G.prog.results);
	if ((ptr->t == BC_RESULT_ARRAY) != !var)
		RETURN_STATUS(bc_error_variable_is_wrong_type());
	v = bc_program_search(name, var);

#if ENABLE_DC
	if (ptr->t == BC_RESULT_STR && !var)
		RETURN_STATUS(bc_error_variable_is_wrong_type());
	if (ptr->t == BC_RESULT_STR)
		RETURN_STATUS(zdc_program_assignStr(ptr, v, true));
#endif

	s = zbc_program_num(ptr, &n, false);
	if (s) RETURN_STATUS(s);

	// Do this once more to make sure that pointers were not invalidated.
	v = bc_program_search(name, var);

	if (var) {
		bc_num_init_DEF_SIZE(&r.d.n);
		bc_num_copy(&r.d.n, n);
	} else {
		bc_array_init(&r.d.v, true);
		bc_array_copy(&r.d.v, (BcVec *) n);
	}

	bc_vec_push(v, &r.d);
	bc_vec_pop(&G.prog.results);

	RETURN_STATUS(s);
}
#define zbc_program_copyToVar(...) (zbc_program_copyToVar(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_assign(char inst)
{
	BcStatus s;
	BcResult *left, *right, res;
	BcNum *l, *r;
	bool assign = (inst == XC_INST_ASSIGN);
	bool ib, sc;

	s = zbc_program_binOpPrep(&left, &l, &right, &r, assign);
	if (s) RETURN_STATUS(s);

	ib = left->t == BC_RESULT_IBASE;
	sc = left->t == BC_RESULT_SCALE;

#if ENABLE_DC
	if (right->t == BC_RESULT_STR) {
		BcVec *v;

		if (left->t != BC_RESULT_VAR)
			RETURN_STATUS(bc_error_variable_is_wrong_type());
		v = bc_program_search(left->d.id.name, true);

		RETURN_STATUS(zdc_program_assignStr(right, v, false));
	}
#endif

	if (left->t == BC_RESULT_CONSTANT || left->t == BC_RESULT_TEMP)
		RETURN_STATUS(bc_error("bad assignment:"
				" left side must be variable"
				" or array element"
		)); // note: shared string

#if ENABLE_BC
	if (inst == BC_INST_ASSIGN_DIVIDE && !bc_num_cmp(r, &G.prog.zero))
		RETURN_STATUS(bc_error("divide by zero"));

	if (assign)
		bc_num_copy(l, r);
	else {
		s = BC_STATUS_SUCCESS;
		IF_ERROR_RETURN_POSSIBLE(s =) zbc_program_ops[inst - BC_INST_ASSIGN_POWER](l, r, l, G.prog.scale);
	}
	if (s) RETURN_STATUS(s);
#else
	bc_num_copy(l, r);
#endif

	if (ib || sc || left->t == BC_RESULT_OBASE) {
		static const char *const msg[] = {
			"bad ibase; must be [2,16]",                 //BC_RESULT_IBASE
			"bad obase; must be [2,"BC_MAX_OBASE_STR"]", //BC_RESULT_OBASE
			"bad scale; must be [0,"BC_MAX_SCALE_STR"]", //BC_RESULT_SCALE
		};
		size_t *ptr;
		size_t max;
		unsigned long val;

		s = zbc_num_ulong(l, &val);
		if (s) RETURN_STATUS(s);
		s = left->t - BC_RESULT_IBASE;
		if (sc) {
			max = BC_MAX_SCALE;
			ptr = &G.prog.scale;
		} else {
			if (val < 2)
				RETURN_STATUS(bc_error(msg[s]));
			max = ib ? BC_NUM_MAX_IBASE : BC_MAX_OBASE;
			ptr = ib ? &G.prog.ib_t : &G.prog.ob_t;
		}

		if (val > max)
			RETURN_STATUS(bc_error(msg[s]));

		*ptr = (size_t) val;
		s = BC_STATUS_SUCCESS;
	}

	bc_num_init(&res.d.n, l->len);
	bc_num_copy(&res.d.n, l);
	bc_program_binOpRetire(&res);

	RETURN_STATUS(s);
}
#define zbc_program_assign(...) (zbc_program_assign(__VA_ARGS__) COMMA_SUCCESS)

#if !ENABLE_DC
#define bc_program_pushVar(code, bgn, pop, copy) \
	bc_program_pushVar(code, bgn)
// for bc, 'pop' and 'copy' are always false
#endif
static BC_STATUS bc_program_pushVar(char *code, size_t *bgn,
                                   bool pop, bool copy)
{
	BcResult r;
	char *name = bc_program_name(code, bgn);

	r.t = BC_RESULT_VAR;
	r.d.id.name = name;

#if ENABLE_DC
	if (pop || copy) {
		BcVec *v = bc_program_search(name, true);
		BcNum *num = bc_vec_top(v);

		free(name);
		if (!STACK_HAS_MORE_THAN(v, 1 - copy)) {
			RETURN_STATUS(bc_error_stack_has_too_few_elements());
		}

		if (!BC_PROG_STR(num)) {
			r.t = BC_RESULT_TEMP;
			bc_num_init_DEF_SIZE(&r.d.n);
			bc_num_copy(&r.d.n, num);
		} else {
			r.t = BC_RESULT_STR;
			r.d.id.idx = num->rdx;
		}

		if (!copy) bc_vec_pop(v);
	}
#endif // ENABLE_DC

	bc_vec_push(&G.prog.results, &r);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_pushVar(...) (bc_program_pushVar(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_pushArray(char *code, size_t *bgn, char inst)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult r;
	BcNum *num;

	r.d.id.name = bc_program_name(code, bgn);

	if (inst == XC_INST_ARRAY) {
		r.t = BC_RESULT_ARRAY;
		bc_vec_push(&G.prog.results, &r);
	} else {
		BcResult *operand;
		unsigned long temp;

		s = zbc_program_prep(&operand, &num);
		if (s) goto err;
		s = zbc_num_ulong(num, &temp);
		if (s) goto err;

		if (temp > BC_MAX_DIM) {
			s = bc_error("array too long; must be [1,"BC_MAX_DIM_STR"]");
			goto err;
		}

		r.d.id.idx = (size_t) temp;
		bc_program_retire(&r, BC_RESULT_ARRAY_ELEM);
	}
 err:
	if (s) free(r.d.id.name);
	RETURN_STATUS(s);
}
#define zbc_program_pushArray(...) (zbc_program_pushArray(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static BC_STATUS zbc_program_incdec(char inst)
{
	BcStatus s;
	BcResult *ptr, res, copy;
	BcNum *num;
	char inst2 = inst;

	s = zbc_program_prep(&ptr, &num);
	if (s) RETURN_STATUS(s);

	if (inst == BC_INST_INC_POST || inst == BC_INST_DEC_POST) {
		copy.t = BC_RESULT_TEMP;
		bc_num_init(&copy.d.n, num->len);
		bc_num_copy(&copy.d.n, num);
	}

	res.t = BC_RESULT_ONE;
	inst = (inst == BC_INST_INC_PRE || inst == BC_INST_INC_POST)
			? BC_INST_ASSIGN_PLUS
			: BC_INST_ASSIGN_MINUS;

	bc_vec_push(&G.prog.results, &res);
	s = zbc_program_assign(inst);
	if (s) RETURN_STATUS(s);

	if (inst2 == BC_INST_INC_POST || inst2 == BC_INST_DEC_POST) {
		bc_result_pop_and_push(&copy);
	}

	RETURN_STATUS(s);
}
#define zbc_program_incdec(...) (zbc_program_incdec(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_call(char *code, size_t *idx)
{
	BcInstPtr ip;
	size_t i, nparams;
	BcFunc *func;
	BcId *a;
	BcResult *arg;

	nparams = bc_program_index(code, idx);
	ip.inst_idx = 0;
	ip.func = bc_program_index(code, idx);
	func = bc_program_func(ip.func);

	if (func->code.len == 0) {
		RETURN_STATUS(bc_error("undefined function"));
	}
	if (nparams != func->nparams) {
		RETURN_STATUS(bc_error_fmt("function has %u parameters, but called with %u", func->nparams, nparams));
	}
	ip.results_len_before_call = G.prog.results.len - nparams;

	for (i = 0; i < nparams; ++i) {
		BcStatus s;

		a = bc_vec_item(&func->autos, nparams - 1 - i);
		arg = bc_vec_top(&G.prog.results);

		if ((!a->idx) != (arg->t == BC_RESULT_ARRAY) || arg->t == BC_RESULT_STR)
			RETURN_STATUS(bc_error_variable_is_wrong_type());

		s = zbc_program_copyToVar(a->name, a->idx);
		if (s) RETURN_STATUS(s);
	}

	a = bc_vec_item(&func->autos, i);
	for (; i < func->autos.len; i++, a++) {
		BcVec *v;

		v = bc_program_search(a->name, a->idx);
		if (a->idx) {
			BcNum n2;
			bc_num_init_DEF_SIZE(&n2);
			bc_vec_push(v, &n2);
		} else {
			BcVec v2;
			bc_array_init(&v2, true);
			bc_vec_push(v, &v2);
		}
	}

	bc_vec_push(&G.prog.exestack, &ip);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_call(...) (zbc_program_call(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_program_return(char inst)
{
	BcResult res;
	BcFunc *f;
	BcId *a;
	size_t i;
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);

	if (!STACK_HAS_EQUAL_OR_MORE_THAN(&G.prog.results, ip->results_len_before_call + (inst == XC_INST_RET)))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	f = bc_program_func(ip->func);
	res.t = BC_RESULT_TEMP;

	if (inst == XC_INST_RET) {
		BcStatus s;
		BcNum *num;
		BcResult *operand = bc_vec_top(&G.prog.results);

		s = zbc_program_num(operand, &num, false);
		if (s) RETURN_STATUS(s);
		bc_num_init(&res.d.n, num->len);
		bc_num_copy(&res.d.n, num);
	} else {
		bc_num_init_DEF_SIZE(&res.d.n);
		//bc_num_zero(&res.d.n); - already is
	}

	// We need to pop arguments as well, so this takes that into account.
	a = (void*)f->autos.v;
	for (i = 0; i < f->autos.len; i++, a++) {
		BcVec *v;
		v = bc_program_search(a->name, a->idx);
		bc_vec_pop(v);
	}

	bc_vec_npop(&G.prog.results, G.prog.results.len - ip->results_len_before_call);
	bc_vec_push(&G.prog.results, &res);
	bc_vec_pop(&G.prog.exestack);

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_return(...) (zbc_program_return(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_BC

static unsigned long bc_program_scale(BcNum *n)
{
	return (unsigned long) n->rdx;
}

static unsigned long bc_program_len(BcNum *n)
{
	size_t len = n->len;

	if (n->rdx != len) return len;
	for (;;) {
		if (len == 0) break;
		len--;
		if (n->num[len] != 0) break;
	}
	return len;
}

static BC_STATUS zbc_program_builtin(char inst)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num;
	BcResult res;
	bool len = (inst == XC_INST_LENGTH);

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	opnd = bc_vec_top(&G.prog.results);

	s = zbc_program_num(opnd, &num, false);
	if (s) RETURN_STATUS(s);

#if ENABLE_DC
	if (!BC_PROG_NUM(opnd, num) && !len)
		RETURN_STATUS(bc_error_variable_is_wrong_type());
#endif

	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == XC_INST_SQRT)
		s = zbc_num_sqrt(num, &res.d.n, G.prog.scale);
#if ENABLE_BC
	else if (len != 0 && opnd->t == BC_RESULT_ARRAY) {
		bc_num_ulong2num(&res.d.n, (unsigned long) ((BcVec *) num)->len);
	}
#endif
#if ENABLE_DC
	else if (len != 0 && !BC_PROG_NUM(opnd, num)) {
		char **str;
		size_t idx = opnd->t == BC_RESULT_STR ? opnd->d.id.idx : num->rdx;

		str = bc_program_str(idx);
		bc_num_ulong2num(&res.d.n, strlen(*str));
	}
#endif
	else {
		bc_num_ulong2num(&res.d.n, len ? bc_program_len(num) : bc_program_scale(num));
	}

	bc_program_retire(&res, BC_RESULT_TEMP);

	RETURN_STATUS(s);
}
#define zbc_program_builtin(...) (zbc_program_builtin(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_DC
static BC_STATUS zdc_program_divmod(void)
{
	BcStatus s;
	BcResult *opd1, *opd2, res, res2;
	BcNum *n1, *n2;

	s = zbc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) RETURN_STATUS(s);

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_init(&res2.d.n, n2->len);

	s = zbc_num_divmod(n1, n2, &res2.d.n, &res.d.n, G.prog.scale);
	if (s) goto err;

	bc_program_binOpRetire(&res2);
	res.t = BC_RESULT_TEMP;
	bc_vec_push(&G.prog.results, &res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res2.d.n);
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zdc_program_divmod(...) (zdc_program_divmod(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_modexp(void)
{
	BcStatus s;
	BcResult *r1, *r2, *r3, res;
	BcNum *n1, *n2, *n3;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 2))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	s = zbc_program_binOpPrep(&r2, &n2, &r3, &n3, false);
	if (s) RETURN_STATUS(s);

	r1 = bc_vec_item_rev(&G.prog.results, 2);
	s = zbc_program_num(r1, &n1, false);
	if (s) RETURN_STATUS(s);
	if (!BC_PROG_NUM(r1, n1))
		RETURN_STATUS(bc_error_variable_is_wrong_type());

	// Make sure that the values have their pointers updated, if necessary.
	if (r1->t == BC_RESULT_VAR || r1->t == BC_RESULT_ARRAY_ELEM) {
		if (r1->t == r2->t) {
			s = zbc_program_num(r2, &n2, false);
			if (s) RETURN_STATUS(s);
		}
		if (r1->t == r3->t) {
			s = zbc_program_num(r3, &n3, false);
			if (s) RETURN_STATUS(s);
		}
	}

	bc_num_init(&res.d.n, n3->len);
	s = zdc_num_modexp(n1, n2, n3, &res.d.n);
	if (s) goto err;

	bc_vec_pop(&G.prog.results);
	bc_program_binOpRetire(&res);

	RETURN_STATUS(s);
 err:
	bc_num_free(&res.d.n);
	RETURN_STATUS(s);
}
#define zdc_program_modexp(...) (zdc_program_modexp(__VA_ARGS__) COMMA_SUCCESS)

static void dc_program_stackLen(void)
{
	BcResult res;
	size_t len = G.prog.results.len;

	res.t = BC_RESULT_TEMP;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, len);
	bc_vec_push(&G.prog.results, &res);
}

static BC_STATUS zdc_program_asciify(void)
{
	BcStatus s;
	BcResult *r, res;
	BcNum *num, n;
	char **strs;
	char *str;
	char c;
	size_t idx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	r = bc_vec_top(&G.prog.results);

	s = zbc_program_num(r, &num, false);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, num)) {
		unsigned long val;
		BcNum strmb;
		BcDig strmb_digs[ULONG_NUM_BUFSIZE];

		bc_num_init_DEF_SIZE(&n);
		bc_num_copy(&n, num);
		bc_num_truncate(&n, n.rdx);

		strmb.cap = ARRAY_SIZE(strmb_digs);
		strmb.num = strmb_digs;
		bc_num_ulong2num(&strmb, 0x100);
		s = zbc_num_mod(&n, &strmb, &n, 0);

		if (s) goto num_err;
		s = zbc_num_ulong(&n, &val);
		if (s) goto num_err;

		c = (char) val;

		bc_num_free(&n);
	} else {
		char *sp;
		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : num->rdx;
		sp = *bc_program_str(idx);
		c = sp[0];
	}

	strs = (void*)G.prog.strs.v;
	for (idx = 0; idx < G.prog.strs.len; idx++) {
		if (strs[idx][0] == c && strs[idx][1] == '\0') {
			goto dup;
		}
	}
	str = xzalloc(2);
	str[0] = c;
	//str[1] = '\0'; - already is
	bc_vec_push(&G.prog.strs, &str);
 dup:
	res.t = BC_RESULT_STR;
	res.d.id.idx = idx;
	bc_result_pop_and_push(&res);

	RETURN_STATUS(BC_STATUS_SUCCESS);
 num_err:
	bc_num_free(&n);
	RETURN_STATUS(s);
}
#define zdc_program_asciify(...) (zdc_program_asciify(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_printStream(void)
{
	BcStatus s;
	BcResult *r;
	BcNum *n;
	size_t idx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	r = bc_vec_top(&G.prog.results);

	s = zbc_program_num(r, &n, false);
	if (s) RETURN_STATUS(s);

	if (BC_PROG_NUM(r, n)) {
		s = zbc_num_printNum(n, 0x100, 1, dc_num_printChar);
	} else {
		char *str;
		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : n->rdx;
		str = *bc_program_str(idx);
		fputs(str, stdout);
	}

	RETURN_STATUS(s);
}
#define zdc_program_printStream(...) (zdc_program_printStream(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_nquit(void)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num;
	unsigned long val;

	s = zbc_program_prep(&opnd, &num);
	if (s) RETURN_STATUS(s);
	s = zbc_num_ulong(num, &val);
	if (s) RETURN_STATUS(s);

	bc_vec_pop(&G.prog.results);

	if (G.prog.exestack.len < val)
		RETURN_STATUS(bc_error_stack_has_too_few_elements());
	if (G.prog.exestack.len == val) {
		QUIT_OR_RETURN_TO_MAIN;
	}

	bc_vec_npop(&G.prog.exestack, val);

	RETURN_STATUS(s);
}
#define zdc_program_nquit(...) (zdc_program_nquit(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zdc_program_execStr(char *code, size_t *bgn, bool cond)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult *r;
	BcFunc *f;
	BcInstPtr ip;
	size_t fidx, sidx;

	if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
		RETURN_STATUS(bc_error_stack_has_too_few_elements());

	r = bc_vec_top(&G.prog.results);

	if (cond) {
		BcNum *n = n; // for compiler
		bool exec;
		char *name;
		char *then_name = bc_program_name(code, bgn);
		char *else_name = NULL;

		if (code[*bgn] == BC_PARSE_STREND)
			(*bgn) += 1;
		else
			else_name = bc_program_name(code, bgn);

		exec = r->d.n.len != 0;
		name = then_name;
		if (!exec && else_name != NULL) {
			exec = true;
			name = else_name;
		}

		if (exec) {
			BcVec *v;
			v = bc_program_search(name, true);
			n = bc_vec_top(v);
		}

		free(then_name);
		free(else_name);

		if (!exec) goto exit;
		if (!BC_PROG_STR(n)) {
			s = bc_error_variable_is_wrong_type();
			goto exit;
		}

		sidx = n->rdx;
	} else {
		if (r->t == BC_RESULT_STR) {
			sidx = r->d.id.idx;
		} else if (r->t == BC_RESULT_VAR) {
			BcNum *n;
			s = zbc_program_num(r, &n, false);
			if (s || !BC_PROG_STR(n)) goto exit;
			sidx = n->rdx;
		} else
			goto exit;
	}

	fidx = sidx + BC_PROG_REQ_FUNCS;

	f = bc_program_func(fidx);

	if (f->code.len == 0) {
		FILE *sv_input_fp;
		BcParse prs;
		char *str;

		bc_parse_create(&prs, fidx);
		str = *bc_program_str(sidx);
		s = zbc_parse_text_init(&prs, str);
		if (s) goto err;

		sv_input_fp = G.input_fp;
		G.input_fp = NULL; // "do not read from input file when <EOL> reached"
		s = zdc_parse_exprs_until_eof(&prs);
		G.input_fp = sv_input_fp;

		if (s) goto err;
		if (prs.l.lex != XC_LEX_EOF) {
			s = bc_error_bad_expression();
 err:
			bc_parse_free(&prs);
			bc_vec_pop_all(&f->code);
			goto exit;
		}
		bc_parse_push(&prs, DC_INST_POP_EXEC);
		bc_parse_free(&prs);
	}

	ip.inst_idx = 0;
	ip.func = fidx;

	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.exestack, &ip);

	RETURN_STATUS(BC_STATUS_SUCCESS);
 exit:
	bc_vec_pop(&G.prog.results);
	RETURN_STATUS(s);
}
#define zdc_program_execStr(...) (zdc_program_execStr(__VA_ARGS__) COMMA_SUCCESS)
#endif // ENABLE_DC

static void bc_program_pushGlobal(char inst)
{
	BcResult res;
	unsigned long val;

	res.t = inst - XC_INST_IBASE + BC_RESULT_IBASE;
	if (inst == XC_INST_IBASE)
		val = (unsigned long) G.prog.ib_t;
	else if (inst == XC_INST_SCALE)
		val = (unsigned long) G.prog.scale;
	else
		val = (unsigned long) G.prog.ob_t;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, val);
	bc_vec_push(&G.prog.results, &res);
}

static BC_STATUS zbc_program_exec(void)
{
	BcResult r, *ptr;
	BcInstPtr *ip = bc_vec_top(&G.prog.exestack);
	BcFunc *func = bc_program_func(ip->func);
	char *code = func->code.v;

	dbg_exec("func:%zd bytes:%zd ip:%zd results.len:%d",
			ip->func, func->code.len, ip->inst_idx, G.prog.results.len);
	while (ip->inst_idx < func->code.len) {
		BcStatus s = BC_STATUS_SUCCESS;
		char inst = code[ip->inst_idx++];

		dbg_exec("inst at %zd:%d results.len:%d", ip->inst_idx - 1, inst, G.prog.results.len);
		switch (inst) {
#if ENABLE_BC
			case BC_INST_JUMP_ZERO: {
				BcNum *num;
				bool zero;
				dbg_exec("BC_INST_JUMP_ZERO:");
				s = zbc_program_prep(&ptr, &num);
				if (s) RETURN_STATUS(s);
				zero = (bc_num_cmp(num, &G.prog.zero) == 0);
				bc_vec_pop(&G.prog.results);
				if (!zero) {
					bc_program_index(code, &ip->inst_idx);
					break;
				}
				// else: fall through
			}
			case BC_INST_JUMP: {
				size_t idx = bc_program_index(code, &ip->inst_idx);
				size_t *addr = bc_vec_item(&func->labels, idx);
				dbg_exec("BC_INST_JUMP: to %ld", (long)*addr);
				ip->inst_idx = *addr;
				break;
			}
			case BC_INST_CALL:
				dbg_exec("BC_INST_CALL:");
				s = zbc_program_call(code, &ip->inst_idx);
				goto read_updated_ip;
			case BC_INST_INC_PRE:
			case BC_INST_DEC_PRE:
			case BC_INST_INC_POST:
			case BC_INST_DEC_POST:
				dbg_exec("BC_INST_INCDEC:");
				s = zbc_program_incdec(inst);
				break;
			case BC_INST_HALT:
				dbg_exec("BC_INST_HALT:");
				QUIT_OR_RETURN_TO_MAIN;
				break;
			case XC_INST_RET:
			case BC_INST_RET0:
				dbg_exec("BC_INST_RET[0]:");
				s = zbc_program_return(inst);
				goto read_updated_ip;
			case XC_INST_BOOL_OR:
			case XC_INST_BOOL_AND:
#endif // ENABLE_BC
			case XC_INST_REL_EQ:
			case XC_INST_REL_LE:
			case XC_INST_REL_GE:
			case XC_INST_REL_NE:
			case XC_INST_REL_LT:
			case XC_INST_REL_GT:
				dbg_exec("BC_INST_BOOL:");
				s = zbc_program_logical(inst);
				break;
			case XC_INST_READ:
				dbg_exec("XC_INST_READ:");
				s = zbc_program_read();
				goto read_updated_ip;
			case XC_INST_VAR:
				dbg_exec("XC_INST_VAR:");
				s = zbc_program_pushVar(code, &ip->inst_idx, false, false);
				break;
			case XC_INST_ARRAY_ELEM:
			case XC_INST_ARRAY:
				dbg_exec("XC_INST_ARRAY[_ELEM]:");
				s = zbc_program_pushArray(code, &ip->inst_idx, inst);
				break;
#if ENABLE_BC
			case BC_INST_LAST:
				dbg_exec("BC_INST_LAST:");
				r.t = BC_RESULT_LAST;
				bc_vec_push(&G.prog.results, &r);
				break;
#endif
			case XC_INST_IBASE:
			case XC_INST_OBASE:
			case XC_INST_SCALE:
				dbg_exec("XC_INST_internalvar(%d):", inst - XC_INST_IBASE);
				bc_program_pushGlobal(inst);
				break;
			case XC_INST_SCALE_FUNC:
			case XC_INST_LENGTH:
			case XC_INST_SQRT:
				dbg_exec("BC_INST_builtin:");
				s = zbc_program_builtin(inst);
				break;
			case XC_INST_NUM:
				dbg_exec("XC_INST_NUM:");
				r.t = BC_RESULT_CONSTANT;
				r.d.id.idx = bc_program_index(code, &ip->inst_idx);
				bc_vec_push(&G.prog.results, &r);
				break;
			case XC_INST_POP:
				dbg_exec("XC_INST_POP:");
				if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
					s = bc_error_stack_has_too_few_elements();
				else
					bc_vec_pop(&G.prog.results);
				break;
			case XC_INST_PRINT:
			case XC_INST_PRINT_POP:
			case XC_INST_PRINT_STR:
				dbg_exec("XC_INST_PRINTxyz:");
				s = zbc_program_print(inst, 0);
				break;
			case XC_INST_STR:
				dbg_exec("XC_INST_STR:");
				r.t = BC_RESULT_STR;
				r.d.id.idx = bc_program_index(code, &ip->inst_idx);
				bc_vec_push(&G.prog.results, &r);
				break;
			case XC_INST_POWER:
			case XC_INST_MULTIPLY:
			case XC_INST_DIVIDE:
			case XC_INST_MODULUS:
			case XC_INST_PLUS:
			case XC_INST_MINUS:
				dbg_exec("BC_INST_binaryop:");
				s = zbc_program_op(inst);
				break;
			case XC_INST_BOOL_NOT: {
				BcNum *num;
				dbg_exec("XC_INST_BOOL_NOT:");
				s = zbc_program_prep(&ptr, &num);
				if (s) RETURN_STATUS(s);
				bc_num_init_DEF_SIZE(&r.d.n);
				if (bc_num_cmp(num, &G.prog.zero) == 0)
					bc_num_one(&r.d.n);
				//else bc_num_zero(&r.d.n); - already is
				bc_program_retire(&r, BC_RESULT_TEMP);
				break;
			}
			case XC_INST_NEG:
				dbg_exec("XC_INST_NEG:");
				s = zbc_program_negate();
				break;
#if ENABLE_BC
			case BC_INST_ASSIGN_POWER:
			case BC_INST_ASSIGN_MULTIPLY:
			case BC_INST_ASSIGN_DIVIDE:
			case BC_INST_ASSIGN_MODULUS:
			case BC_INST_ASSIGN_PLUS:
			case BC_INST_ASSIGN_MINUS:
#endif
			case XC_INST_ASSIGN:
				dbg_exec("BC_INST_ASSIGNxyz:");
				s = zbc_program_assign(inst);
				break;
#if ENABLE_DC
			case DC_INST_POP_EXEC:
				dbg_exec("DC_INST_POP_EXEC:");
				bc_vec_pop(&G.prog.exestack);
				goto read_updated_ip;
			case DC_INST_MODEXP:
				dbg_exec("DC_INST_MODEXP:");
				s = zdc_program_modexp();
				break;
			case DC_INST_DIVMOD:
				dbg_exec("DC_INST_DIVMOD:");
				s = zdc_program_divmod();
				break;
			case DC_INST_EXECUTE:
			case DC_INST_EXEC_COND:
				dbg_exec("DC_INST_EXEC[_COND]:");
				s = zdc_program_execStr(code, &ip->inst_idx, inst == DC_INST_EXEC_COND);
				goto read_updated_ip;
			case DC_INST_PRINT_STACK: {
				size_t idx;
				dbg_exec("DC_INST_PRINT_STACK:");
				for (idx = 0; idx < G.prog.results.len; ++idx) {
					s = zbc_program_print(XC_INST_PRINT, idx);
					if (s) break;
				}
				break;
			}
			case DC_INST_CLEAR_STACK:
				dbg_exec("DC_INST_CLEAR_STACK:");
				bc_vec_pop_all(&G.prog.results);
				break;
			case DC_INST_STACK_LEN:
				dbg_exec("DC_INST_STACK_LEN:");
				dc_program_stackLen();
				break;
			case DC_INST_DUPLICATE:
				dbg_exec("DC_INST_DUPLICATE:");
				if (!STACK_HAS_MORE_THAN(&G.prog.results, 0))
					RETURN_STATUS(bc_error_stack_has_too_few_elements());
				ptr = bc_vec_top(&G.prog.results);
				dc_result_copy(&r, ptr);
				bc_vec_push(&G.prog.results, &r);
				break;
			case DC_INST_SWAP: {
				BcResult *ptr2;
				dbg_exec("DC_INST_SWAP:");
				if (!STACK_HAS_MORE_THAN(&G.prog.results, 1))
					RETURN_STATUS(bc_error_stack_has_too_few_elements());
				ptr = bc_vec_item_rev(&G.prog.results, 0);
				ptr2 = bc_vec_item_rev(&G.prog.results, 1);
				memcpy(&r, ptr, sizeof(BcResult));
				memcpy(ptr, ptr2, sizeof(BcResult));
				memcpy(ptr2, &r, sizeof(BcResult));
				break;
			}
			case DC_INST_ASCIIFY:
				dbg_exec("DC_INST_ASCIIFY:");
				s = zdc_program_asciify();
				break;
			case DC_INST_PRINT_STREAM:
				dbg_exec("DC_INST_PRINT_STREAM:");
				s = zdc_program_printStream();
				break;
			case DC_INST_LOAD:
			case DC_INST_PUSH_VAR: {
				bool copy = inst == DC_INST_LOAD;
				s = zbc_program_pushVar(code, &ip->inst_idx, true, copy);
				break;
			}
			case DC_INST_PUSH_TO_VAR: {
				char *name = bc_program_name(code, &ip->inst_idx);
				s = zbc_program_copyToVar(name, true);
				free(name);
				break;
			}
			case DC_INST_QUIT:
				dbg_exec("DC_INST_QUIT:");
				if (G.prog.exestack.len <= 2)
					QUIT_OR_RETURN_TO_MAIN;
				bc_vec_npop(&G.prog.exestack, 2);
				goto read_updated_ip;
			case DC_INST_NQUIT:
				dbg_exec("DC_INST_NQUIT:");
				s = zdc_program_nquit();
				//goto read_updated_ip; - just fall through to it
#endif // ENABLE_DC
 read_updated_ip:
				// Instruction stack has changed, read new pointers
				ip = bc_vec_top(&G.prog.exestack);
				func = bc_program_func(ip->func);
				code = func->code.v;
				dbg_exec("func:%zd bytes:%zd ip:%zd", ip->func, func->code.len, ip->inst_idx);
		}

		if (s || G_interrupt) {
			bc_program_reset();
			RETURN_STATUS(s);
		}

		fflush_and_check();
	}

	RETURN_STATUS(BC_STATUS_SUCCESS);
}
#define zbc_program_exec(...) (zbc_program_exec(__VA_ARGS__) COMMA_SUCCESS)

static unsigned bc_vm_envLen(const char *var)
{
	char *lenv;
	unsigned len;

	lenv = getenv(var);
	len = BC_NUM_PRINT_WIDTH;
	if (!lenv) return len;

	len = bb_strtou(lenv, NULL, 10) - 1;
	if (errno || len < 2 || len >= INT_MAX)
		len = BC_NUM_PRINT_WIDTH;

	return len;
}

static BC_STATUS zbc_vm_process(const char *text)
{
	BcStatus s;

	dbg_lex_enter("%s:%d entered", __func__, __LINE__);
	s = zbc_parse_text_init(&G.prs, text); // does the first zbc_lex_next()
	if (s) RETURN_STATUS(s);

	while (G.prs.l.lex != XC_LEX_EOF) {
		BcInstPtr *ip;
		BcFunc *f;

		dbg_lex("%s:%d G.prs.l.lex:%d, parsing...", __func__, __LINE__, G.prs.l.lex);
		if (IS_BC) {
// FIXME: "eating" of stmt delimiters is coded inconsistently
// (sometimes zbc_parse_stmt() eats the delimiter, sometimes don't),
// which causes bugs such as "print 1 print 2" erroneously accepted,
// or "print 1 else 2" detecting parse error only after executing
// "print 1" part.
			IF_BC(s = zbc_parse_stmt_or_funcdef(&G.prs));
		} else {
			// Most of dc parsing assumes all whitespace,
			// including '\n', is eaten.
			while (G.prs.l.lex == XC_LEX_NLINE) {
				s = zbc_lex_next(&G.prs.l);
				if (s) goto err;
				if (G.prs.l.lex == XC_LEX_EOF)
					goto done;
			}
			IF_DC(s = zdc_parse_expr(&G.prs));
		}
		if (s || G_interrupt) {
 err:
			bc_parse_reset(&G.prs); // includes bc_program_reset()
			RETURN_STATUS(BC_STATUS_FAILURE);
		}

		dbg_lex("%s:%d executing...", __func__, __LINE__);
		s = zbc_program_exec();
		if (s) {
			bc_program_reset();
			break;
		}

		ip = (void*)G.prog.exestack.v;
#if SANITY_CHECKS
		if (G.prog.exestack.len != 1) // should have only main's IP
			bb_error_msg_and_die("BUG:call stack");
		if (ip->func != BC_PROG_MAIN)
			bb_error_msg_and_die("BUG:not MAIN");
#endif
		f = bc_program_func_BC_PROG_MAIN();
		// bc discards strings, constants and code after each
		// top-level statement in the "main program".
		// This prevents "yes 1 | bc" from growing its memory
		// without bound. This can be done because data stack
		// is empty and thus can't hold any references to
		// strings or constants, there is no generated code
		// which can hold references (after we discard one
		// we just executed). Code of functions can have references,
		// but bc stores function strings/constants in per-function
		// storage.
		if (IS_BC) {
#if SANITY_CHECKS
			if (G.prog.results.len != 0) // should be empty
				bb_error_msg_and_die("BUG:data stack");
#endif
			IF_BC(bc_vec_pop_all(&f->strs);)
			IF_BC(bc_vec_pop_all(&f->consts);)
		} else {
			if (G.prog.results.len == 0
			 && G.prog.vars.len == 0
			) {
				// If stack is empty and no registers exist (TODO: or they are all empty),
				// we can get rid of accumulated strings and constants.
				// In this example dc process should not grow
				// its memory consumption with time:
				// yes 1pc | dc
				IF_DC(bc_vec_pop_all(&G.prog.strs);)
				IF_DC(bc_vec_pop_all(&G.prog.consts);)
			}
			// The code is discarded always (below), thus this example
			// should also not grow its memory consumption with time,
			// even though its data stack is not empty:
			// { echo 1; yes dk; } | dc
		}
		// We drop generated and executed code for both bc and dc:
		bc_vec_pop_all(&f->code);
		ip->inst_idx = 0;
	}
 done:
	dbg_lex_done("%s:%d done", __func__, __LINE__);
	RETURN_STATUS(s);
}
#define zbc_vm_process(...) (zbc_vm_process(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_vm_execute_FILE(FILE *fp, const char *filename)
{
	// So far bc/dc have no way to include a file from another file,
	// therefore we know G.prog.file == NULL on entry
	//const char *sv_file;
	BcStatus s;

	G.prog.file = filename;
	G.input_fp = fp;
	bc_lex_file(&G.prs.l);

	do {
		s = zbc_vm_process("");
		// We do not stop looping on errors here if reading stdin.
		// Example: start interactive bc and enter "return".
		// It should say "'return' not in a function"
		// but should not exit.
	} while (G.input_fp == stdin);
	G.prog.file = NULL;
	RETURN_STATUS(s);
}
#define zbc_vm_execute_FILE(...) (zbc_vm_execute_FILE(__VA_ARGS__) COMMA_SUCCESS)

static BC_STATUS zbc_vm_file(const char *file)
{
	BcStatus s;
	FILE *fp;

	fp = xfopen_for_read(file);
	s = zbc_vm_execute_FILE(fp, file);
	fclose(fp);

	RETURN_STATUS(s);
}
#define zbc_vm_file(...) (zbc_vm_file(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_BC
static void bc_vm_info(void)
{
	printf("%s "BB_VER"\n"
		"Copyright (c) 2018 Gavin D. Howard and contributors\n"
	, applet_name);
}

static void bc_args(char **argv)
{
	unsigned opts;
	int i;

	GETOPT_RESET();
#if ENABLE_FEATURE_BC_LONG_OPTIONS
	opts = option_mask32 |= getopt32long(argv, "wvsqli",
		"warn\0"              No_argument "w"
		"version\0"           No_argument "v"
		"standard\0"          No_argument "s"
		"quiet\0"             No_argument "q"
		"mathlib\0"           No_argument "l"
		"interactive\0"       No_argument "i"
	);
#else
	opts = option_mask32 |= getopt32(argv, "wvsqli");
#endif
	if (getenv("POSIXLY_CORRECT"))
		option_mask32 |= BC_FLAG_S;

	if (opts & BC_FLAG_V) {
		bc_vm_info();
		exit(0);
	}

	for (i = optind; argv[i]; ++i)
		bc_vec_push(&G.files, argv + i);
}

static void bc_vm_envArgs(void)
{
	BcVec v;
	char *buf;
	char *env_args = getenv("BC_ENV_ARGS");

	if (!env_args) return;

	G.env_args = xstrdup(env_args);
	buf = G.env_args;

	bc_vec_init(&v, sizeof(char *), NULL);

	while (*(buf = skip_whitespace(buf)) != '\0') {
		bc_vec_push(&v, &buf);
		buf = skip_non_whitespace(buf);
		if (!*buf)
			break;
		*buf++ = '\0';
	}

	// NULL terminate, and pass argv[] so that first arg is argv[1]
	if (sizeof(int) == sizeof(char*)) {
		bc_vec_push(&v, &const_int_0);
	} else {
		static char *const nullptr = NULL;
		bc_vec_push(&v, &nullptr);
	}
	bc_args(((char **)v.v) - 1);

	bc_vec_free(&v);
}

static const char bc_lib[] ALIGN1 = {
	"scale=20"
"\n"	"define e(x){"
"\n"		"auto b,s,n,r,d,i,p,f,v"
////////////////"if(x<0)return(1/e(-x))" // and drop 'n' and x<0 logic below
//^^^^^^^^^^^^^^^^ this would work, and is even more precise than GNU bc:
//e(-.998896): GNU:.36828580434569428695
//      above code:.36828580434569428696
//    actual value:.3682858043456942869594...
// but for now let's be "GNU compatible"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"if(x<0){"
"\n"			"n=1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"s=scale"
"\n"		"r=6+s+.44*x"
"\n"		"scale=scale(x)+1"
"\n"		"while(x>1){"
"\n"			"d+=1"
"\n"			"x/=2"
"\n"			"scale+=1"
"\n"		"}"
"\n"		"scale=r"
"\n"		"r=x+1"
"\n"		"p=x"
"\n"		"f=v=1"
"\n"		"for(i=2;v;++i){"
"\n"			"p*=x"
"\n"			"f*=i"
"\n"			"v=p/f"
"\n"			"r+=v"
"\n"		"}"
"\n"		"while(d--)r*=r"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(n)return(1/r)"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define l(x){"
"\n"		"auto b,s,r,p,a,q,i,v"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"if(x<=0){"
"\n"			"r=(1-10^scale)/1"
"\n"			"ibase=b"
"\n"			"return(r)"
"\n"		"}"
"\n"		"s=scale"
"\n"		"scale+=6"
"\n"		"p=2"
"\n"		"while(x>=2){"
"\n"			"p*=2"
"\n"			"x=sqrt(x)"
"\n"		"}"
"\n"		"while(x<=.5){"
"\n"			"p*=2"
"\n"			"x=sqrt(x)"
"\n"		"}"
"\n"		"r=a=(x-1)/(x+1)"
"\n"		"q=a*a"
"\n"		"v=1"
"\n"		"for(i=3;v;i+=2){"
"\n"			"a*=q"
"\n"			"v=a/i"
"\n"			"r+=v"
"\n"		"}"
"\n"		"r*=p"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define s(x){"
"\n"		"auto b,s,r,a,q,i"
"\n"		"if(x<0)return(-s(-x))"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale=1.1*s+2"
"\n"		"a=a(1)"
"\n"		"scale=0"
"\n"		"q=(x/a+2)/4"
"\n"		"x-=4*q*a"
"\n"		"if(q%2)x=-x"
"\n"		"scale=s+2"
"\n"		"r=a=x"
"\n"		"q=-x*x"
"\n"		"for(i=3;a;i+=2){"
"\n"			"a*=q/(i*(i-1))"
"\n"			"r+=a"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(r/1)"
"\n"	"}"
"\n"	"define c(x){"
"\n"		"auto b,s"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale*=1.2"
"\n"		"x=s(2*a(1)+x)"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return(x/1)"
"\n"	"}"
"\n"	"define a(x){"
"\n"		"auto b,s,r,n,a,m,t,f,i,u"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"n=1"
"\n"		"if(x<0){"
"\n"			"n=-1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"if(scale<65){"
"\n"			"if(x==1)return(.7853981633974483096156608458198757210492923498437764552437361480/n)"
"\n"			"if(x==.2)return(.1973955598498807583700497651947902934475851037878521015176889402/n)"
"\n"		"}"
"\n"		"s=scale"
"\n"		"if(x>.2){"
"\n"			"scale+=5"
"\n"			"a=a(.2)"
"\n"		"}"
"\n"		"scale=s+3"
"\n"		"while(x>.2){"
"\n"			"m+=1"
"\n"			"x=(x-.2)/(1+.2*x)"
"\n"		"}"
"\n"		"r=u=x"
"\n"		"f=-x*x"
"\n"		"t=1"
"\n"		"for(i=3;t;i+=2){"
"\n"			"u*=f"
"\n"			"t=u/i"
"\n"			"r+=t"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"return((m*a+r)/n)"
"\n"	"}"
"\n"	"define j(n,x){"
"\n"		"auto b,s,o,a,i,v,f"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale=0"
"\n"		"n/=1"
"\n"		"if(n<0){"
"\n"			"n=-n"
"\n"			"o=n%2"
"\n"		"}"
"\n"		"a=1"
"\n"		"for(i=2;i<=n;++i)a*=i"
"\n"		"scale=1.5*s"
"\n"		"a=(x^n)/2^n/a"
"\n"		"r=v=1"
"\n"		"f=-x*x/4"
"\n"		"scale+=length(a)-scale(a)"
"\n"		"for(i=1;v;++i){"
"\n"			"v=v*f/i/(n+i)"
"\n"			"r+=v"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(o)a=-a"
"\n"		"return(a*r/1)"
"\n"	"}"
};
#endif // ENABLE_BC

static BC_STATUS zbc_vm_exec(void)
{
	char **fname;
	BcStatus s;
	size_t i;

#if ENABLE_BC
	if (option_mask32 & BC_FLAG_L) {
		// We know that internal library is not buggy,
		// thus error checking is normally disabled.
# define DEBUG_LIB 0
		bc_lex_file(&G.prs.l);
		s = zbc_vm_process(bc_lib);
		if (DEBUG_LIB && s) RETURN_STATUS(s);
	}
#endif

	s = BC_STATUS_SUCCESS;
	fname = (void*)G.files.v;
	for (i = 0; i < G.files.len; i++) {
		s = zbc_vm_file(*fname++);
		if (ENABLE_FEATURE_CLEAN_UP && !G_ttyin && s) {
			// Debug config, non-interactive mode:
			// return all the way back to main.
			// Non-debug builds do not come here
			// in non-interactive mode, they exit.
			RETURN_STATUS(s);
		}
	}

	if (IS_BC || (option_mask32 & BC_FLAG_I))
		s = zbc_vm_execute_FILE(stdin, /*filename:*/ NULL);

	RETURN_STATUS(s);
}
#define zbc_vm_exec(...) (zbc_vm_exec(__VA_ARGS__) COMMA_SUCCESS)

#if ENABLE_FEATURE_CLEAN_UP
static void bc_program_free(void)
{
	bc_vec_free(&G.prog.fns);
	IF_BC(bc_vec_free(&G.prog.fn_map);)
	bc_vec_free(&G.prog.vars);
	bc_vec_free(&G.prog.var_map);
	bc_vec_free(&G.prog.arrs);
	bc_vec_free(&G.prog.arr_map);
	IF_DC(bc_vec_free(&G.prog.strs);)
	IF_DC(bc_vec_free(&G.prog.consts);)
	bc_vec_free(&G.prog.results);
	bc_vec_free(&G.prog.exestack);
	IF_BC(bc_num_free(&G.prog.last);)
	IF_BC(bc_num_free(&G.prog.zero);)
	IF_BC(bc_num_free(&G.prog.one);)
	bc_vec_free(&G.input_buffer);
}

static void bc_vm_free(void)
{
	bc_vec_free(&G.files);
	bc_program_free();
	bc_parse_free(&G.prs);
	free(G.env_args);
}
#endif

static void bc_program_init(void)
{
	BcInstPtr ip;

	// memset(&G.prog, 0, sizeof(G.prog)); - already is
	memset(&ip, 0, sizeof(BcInstPtr));

	// G.prog.nchars = G.prog.scale = 0; - already is
	G.prog.ib_t = 10;
	G.prog.ob_t = 10;

	IF_BC(bc_num_init_DEF_SIZE(&G.prog.last);)
	//IF_BC(bc_num_zero(&G.prog.last);) - already is

	bc_num_init_DEF_SIZE(&G.prog.zero);
	//bc_num_zero(&G.prog.zero); - already is

	IF_BC(bc_num_init_DEF_SIZE(&G.prog.one);)
	IF_BC(bc_num_one(&G.prog.one);)

	bc_vec_init(&G.prog.fns, sizeof(BcFunc), bc_func_free);
	IF_BC(bc_vec_init(&G.prog.fn_map, sizeof(BcId), bc_id_free);)

	if (IS_BC) {
		// Names are chosen simply to be distinct and never match
		// a valid function name (and be short)
		IF_BC(bc_program_addFunc(xstrdup(""))); // func #0: main
		IF_BC(bc_program_addFunc(xstrdup("1"))); // func #1: for read()
	} else {
		// in dc, functions have no names
		bc_program_add_fn();
		bc_program_add_fn();
	}

	bc_vec_init(&G.prog.vars, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.var_map, sizeof(BcId), bc_id_free);

	bc_vec_init(&G.prog.arrs, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.arr_map, sizeof(BcId), bc_id_free);

	IF_DC(bc_vec_init(&G.prog.strs, sizeof(char *), bc_string_free);)
	IF_DC(bc_vec_init(&G.prog.consts, sizeof(char *), bc_string_free);)
	bc_vec_init(&G.prog.results, sizeof(BcResult), bc_result_free);
	bc_vec_init(&G.prog.exestack, sizeof(BcInstPtr), NULL);
	bc_vec_push(&G.prog.exestack, &ip);

	bc_char_vec_init(&G.input_buffer);
}

static int bc_vm_init(const char *env_len)
{
#if ENABLE_FEATURE_EDITING
	G.line_input_state = new_line_input_t(DO_HISTORY);
#endif
	G.prog.len = bc_vm_envLen(env_len);

	bc_vec_init(&G.files, sizeof(char *), NULL);
	IF_BC(if (IS_BC) bc_vm_envArgs();)
	bc_program_init();
	bc_parse_create(&G.prs, BC_PROG_MAIN);

//TODO: in GNU bc, the check is (isatty(0) && isatty(1)),
//-i option unconditionally enables this regardless of isatty():
	if (isatty(0)) {
#if ENABLE_FEATURE_BC_SIGNALS
		G_ttyin = 1;
		// With SA_RESTART, most system calls will restart
		// (IOW: they won't fail with EINTR).
		// In particular, this means ^C won't cause
		// stdout to get into "error state" if SIGINT hits
		// within write() syscall.
		//
		// The downside is that ^C while tty input is taken
		// will only be handled after [Enter] since read()
		// from stdin is not interrupted by ^C either,
		// it restarts, thus fgetc() does not return on ^C.
		// (This problem manifests only if line editing is disabled)
		signal_SA_RESTART_empty_mask(SIGINT, record_signo);

		// Without SA_RESTART, this exhibits a bug:
		// "while (1) print 1" and try ^C-ing it.
		// Intermittently, instead of returning to input line,
		// you'll get "output error: Interrupted system call"
		// and exit.
		//signal_no_SA_RESTART_empty_mask(SIGINT, record_signo);
#endif
		return 1; // "tty"
	}
	return 0; // "not a tty"
}

static BcStatus bc_vm_run(void)
{
	BcStatus st = zbc_vm_exec();
#if ENABLE_FEATURE_CLEAN_UP
	if (G_exiting) // it was actually "halt" or "quit"
		st = EXIT_SUCCESS;
	bc_vm_free();
# if ENABLE_FEATURE_EDITING
	free_line_input_t(G.line_input_state);
# endif
	FREE_G();
#endif
	dbg_exec("exiting with exitcode %d", st);
	return st;
}

#if ENABLE_BC
int bc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bc_main(int argc UNUSED_PARAM, char **argv)
{
	int is_tty;

	INIT_G();

	is_tty = bc_vm_init("BC_LINE_LENGTH");

	bc_args(argv);

	if (is_tty && !(option_mask32 & BC_FLAG_Q))
		bc_vm_info();

	return bc_vm_run();
}
#endif

#if ENABLE_DC
int dc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dc_main(int argc UNUSED_PARAM, char **argv)
{
	int noscript;

	INIT_G();

	// TODO: dc (GNU bc 1.07.1) 1.4.1 seems to use width
	// 1 char wider than bc from the same package.
	// Both default width, and xC_LINE_LENGTH=N are wider:
	// "DC_LINE_LENGTH=5 dc -e'123456 p'" prints:
	//	|1234\   |
	//	|56      |
	// "echo '123456' | BC_LINE_LENGTH=5 bc" prints:
	//	|123\    |
	//	|456     |
	// Do the same, or it's a bug?
	bc_vm_init("DC_LINE_LENGTH");

	// Run -e'SCRIPT' and -fFILE in order of appearance, then handle FILEs
	noscript = BC_FLAG_I;
	for (;;) {
		int n = getopt(argc, argv, "e:f:x");
		if (n <= 0)
			break;
		switch (n) {
		case 'e':
			noscript = 0;
			n = zbc_vm_process(optarg);
			if (n) return n;
			break;
		case 'f':
			noscript = 0;
			n = zbc_vm_file(optarg);
			if (n) return n;
			break;
		case 'x':
			option_mask32 |= DC_FLAG_X;
			break;
		default:
			bb_show_usage();
		}
	}
	argv += optind;

	while (*argv) {
		noscript = 0;
		bc_vec_push(&G.files, argv++);
	}

	option_mask32 |= noscript; // set BC_FLAG_I if we need to interpret stdin

	return bc_vm_run();
}
#endif

#endif // not DC_SMALL
