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
//config:	(http://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html)
//config:	for details.
//config:
//config:	This bc has four differences to the GNU bc:
//config:
//config:	  1) The period (.) can also be used as a shortcut for "last", as in
//config:	     the BSD bc.
//config:	  2) Arrays are copied before being passed as arguments to
//config:	     functions. This behavior is required by the bc spec.
//config:	  3) Arrays can be passed to the builtin "length" function to get
//config:	     the number of elements currently in the array. The following
//config:	     example prints "1":
//config:
//config:	       a[0] = 0
//config:	       length(a[])
//config:
//config:	  4) The precedence of the boolean "not" operator (!) is equal to
//config:	     that of the unary minus (-), or negation, operator. This still
//config:	     allows POSIX-compliant scripts to work while somewhat
//config:	     preserving expected behavior (versus C) and making parsing
//config:	     easier.
//config:
//config:	Options:
//config:
//config:	  -i  --interactive  force interactive mode
//config:	  -l  --mathlib      use predefined math routines:
//config:
//config:	                       s(expr)  =  sine of expr in radians
//config:	                       c(expr)  =  cosine of expr in radians
//config:	                       a(expr)  =  arctangent of expr, returning
//config:	                                   radians
//config:	                       l(expr)  =  natural log of expr
//config:	                       e(expr)  =  raises e to the power of expr
//config:	                       j(n, x)  =  Bessel function of integer order
//config:	                                   n of x
//config:
//config:	  -q  --quiet        don't print version and copyright.
//config:	  -s  --standard     error if any non-POSIX extensions are used.
//config:	  -w  --warn         warn if any non-POSIX extensions are used.
//config:	  -v  --version      print version and copyright and exit.
//config:
//config:	Long options are only available if FEATURE_BC_LONG_OPTIONS is
//config:	enabled.
//config:
//config:config DC
//config:	bool "dc (38 kb; 49 kb when combined with bc)"
//config:	default y
//config:	help
//config:	dc is a reverse-polish notation command-line calculator which
//config:	supports unlimited precision arithmetic. See the FreeBSD man page
//config:	(https://www.unix.com/man-page/FreeBSD/1/dc/) and GNU dc manual
//config:	(https://www.gnu.org/software/bc/manual/dc-1.05/html_mono/dc.html)
//config:	for details.
//config:
//config:	This dc has a few differences from the two above:
//config:
//config:	  1) When printing a byte stream (command "P"), this bc follows what
//config:	     the FreeBSD dc does.
//config:	  2) This dc implements the GNU extensions for divmod ("~") and
//config:	     modular exponentiation ("|").
//config:	  3) This dc implements all FreeBSD extensions, except for "J" and
//config:	     "M".
//config:	  4) Like the FreeBSD dc, this dc supports extended registers.
//config:	     However, they are implemented differently. When it encounters
//config:	     whitespace where a register should be, it skips the whitespace.
//config:	     If the character following is not a lowercase letter, an error
//config:	     is issued. Otherwise, the register name is parsed by the
//config:	     following regex:
//config:
//config:	       [a-z][a-z0-9_]*
//config:
//config:	     This generally means that register names will be surrounded by
//config:	     whitespace.
//config:
//config:	     Examples:
//config:
//config:	       l idx s temp L index S temp2 < do_thing
//config:
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
//config:	bool "Enable bc/dc signal handling"
//config:	default y
//config:	depends on (BC || DC) && !FEATURE_DC_SMALL
//config:	help
//config:	Enable signal handling for bc and dc.
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
//usage:       "[-sqliw] FILE..."
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
//usage:     "\np - print top of the stack (without popping)"
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

typedef enum BcStatus {
	BC_STATUS_SUCCESS = 0,
	BC_STATUS_FAILURE = 1,
	BC_STATUS_PARSE_EMPTY_EXP = 2, // bc_parse_expr() uses this
	BC_STATUS_EOF = 3, // bc_vm_stdin() uses this
} BcStatus;

#define BC_VEC_INVALID_IDX ((size_t) -1)
#define BC_VEC_START_CAP (1 << 5)

typedef void (*BcVecFree)(void *);

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

#define BC_NUM_MIN_BASE         ((unsigned long) 2)
#define BC_NUM_MAX_IBASE        ((unsigned long) 16)
// larger value might speed up BIGNUM calculations a bit:
#define BC_NUM_DEF_SIZE         (16)
#define BC_NUM_PRINT_WIDTH      (69)

#define BC_NUM_KARATSUBA_LEN    (32)

typedef void (*BcNumDigitOp)(size_t, size_t, bool);

typedef BcStatus (*BcNumBinaryOp)(BcNum *, BcNum *, BcNum *, size_t);

static BcStatus bc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale);
static BcStatus bc_num_sqrt(BcNum *a, BcNum *b, size_t scale);
static BcStatus bc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d,
                              size_t scale);

typedef enum BcInst {

#if ENABLE_BC
	BC_INST_INC_PRE,
	BC_INST_DEC_PRE,
	BC_INST_INC_POST,
	BC_INST_DEC_POST,
#endif

	BC_INST_NEG,

	BC_INST_POWER,
	BC_INST_MULTIPLY,
	BC_INST_DIVIDE,
	BC_INST_MODULUS,
	BC_INST_PLUS,
	BC_INST_MINUS,

	BC_INST_REL_EQ,
	BC_INST_REL_LE,
	BC_INST_REL_GE,
	BC_INST_REL_NE,
	BC_INST_REL_LT,
	BC_INST_REL_GT,

	BC_INST_BOOL_NOT,
	BC_INST_BOOL_OR,
	BC_INST_BOOL_AND,

#if ENABLE_BC
	BC_INST_ASSIGN_POWER,
	BC_INST_ASSIGN_MULTIPLY,
	BC_INST_ASSIGN_DIVIDE,
	BC_INST_ASSIGN_MODULUS,
	BC_INST_ASSIGN_PLUS,
	BC_INST_ASSIGN_MINUS,
#endif
	BC_INST_ASSIGN,

	BC_INST_NUM,
	BC_INST_VAR,
	BC_INST_ARRAY_ELEM,
	BC_INST_ARRAY,

	BC_INST_SCALE_FUNC,
	BC_INST_IBASE,
	BC_INST_SCALE,
	BC_INST_LAST,
	BC_INST_LENGTH,
	BC_INST_READ,
	BC_INST_OBASE,
	BC_INST_SQRT,

	BC_INST_PRINT,
	BC_INST_PRINT_POP,
	BC_INST_STR,
	BC_INST_PRINT_STR,

#if ENABLE_BC
	BC_INST_JUMP,
	BC_INST_JUMP_ZERO,

	BC_INST_CALL,

	BC_INST_RET,
	BC_INST_RET0,

	BC_INST_HALT,
#endif

	BC_INST_POP,
	BC_INST_POP_EXEC,

#if ENABLE_DC
	BC_INST_MODEXP,
	BC_INST_DIVMOD,

	BC_INST_EXECUTE,
	BC_INST_EXEC_COND,

	BC_INST_ASCIIFY,
	BC_INST_PRINT_STREAM,

	BC_INST_PRINT_STACK,
	BC_INST_CLEAR_STACK,
	BC_INST_STACK_LEN,
	BC_INST_DUPLICATE,
	BC_INST_SWAP,

	BC_INST_LOAD,
	BC_INST_PUSH_VAR,
	BC_INST_PUSH_TO_VAR,

	BC_INST_QUIT,
	BC_INST_NQUIT,

	BC_INST_INVALID = -1,
#endif

} BcInst;

typedef struct BcId {
	char *name;
	size_t idx;
} BcId;

typedef struct BcFunc {
	BcVec code;
	BcVec labels;
	size_t nparams;
	BcVec autos;
} BcFunc;

typedef enum BcResultType {

	BC_RESULT_TEMP,

	BC_RESULT_VAR,
	BC_RESULT_ARRAY_ELEM,
	BC_RESULT_ARRAY,

	BC_RESULT_STR,

	BC_RESULT_IBASE,
	BC_RESULT_SCALE,
	BC_RESULT_LAST,

	// These are between to calculate ibase, obase, and last from instructions.
	BC_RESULT_CONSTANT,
	BC_RESULT_ONE,

	BC_RESULT_OBASE,

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
	size_t idx;
	size_t len;
} BcInstPtr;

// BC_LEX_NEG is not used in lexing; it is only for parsing.
typedef enum BcLexType {

	BC_LEX_EOF,
	BC_LEX_INVALID,

	BC_LEX_OP_INC,
	BC_LEX_OP_DEC,

	BC_LEX_NEG,

	BC_LEX_OP_POWER,
	BC_LEX_OP_MULTIPLY,
	BC_LEX_OP_DIVIDE,
	BC_LEX_OP_MODULUS,
	BC_LEX_OP_PLUS,
	BC_LEX_OP_MINUS,

	BC_LEX_OP_REL_EQ,
	BC_LEX_OP_REL_LE,
	BC_LEX_OP_REL_GE,
	BC_LEX_OP_REL_NE,
	BC_LEX_OP_REL_LT,
	BC_LEX_OP_REL_GT,

	BC_LEX_OP_BOOL_NOT,
	BC_LEX_OP_BOOL_OR,
	BC_LEX_OP_BOOL_AND,

	BC_LEX_OP_ASSIGN_POWER,
	BC_LEX_OP_ASSIGN_MULTIPLY,
	BC_LEX_OP_ASSIGN_DIVIDE,
	BC_LEX_OP_ASSIGN_MODULUS,
	BC_LEX_OP_ASSIGN_PLUS,
	BC_LEX_OP_ASSIGN_MINUS,
	BC_LEX_OP_ASSIGN,

	BC_LEX_NLINE,
	BC_LEX_WHITESPACE,

	BC_LEX_LPAREN,
	BC_LEX_RPAREN,

	BC_LEX_LBRACKET,
	BC_LEX_COMMA,
	BC_LEX_RBRACKET,

	BC_LEX_LBRACE,
	BC_LEX_SCOLON,
	BC_LEX_RBRACE,

	BC_LEX_STR,
	BC_LEX_NAME,
	BC_LEX_NUMBER,

	BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_AUTO = BC_LEX_KEY_1st_keyword,
	BC_LEX_KEY_BREAK,
	BC_LEX_KEY_CONTINUE,
	BC_LEX_KEY_DEFINE,
	BC_LEX_KEY_ELSE,
	BC_LEX_KEY_FOR,
	BC_LEX_KEY_HALT,
	// code uses "type - BC_LEX_KEY_IBASE + BC_INST_IBASE" construct,
	BC_LEX_KEY_IBASE,  // relative order should match for: BC_INST_IBASE
	BC_LEX_KEY_IF,
	BC_LEX_KEY_LAST,   // relative order should match for: BC_INST_LAST
	BC_LEX_KEY_LENGTH,
	BC_LEX_KEY_LIMITS,
	BC_LEX_KEY_OBASE,  // relative order should match for: BC_INST_OBASE
	BC_LEX_KEY_PRINT,
	BC_LEX_KEY_QUIT,
	BC_LEX_KEY_READ,
	BC_LEX_KEY_RETURN,
	BC_LEX_KEY_SCALE,
	BC_LEX_KEY_SQRT,
	BC_LEX_KEY_WHILE,

#if ENABLE_DC
	BC_LEX_EQ_NO_REG,
	BC_LEX_OP_MODEXP,
	BC_LEX_OP_DIVMOD,

	BC_LEX_COLON,
	BC_LEX_ELSE,
	BC_LEX_EXECUTE,
	BC_LEX_PRINT_STACK,
	BC_LEX_CLEAR_STACK,
	BC_LEX_STACK_LEVEL,
	BC_LEX_DUPLICATE,
	BC_LEX_SWAP,
	BC_LEX_POP,

	BC_LEX_ASCIIFY,
	BC_LEX_PRINT_STREAM,

	BC_LEX_STORE_IBASE,
	BC_LEX_STORE_SCALE,
	BC_LEX_LOAD,
	BC_LEX_LOAD_POP,
	BC_LEX_STORE_PUSH,
	BC_LEX_STORE_OBASE,
	BC_LEX_PRINT_POP,
	BC_LEX_NQUIT,
	BC_LEX_SCALE_FACTOR,
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

	BC_LEX_KW_ENTRY("if"      , 1), // 8
	BC_LEX_KW_ENTRY("last"    , 0), // 9
	BC_LEX_KW_ENTRY("length"  , 1), // 10
	BC_LEX_KW_ENTRY("limits"  , 0), // 11

	BC_LEX_KW_ENTRY("obase"   , 1), // 12
	BC_LEX_KW_ENTRY("print"   , 0), // 13
	BC_LEX_KW_ENTRY("quit"    , 1), // 14
	BC_LEX_KW_ENTRY("read"    , 0), // 15

	BC_LEX_KW_ENTRY("return"  , 1), // 16
	BC_LEX_KW_ENTRY("scale"   , 1), // 17
	BC_LEX_KW_ENTRY("sqrt"    , 1), // 18
	BC_LEX_KW_ENTRY("while"   , 1), // 19
};
#undef BC_LEX_KW_ENTRY
enum {
	POSIX_KWORD_MASK = 0
		| (1 << 0)
		| (1 << 1)
		| (0 << 2)
		| (1 << 3)
		\
		| (0 << 4)
		| (1 << 5)
		| (0 << 6)
		| (1 << 7)
		\
		| (1 << 8)
		| (0 << 9)
		| (1 << 10)
		| (0 << 11)
		\
		| (1 << 12)
		| (0 << 13)
		| (1 << 14)
		| (0 << 15)
		\
		| (1 << 16)
		| (1 << 17)
		| (1 << 18)
		| (1 << 19)
};
#define bc_lex_kws_POSIX(i) ((1 << (i)) & POSIX_KWORD_MASK)
#endif

struct BcLex;
typedef BcStatus (*BcLexNext)(struct BcLex *);

typedef struct BcLex {

	const char *buf;
	size_t i;
	size_t line;
	size_t len;
	bool newline;

	struct {
		BcLexType t;
		BcLexType last;
		BcVec v;
	} t;

	BcLexNext next;

} BcLex;

#define BC_PARSE_STREND ((char) UCHAR_MAX)

#define BC_PARSE_REL    (1 << 0)
#define BC_PARSE_PRINT  (1 << 1)
#define BC_PARSE_NOCALL (1 << 2)
#define BC_PARSE_NOREAD (1 << 3)
#define BC_PARSE_ARRAY  (1 << 4)

#define BC_PARSE_TOP_FLAG_PTR(parse) ((uint8_t *) bc_vec_top(&(parse)->flags))
#define BC_PARSE_TOP_FLAG(parse) (*(BC_PARSE_TOP_FLAG_PTR(parse)))

#define BC_PARSE_FLAG_FUNC_INNER (1 << 0)
#define BC_PARSE_FUNC_INNER(parse) \
	(BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_FUNC_INNER)

#define BC_PARSE_FLAG_FUNC (1 << 1)
#define BC_PARSE_FUNC(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_FUNC)

#define BC_PARSE_FLAG_BODY (1 << 2)
#define BC_PARSE_BODY(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_BODY)

#define BC_PARSE_FLAG_LOOP (1 << 3)
#define BC_PARSE_LOOP(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_LOOP)

#define BC_PARSE_FLAG_LOOP_INNER (1 << 4)
#define BC_PARSE_LOOP_INNER(parse) \
	(BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_LOOP_INNER)

#define BC_PARSE_FLAG_IF (1 << 5)
#define BC_PARSE_IF(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_IF)

#define BC_PARSE_FLAG_ELSE (1 << 6)
#define BC_PARSE_ELSE(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_ELSE)

#define BC_PARSE_FLAG_IF_END (1 << 7)
#define BC_PARSE_IF_END(parse) (BC_PARSE_TOP_FLAG(parse) & BC_PARSE_FLAG_IF_END)

#define BC_PARSE_CAN_EXEC(parse)                                             \
	(!(BC_PARSE_TOP_FLAG(parse) &                                            \
	   (BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_BODY | \
	    BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER | BC_PARSE_FLAG_IF |   \
	    BC_PARSE_FLAG_ELSE | BC_PARSE_FLAG_IF_END)))

struct BcParse;

struct BcProgram;

typedef BcStatus (*BcParseParse)(struct BcParse *);

typedef struct BcParse {

	BcParseParse parse;

	BcLex l;

	BcVec flags;

	BcVec exits;
	BcVec conds;

	BcVec ops;

	BcFunc *func;
	size_t fidx;

	size_t nbraces;
	bool auto_part;

} BcParse;

typedef struct BcProgram {

	size_t len;
	size_t scale;

	BcNum ib;
	size_t ib_t;
	BcNum ob;
	size_t ob_t;

	BcNum hexb;

#if ENABLE_DC
	BcNum strmb;
#endif

	BcVec results;
	BcVec stack;

	BcVec fns;
	BcVec fn_map;

	BcVec vars;
	BcVec var_map;

	BcVec arrs;
	BcVec arr_map;

	BcVec strs;
	BcVec consts;

	const char *file;

	BcNum last;
	BcNum zero;
	BcNum one;

	size_t nchars;

} BcProgram;

#define BC_PROG_STACK(s, n) ((s)->len >= ((size_t) n))

#define BC_PROG_MAIN (0)
#define BC_PROG_READ (1)
#if ENABLE_DC
#define BC_PROG_REQ_FUNCS (2)
#endif

#define BC_PROG_STR(n) (!(n)->num && !(n)->cap)
#define BC_PROG_NUM(r, n) \
	((r)->t != BC_RESULT_ARRAY && (r)->t != BC_RESULT_STR && !BC_PROG_STR(n))

typedef unsigned long (*BcProgramBuiltIn)(BcNum *);

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
	char sbgn;
	char send;

	BcParse prs;
	BcProgram prog;

	// For error messages. Can be set to current parsed line,
	// or [TODO] to current executing line (can be before last parsed one)
	unsigned err_line;

	BcVec files;

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

#if ENABLE_BC

// This is a bit array that corresponds to token types. An entry is
// true if the token is valid in an expression, false otherwise.
enum {
	BC_PARSE_EXPRS_BITS = 0
	+ ((uint64_t)((0 << 0)+(0 << 1)+(1 << 2)+(1 << 3)+(1 << 4)+(1 << 5)+(1 << 6)+(1 << 7)) << (0*8))
	+ ((uint64_t)((1 << 0)+(1 << 1)+(1 << 2)+(1 << 3)+(1 << 4)+(1 << 5)+(1 << 6)+(1 << 7)) << (1*8))
	+ ((uint64_t)((1 << 0)+(1 << 1)+(1 << 2)+(1 << 3)+(1 << 4)+(1 << 5)+(1 << 6)+(1 << 7)) << (2*8))
	+ ((uint64_t)((1 << 0)+(1 << 1)+(1 << 2)+(0 << 3)+(0 << 4)+(1 << 5)+(1 << 6)+(0 << 7)) << (3*8))
	+ ((uint64_t)((0 << 0)+(0 << 1)+(0 << 2)+(0 << 3)+(0 << 4)+(0 << 5)+(1 << 6)+(1 << 7)) << (4*8))
	+ ((uint64_t)((0 << 0)+(0 << 1)+(0 << 2)+(0 << 3)+(0 << 4)+(0 << 5)+(0 << 6)+(1 << 7)) << (5*8))
	+ ((uint64_t)((0 << 0)+(1 << 1)+(1 << 2)+(1 << 3)+(1 << 4)+(0 << 5)+(0 << 6)+(1 << 7)) << (6*8))
	+ ((uint64_t)((0 << 0)+(1 << 1)+(1 << 2)+(0 << 3)                                    ) << (7*8))
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

// This is an array of data for operators that correspond to token types.
static const uint8_t bc_parse_ops[] = {
#define OP(p,l) ((int)(l) * 0x10 + (p))
	OP(0, false), OP( 0, false ), // inc dec
	OP(1, false), // neg
	OP(2, false),
	OP(3, true ), OP( 3, true  ), OP( 3, true  ), // pow mul div
	OP(4, true ), OP( 4, true  ), // mod + -
	OP(6, true ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true  ), OP( 6, true ), // == <= >= != < >
	OP(1, false), // not
	OP(7, true ), OP( 7, true  ), // or and
	OP(5, false), OP( 5, false ), OP( 5, false ), OP( 5, false ), OP( 5, false ), // ^= *= /= %= +=
	OP(5, false), OP( 5, false ), // -= =
#undef OP
};
#define bc_parse_op_PREC(i) (bc_parse_ops[i] & 0x0f)
#define bc_parse_op_LEFT(i) (bc_parse_ops[i] & 0x10)

// Byte array of up to 4 BC_LEX's, packed into 32-bit word
typedef uint32_t BcParseNext;

// These identify what tokens can come after expressions in certain cases.
enum {
#define BC_PARSE_NEXT4(a,b,c,d) ( (a) | ((b)<<8) | ((c)<<16) | ((((d)|0x80)<<24)) )
#define BC_PARSE_NEXT2(a,b)     BC_PARSE_NEXT4(a,b,0xff,0xff)
#define BC_PARSE_NEXT1(a)       BC_PARSE_NEXT4(a,0xff,0xff,0xff)
	bc_parse_next_expr  = BC_PARSE_NEXT4(BC_LEX_NLINE,  BC_LEX_SCOLON, BC_LEX_RBRACE, BC_LEX_EOF),
	bc_parse_next_param = BC_PARSE_NEXT2(BC_LEX_RPAREN, BC_LEX_COMMA),
	bc_parse_next_print = BC_PARSE_NEXT4(BC_LEX_COMMA,  BC_LEX_NLINE,  BC_LEX_SCOLON, BC_LEX_EOF),
	bc_parse_next_rel   = BC_PARSE_NEXT1(BC_LEX_RPAREN),
	bc_parse_next_elem  = BC_PARSE_NEXT1(BC_LEX_RBRACKET),
	bc_parse_next_for   = BC_PARSE_NEXT1(BC_LEX_SCOLON),
	bc_parse_next_read  = BC_PARSE_NEXT2(BC_LEX_NLINE,  BC_LEX_EOF),
#undef BC_PARSE_NEXT4
#undef BC_PARSE_NEXT2
#undef BC_PARSE_NEXT1
};
#endif // ENABLE_BC

#if ENABLE_DC
static const //BcLexType - should be this type, but narrower type saves size:
uint8_t
dc_lex_regs[] = {
	BC_LEX_OP_REL_EQ, BC_LEX_OP_REL_LE, BC_LEX_OP_REL_GE, BC_LEX_OP_REL_NE,
	BC_LEX_OP_REL_LT, BC_LEX_OP_REL_GT, BC_LEX_SCOLON, BC_LEX_COLON,
	BC_LEX_ELSE, BC_LEX_LOAD, BC_LEX_LOAD_POP, BC_LEX_OP_ASSIGN,
	BC_LEX_STORE_PUSH,
};

static const //BcLexType - should be this type
uint8_t
dc_lex_tokens[] = {
	BC_LEX_OP_MODULUS, BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_LPAREN,
	BC_LEX_INVALID, BC_LEX_OP_MULTIPLY, BC_LEX_OP_PLUS, BC_LEX_INVALID,
	BC_LEX_OP_MINUS, BC_LEX_INVALID, BC_LEX_OP_DIVIDE,
	BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_COLON, BC_LEX_SCOLON, BC_LEX_OP_REL_GT, BC_LEX_OP_REL_EQ,
	BC_LEX_OP_REL_LT, BC_LEX_KEY_READ, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_EQ_NO_REG, BC_LEX_INVALID,
	BC_LEX_KEY_IBASE, BC_LEX_INVALID, BC_LEX_KEY_SCALE, BC_LEX_LOAD_POP,
	BC_LEX_INVALID, BC_LEX_OP_BOOL_NOT, BC_LEX_KEY_OBASE, BC_LEX_PRINT_STREAM,
	BC_LEX_NQUIT, BC_LEX_POP, BC_LEX_STORE_PUSH, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_SCALE_FACTOR, BC_LEX_INVALID,
	BC_LEX_KEY_LENGTH, BC_LEX_INVALID, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_OP_POWER, BC_LEX_NEG, BC_LEX_INVALID,
	BC_LEX_ASCIIFY, BC_LEX_INVALID, BC_LEX_CLEAR_STACK, BC_LEX_DUPLICATE,
	BC_LEX_ELSE, BC_LEX_PRINT_STACK, BC_LEX_INVALID, BC_LEX_INVALID,
	BC_LEX_STORE_IBASE, BC_LEX_INVALID, BC_LEX_STORE_SCALE, BC_LEX_LOAD,
	BC_LEX_INVALID, BC_LEX_PRINT_POP, BC_LEX_STORE_OBASE, BC_LEX_KEY_PRINT,
	BC_LEX_KEY_QUIT, BC_LEX_SWAP, BC_LEX_OP_ASSIGN, BC_LEX_INVALID,
	BC_LEX_INVALID, BC_LEX_KEY_SQRT, BC_LEX_INVALID, BC_LEX_EXECUTE,
	BC_LEX_INVALID, BC_LEX_STACK_LEVEL,
	BC_LEX_LBRACE, BC_LEX_OP_MODEXP, BC_LEX_INVALID, BC_LEX_OP_DIVMOD,
	BC_LEX_INVALID
};

static const //BcInst - should be this type. Using signed narrow type since BC_INST_INVALID is -1
int8_t
dc_parse_insts[] = {
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_REL_GE,
	BC_INST_INVALID, BC_INST_POWER, BC_INST_MULTIPLY, BC_INST_DIVIDE,
	BC_INST_MODULUS, BC_INST_PLUS, BC_INST_MINUS,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_BOOL_NOT, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_REL_GT, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_REL_GE,
	BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_IBASE,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_LENGTH, BC_INST_INVALID,
	BC_INST_OBASE, BC_INST_PRINT, BC_INST_QUIT, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_SCALE, BC_INST_SQRT, BC_INST_INVALID,
	BC_INST_REL_EQ, BC_INST_MODEXP, BC_INST_DIVMOD, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_EXECUTE, BC_INST_PRINT_STACK, BC_INST_CLEAR_STACK,
	BC_INST_STACK_LEN, BC_INST_DUPLICATE, BC_INST_SWAP, BC_INST_POP,
	BC_INST_ASCIIFY, BC_INST_PRINT_STREAM, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID, BC_INST_INVALID,
	BC_INST_PRINT, BC_INST_NQUIT, BC_INST_SCALE_FUNC,
};
#endif // ENABLE_DC

static const BcNumBinaryOp bc_program_ops[] = {
	bc_num_pow, bc_num_mul, bc_num_div, bc_num_mod, bc_num_add, bc_num_sub,
};

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
	exit(0);
}

static void bc_verror_msg(const char *fmt, va_list p)
{
	const char *sv = sv; /* for compiler */
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

static NOINLINE int bc_error_fmt(const char *fmt, ...)
{
	va_list p;

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	if (!ENABLE_FEATURE_CLEAN_UP && !G_ttyin)
		exit(1);
	return BC_STATUS_FAILURE;
}

#if ENABLE_BC
static NOINLINE int bc_posix_error_fmt(const char *fmt, ...)
{
	va_list p;

	// Are non-POSIX constructs totally ok?
	if (!(option_mask32 & (BC_FLAG_S|BC_FLAG_W)))
		return BC_STATUS_SUCCESS; // yes

	va_start(p, fmt);
	bc_verror_msg(fmt, p);
	va_end(p);

	// Do we treat non-POSIX constructs as errors?
	if (!(option_mask32 & BC_FLAG_S))
		return BC_STATUS_SUCCESS; // no, it's a warning
	if (!ENABLE_FEATURE_CLEAN_UP && !G_ttyin)
		exit(1);
	return BC_STATUS_FAILURE;
}
#endif

// We use error functions with "return bc_error(FMT[, PARAMS])" idiom.
// This idiom begs for tail-call optimization, but for it to work,
// function must not have caller-cleaned parameters on stack.
// Unfortunately, vararg function API does exactly that on most arches.
// Thus, use these shims for the cases when we have no vararg PARAMS:
static int bc_error(const char *msg)
{
	return bc_error_fmt("%s", msg);
}
#if ENABLE_BC
static int bc_POSIX_requires(const char *msg)
{
	return bc_posix_error_fmt("POSIX requires %s", msg);
}
static int bc_POSIX_does_not_allow(const char *msg)
{
	return bc_posix_error_fmt("%s%s", "POSIX does not allow ", msg);
}
static int bc_POSIX_does_not_allow_bool_ops_this_is_bad(const char *msg)
{
	return bc_posix_error_fmt("%s%s %s", "POSIX does not allow ", "boolean operators; the following is bad:", msg);
}
static int bc_POSIX_does_not_allow_empty_X_expression_in_for(const char *msg)
{
	return bc_posix_error_fmt("%san empty %s expression in a for loop", "POSIX does not allow ", msg);
}
#endif
static int bc_error_bad_character(char c)
{
	return bc_error_fmt("bad character '%c'", c);
}
static int bc_error_bad_expression(void)
{
	return bc_error("bad expression");
}
static int bc_error_bad_token(void)
{
	return bc_error("bad token");
}
static int bc_error_stack_has_too_few_elements(void)
{
	return bc_error("stack has too few elements");
}
static int bc_error_variable_is_wrong_type(void)
{
	return bc_error("variable is wrong type");
}
static int bc_error_nested_read_call(void)
{
	return bc_error("read() call inside of a read() call");
}

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

static void bc_vec_push(BcVec *v, const void *data)
{
	if (v->len + 1 > v->cap) bc_vec_grow(v, 1);
	memmove(v->v + (v->size * v->len), data, v->size);
	v->len += 1;
}

static void bc_vec_pushByte(BcVec *v, char data)
{
	bc_vec_push(v, &data);
}

static void bc_vec_pushZeroByte(BcVec *v)
{
	//bc_vec_pushByte(v, '\0');
	// better:
	bc_vec_push(v, &const_int_0);
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

static void bc_vec_concat(BcVec *v, const char *str)
{
	size_t len;

	if (v->len == 0) bc_vec_pushZeroByte(v);

	len = v->len + strlen(str);

	if (v->cap < len) bc_vec_grow(v, len - v->len);
	strcpy(v->v + v->len - 1, str);

	v->len = len;
}

static void *bc_vec_item(const BcVec *v, size_t idx)
{
	return v->v + v->size * idx;
}

static char** bc_program_str(size_t idx)
{
	return bc_vec_item(&G.prog.strs, idx);
}

static BcFunc* bc_program_func(size_t idx)
{
	return bc_vec_item(&G.prog.fns, idx);
}

static void *bc_vec_item_rev(const BcVec *v, size_t idx)
{
	return v->v + v->size * (v->len - idx - 1);
}

static void *bc_vec_top(const BcVec *v)
{
	return v->v + v->size * (v->len - 1);
}

static void bc_vec_free(void *vec)
{
	BcVec *v = (BcVec *) vec;
	bc_vec_pop_all(v);
	free(v->v);
}

static int bc_id_cmp(const void *e1, const void *e2)
{
	return strcmp(((const BcId *) e1)->name, ((const BcId *) e2)->name);
}

static void bc_id_free(void *id)
{
	free(((BcId *) id)->name);
}

static size_t bc_map_find(const BcVec *v, const void *ptr)
{
	size_t low = 0, high = v->len;

	while (low < high) {

		size_t mid = (low + high) / 2;
		BcId *id = bc_vec_item(v, mid);
		int result = bc_id_cmp(ptr, id);

		if (result == 0)
			return mid;
		else if (result < 0)
			high = mid;
		else
			low = mid + 1;
	}

	return low;
}

static int bc_map_insert(BcVec *v, const void *ptr, size_t *i)
{
	size_t n = *i = bc_map_find(v, ptr);

	if (n == v->len)
		bc_vec_push(v, ptr);
	else if (!bc_id_cmp(ptr, bc_vec_item(v, n)))
		return 0; // "was not inserted"
	else
		bc_vec_pushAt(v, ptr, n);
	return 1; // "was inserted"
}

#if ENABLE_BC
static size_t bc_map_index(const BcVec *v, const void *ptr)
{
	size_t i = bc_map_find(v, ptr);
	if (i >= v->len) return BC_VEC_INVALID_IDX;
	return bc_id_cmp(ptr, bc_vec_item(v, i)) ? BC_VEC_INVALID_IDX : i;
}
#endif

static int push_input_byte(BcVec *vec, char c)
{
	if ((c < ' ' && c != '\t' && c != '\r' && c != '\n') // also allow '\v' '\f'?
	 || c > 0x7e
	) {
		// Bad chars on this line, ignore entire line
		bc_error_fmt("illegal character 0x%02x", c);
		return 1;
	}
	bc_vec_pushByte(vec, (char)c);
	return 0;
}

static BcStatus bc_read_line(BcVec *vec)
{
	BcStatus s;
	bool bad_chars;

	s = BC_STATUS_SUCCESS;
	do {
		int c;

		bad_chars = 0;
		bc_vec_pop_all(vec);

		fflush_and_check();

#if ENABLE_FEATURE_BC_SIGNALS
		if (G_interrupt) { // ^C was pressed
 intr:
			G_interrupt = 0;
			// GNU bc says "interrupted execution."
			// GNU dc says "Interrupt!"
			fputs("\ninterrupted execution\n", stderr);
		}
# if ENABLE_FEATURE_EDITING
		if (G_ttyin) {
			int n, i;
#  define line_buf bb_common_bufsiz1
			n = read_line_input(G.line_input_state, "", line_buf, COMMON_BUFSIZE);
			if (n <= 0) { // read errors or EOF, or ^D, or ^C
				if (n == 0) // ^C
					goto intr;
				s = BC_STATUS_EOF;
				break;
			}
			i = 0;
			for (;;) {
				c = line_buf[i++];
				if (!c) break;
				bad_chars |= push_input_byte(vec, c);
			}
#  undef line_buf
		} else
# endif
#endif
		{
			IF_FEATURE_BC_SIGNALS(errno = 0;)
			do {
				c = fgetc(stdin);
#if ENABLE_FEATURE_BC_SIGNALS && !ENABLE_FEATURE_EDITING
				// Both conditions appear simultaneously, check both just in case
				if (errno == EINTR || G_interrupt) {
					// ^C was pressed
					clearerr(stdin);
					goto intr;
				}
#endif
				if (c == EOF) {
					if (ferror(stdin))
						quit(); // this emits error message
					s = BC_STATUS_EOF;
					// Note: EOF does not append '\n', therefore:
					// printf 'print 123\n' | bc - works
					// printf 'print 123' | bc   - fails (syntax error)
					break;
				}
				bad_chars |= push_input_byte(vec, c);
			} while (c != '\n');
		}
	} while (bad_chars);

	bc_vec_pushZeroByte(vec);

	return s;
}

static char* bc_read_file(const char *path)
{
	char *buf;
	size_t size = ((size_t) -1);
	size_t i;

	// Never returns NULL (dies on errors)
	buf = xmalloc_xopen_read_close(path, &size);

	for (i = 0; i < size; ++i) {
		char c = buf[i];
		if ((c < ' ' && c != '\t' && c != '\r' && c != '\n') // also allow '\v' '\f'?
		 || c > 0x7e
		) {
			free(buf);
			buf = NULL;
			break;
		}
	}

	return buf;
}

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

static void bc_num_ten(BcNum *n)
{
	bc_num_setToZero(n, 0);
	n->len = 2;
	n->num[0] = 0;
	n->num[1] = 1;
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

static void bc_num_free(void *num)
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

static BcStatus bc_num_ulong(BcNum *n, unsigned long *result_p)
{
	size_t i;
	unsigned long pow, result;

	if (n->neg) return bc_error("negative number");

	for (result = 0, pow = 1, i = n->rdx; i < n->len; ++i) {

		unsigned long prev = result, powprev = pow;

		result += ((unsigned long) n->num[i]) * pow;
		pow *= 10;

		if (result < prev || pow < powprev)
			return bc_error("overflow");
		prev = result;
		powprev = pow;
	}
	*result_p = result;

	return BC_STATUS_SUCCESS;
}

static void bc_num_ulong2num(BcNum *n, unsigned long val)
{
	BcDig *ptr;

	bc_num_zero(n);

	if (val == 0) return;

	if (ULONG_MAX == 0xffffffffUL)
		bc_num_expand(n, 10); // 10 digits: 4294967295
	if (ULONG_MAX == 0xffffffffffffffffUL)
		bc_num_expand(n, 20); // 20 digits: 18446744073709551615
	BUILD_BUG_ON(ULONG_MAX > 0xffffffffffffffffUL);

	ptr = n->num;
	for (;;) {
		n->len++;
		*ptr++ = val % 10;
		val /= 10;
		if (val == 0) break;
	}
}

static void bc_num_subArrays(BcDig *restrict a, BcDig *restrict b,
                                 size_t len)
{
	size_t i, j;
	for (i = 0; i < len; ++i) {
		for (a[i] -= b[i], j = 0; a[i + j] < 0;) {
			a[i + j++] += 10;
			a[i + j] -= 1;
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

static ssize_t bc_num_compare(BcDig *restrict a, BcDig *restrict b, size_t len)
{
	size_t i;
	int c = 0;
	for (i = len - 1; i < len && !(c = a[i] - b[i]); --i);
	return BC_NUM_NEG(i + 1, c < 0);
}

static ssize_t bc_num_cmp(BcNum *a, BcNum *b)
{
	size_t i, min, a_int, b_int, diff;
	BcDig *max_num, *min_num;
	bool a_max, neg = false;
	ssize_t cmp;

	if (a == b) return 0;
	if (a->len == 0) return BC_NUM_NEG(!!b->len, !b->neg);
	if (b->len == 0) return BC_NUM_NEG(1, a->neg);
	if (a->neg) {
		if (b->neg)
			neg = true;
		else
			return -1;
	}
	else if (b->neg)
		return 1;

	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);
	a_int -= b_int;
	a_max = (a->rdx > b->rdx);

	if (a_int != 0) return (ssize_t) a_int;

	if (a_max) {
		min = b->rdx;
		diff = a->rdx - b->rdx;
		max_num = a->num + diff;
		min_num = b->num;
	}
	else {
		min = a->rdx;
		diff = b->rdx - a->rdx;
		max_num = b->num + diff;
		min_num = a->num;
	}

	cmp = bc_num_compare(max_num, min_num, b_int + min);
	if (cmp != 0) return BC_NUM_NEG(cmp, (!a_max) != neg);

	for (max_num -= diff, i = diff - 1; i < diff; --i) {
		if (max_num[i]) return BC_NUM_NEG(1, (!a_max) != neg);
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
	}
	else {
		bc_num_zero(b);
		bc_num_copy(a, n);
	}

	bc_num_clean(a);
	bc_num_clean(b);
}

static BcStatus bc_num_shift(BcNum *n, size_t places)
{
	if (places == 0 || n->len == 0) return BC_STATUS_SUCCESS;

	// This check makes sense only if size_t is (much) larger than BC_MAX_NUM.
	if (SIZE_MAX > (BC_MAX_NUM | 0xff)) {
		if (places + n->len > BC_MAX_NUM)
			return bc_error("number too long: must be [1,"BC_MAX_NUM_STR"]");
	}

	if (n->rdx >= places)
		n->rdx -= places;
	else {
		bc_num_extend(n, places - n->rdx);
		n->rdx = 0;
	}

	bc_num_clean(n);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_num_inv(BcNum *a, BcNum *b, size_t scale)
{
	BcNum one;
	BcDig num[2];

	one.cap = 2;
	one.num = num;
	bc_num_one(&one);

	return bc_num_div(&one, a, b, scale);
}

static BcStatus bc_num_a(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
{
	BcDig *ptr, *ptr_a, *ptr_b, *ptr_c;
	size_t i, max, min_rdx, min_int, diff, a_int, b_int;
	int carry, in;

	// Because this function doesn't need to use scale (per the bc spec),
	// I am hijacking it to say whether it's doing an add or a subtract.

	if (a->len == 0) {
		bc_num_copy(c, b);
		if (sub && c->len) c->neg = !c->neg;
		return BC_STATUS_SUCCESS;
	}
	else if (b->len == 0) {
		bc_num_copy(c, a);
		return BC_STATUS_SUCCESS;
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
	}
	else {
		diff = b->rdx - a->rdx;
		ptr = b->num;
		ptr_a = a->num;
		ptr_b = b->num + diff;
	}

	for (ptr_c = c->num, i = 0; i < diff; ++i, ++c->len) ptr_c[i] = ptr[i];

	ptr_c += diff;
	a_int = BC_NUM_INT(a);
	b_int = BC_NUM_INT(b);

	if (a_int > b_int) {
		min_int = b_int;
		max = a_int;
		ptr = ptr_a;
	}
	else {
		min_int = a_int;
		max = b_int;
		ptr = ptr_b;
	}

	for (carry = 0, i = 0; i < min_rdx + min_int; ++i, ++c->len) {
		in = ((int) ptr_a[i]) + ((int) ptr_b[i]) + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}

	for (; i < max + min_rdx; ++i, ++c->len) {
		in = ((int) ptr[i]) + carry;
		carry = in / 10;
		ptr_c[i] = (BcDig)(in % 10);
	}

	if (carry != 0) c->num[c->len++] = (BcDig) carry;

	return BC_STATUS_SUCCESS; // can't make void, see bc_num_binary()
}

static BcStatus bc_num_s(BcNum *a, BcNum *b, BcNum *restrict c, size_t sub)
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
		return BC_STATUS_SUCCESS;
	}
	else if (b->len == 0) {
		bc_num_copy(c, a);
		return BC_STATUS_SUCCESS;
	}

	aneg = a->neg;
	bneg = b->neg;
	a->neg = b->neg = false;

	cmp = bc_num_cmp(a, b);

	a->neg = aneg;
	b->neg = bneg;

	if (cmp == 0) {
		bc_num_setToZero(c, BC_MAX(a->rdx, b->rdx));
		return BC_STATUS_SUCCESS;
	}
	else if (cmp > 0) {
		neg = a->neg;
		minuend = a;
		subtrahend = b;
	}
	else {
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
	}
	else
		start = c->rdx - subtrahend->rdx;

	bc_num_subArrays(c->num + start, subtrahend->num, subtrahend->len);

	bc_num_clean(c);

	return BC_STATUS_SUCCESS; // can't make void, see bc_num_binary()
}

static BcStatus bc_num_k(BcNum *restrict a, BcNum *restrict b,
                         BcNum *restrict c)
{
	BcStatus s;
	size_t max = BC_MAX(a->len, b->len), max2 = (max + 1) / 2;
	BcNum l1, h1, l2, h2, m2, m1, z0, z1, z2, temp;
	bool aone;

	if (a->len == 0 || b->len == 0) {
		bc_num_zero(c);
		return BC_STATUS_SUCCESS;
	}
	aone = BC_NUM_ONE(a);
	if (aone || BC_NUM_ONE(b)) {
		bc_num_copy(c, aone ? b : a);
		return BC_STATUS_SUCCESS;
	}

	if (a->len + b->len < BC_NUM_KARATSUBA_LEN ||
	    a->len < BC_NUM_KARATSUBA_LEN || b->len < BC_NUM_KARATSUBA_LEN)
	{
		size_t i, j, len;
		unsigned carry;

		bc_num_expand(c, a->len + b->len + 1);

		memset(c->num, 0, sizeof(BcDig) * c->cap);
		c->len = len = 0;

		for (i = 0; i < b->len; ++i) {

			carry = 0;
			for (j = 0; j < a->len; ++j) {
				unsigned in = c->num[i + j];
				in += ((unsigned) a->num[j]) * ((unsigned) b->num[i]) + carry;
				// note: compilers prefer _unsigned_ div/const
				carry = in / 10;
				c->num[i + j] = (BcDig)(in % 10);
			}

			c->num[i + j] += (BcDig) carry;
			len = BC_MAX(len, i + j + !!carry);

			// a=2^1000000
			// a*a <- without check below, this will not be interruptible
			if (G_interrupt) return BC_STATUS_FAILURE;
		}

		c->len = len;

		return BC_STATUS_SUCCESS;
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

	s = bc_num_add(&h1, &l1, &m1, 0);
	if (s) goto err;
	s = bc_num_add(&h2, &l2, &m2, 0);
	if (s) goto err;

	s = bc_num_k(&h1, &h2, &z0);
	if (s) goto err;
	s = bc_num_k(&m1, &m2, &z1);
	if (s) goto err;
	s = bc_num_k(&l1, &l2, &z2);
	if (s) goto err;

	s = bc_num_sub(&z1, &z0, &temp, 0);
	if (s) goto err;
	s = bc_num_sub(&temp, &z2, &z1, 0);
	if (s) goto err;

	s = bc_num_shift(&z0, max2 * 2);
	if (s) goto err;
	s = bc_num_shift(&z1, max2);
	if (s) goto err;
	s = bc_num_add(&z0, &z1, &temp, 0);
	if (s) goto err;
	s = bc_num_add(&temp, &z2, c, 0);

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
	return s;
}

static BcStatus bc_num_m(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
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

	s = bc_num_shift(&cpa, maxrdx);
	if (s) goto err;
	s = bc_num_shift(&cpb, maxrdx);
	if (s) goto err;
	s = bc_num_k(&cpa, &cpb, c);
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
	return s;
}

static BcStatus bc_num_d(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcDig *n, *p, q;
	size_t len, end, i;
	BcNum cp;
	bool zero = true;

	if (b->len == 0)
		return bc_error("divide by zero");
	else if (a->len == 0) {
		bc_num_setToZero(c, scale);
		return BC_STATUS_SUCCESS;
	}
	else if (BC_NUM_ONE(b)) {
		bc_num_copy(c, a);
		bc_num_retireMul(c, scale, a->neg, b->neg);
		return BC_STATUS_SUCCESS;
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
		for (i = 0; zero && i < len; ++i) zero = !b->num[len - i - 1];
		len -= i - 1;
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
	p = b->num;

	for (i = end - 1; !s && i < end; --i) {
		n = cp.num + i;
		for (q = 0; (!s && n[len] != 0) || bc_num_compare(n, p, len) >= 0; ++q)
			bc_num_subArrays(n, p, len);
		c->num[i] = q;
		// a=2^100000
		// scale=40000
		// 1/a <- without check below, this will not be interruptible
		if (G_interrupt) {
			s = BC_STATUS_FAILURE;
			break;
		}
	}

	bc_num_retireMul(c, scale, a->neg, b->neg);
	bc_num_free(&cp);

	return s;
}

static BcStatus bc_num_r(BcNum *a, BcNum *b, BcNum *restrict c,
                         BcNum *restrict d, size_t scale, size_t ts)
{
	BcStatus s;
	BcNum temp;
	bool neg;

	if (b->len == 0)
		return bc_error("divide by zero");

	if (a->len == 0) {
		bc_num_setToZero(d, ts);
		return BC_STATUS_SUCCESS;
	}

	bc_num_init(&temp, d->cap);
	s = bc_num_d(a, b, c, scale);
	if (s) goto err;

	if (scale != 0) scale = ts;

	s = bc_num_m(c, b, &temp, scale);
	if (s) goto err;
	s = bc_num_sub(a, &temp, d, scale);
	if (s) goto err;

	if (ts > d->rdx && d->len) bc_num_extend(d, ts - d->rdx);

	neg = d->neg;
	bc_num_retireMul(d, ts, a->neg, b->neg);
	d->neg = neg;

err:
	bc_num_free(&temp);
	return s;
}

static BcStatus bc_num_rem(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s;
	BcNum c1;
	size_t ts = BC_MAX(scale + b->rdx, a->rdx), len = BC_NUM_MREQ(a, b, ts);

	bc_num_init(&c1, len);
	s = bc_num_r(a, b, &c1, c, scale, ts);
	bc_num_free(&c1);

	return s;
}

static BcStatus bc_num_p(BcNum *a, BcNum *b, BcNum *restrict c, size_t scale)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcNum copy;
	unsigned long pow;
	size_t i, powrdx, resrdx;
	bool neg, zero;

	if (b->rdx) return bc_error("non integer number");

	if (b->len == 0) {
		bc_num_one(c);
		return BC_STATUS_SUCCESS;
	}
	else if (a->len == 0) {
		bc_num_setToZero(c, scale);
		return BC_STATUS_SUCCESS;
	}
	else if (BC_NUM_ONE(b)) {
		if (!b->neg)
			bc_num_copy(c, a);
		else
			s = bc_num_inv(a, c, scale);
		return s;
	}

	neg = b->neg;
	b->neg = false;

	s = bc_num_ulong(b, &pow);
	if (s) return s;

	bc_num_init(&copy, a->len);
	bc_num_copy(&copy, a);

	if (!neg) {
		if (a->rdx > scale)
			scale = a->rdx;
		if (a->rdx * pow < scale)
			scale = a->rdx * pow;
	}

	b->neg = neg;

	for (powrdx = a->rdx; !(pow & 1); pow >>= 1) {
		powrdx <<= 1;
		s = bc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;
		// Not needed: bc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	bc_num_copy(c, &copy);

	for (resrdx = powrdx, pow >>= 1; pow != 0; pow >>= 1) {

		powrdx <<= 1;
		s = bc_num_mul(&copy, &copy, &copy, powrdx);
		if (s) goto err;

		if (pow & 1) {
			resrdx += powrdx;
			s = bc_num_mul(c, &copy, c, resrdx);
			if (s) goto err;
		}
		// Not needed: bc_num_mul() has a check for ^C:
		//if (G_interrupt) {
		//	s = BC_STATUS_FAILURE;
		//	goto err;
		//}
	}

	if (neg) {
		s = bc_num_inv(c, c, scale);
		if (s) goto err;
	}

	if (c->rdx > scale) bc_num_truncate(c, c->rdx - scale);

	// We can't use bc_num_clean() here.
	for (zero = true, i = 0; zero && i < c->len; ++i) zero = !c->num[i];
	if (zero) bc_num_setToZero(c, scale);

err:
	bc_num_free(&copy);
	return s;
}

static BcStatus bc_num_binary(BcNum *a, BcNum *b, BcNum *c, size_t scale,
                              BcNumBinaryOp op, size_t req)
{
	BcStatus s;
	BcNum num2, *ptr_a, *ptr_b;
	bool init = false;

	if (c == a) {
		ptr_a = &num2;
		memcpy(ptr_a, c, sizeof(BcNum));
		init = true;
	}
	else
		ptr_a = a;

	if (c == b) {
		ptr_b = &num2;
		if (c != a) {
			memcpy(ptr_b, c, sizeof(BcNum));
			init = true;
		}
	}
	else
		ptr_b = b;

	if (init)
		bc_num_init(c, req);
	else
		bc_num_expand(c, req);

	s = op(ptr_a, ptr_b, c, scale);

	if (init) bc_num_free(&num2);

	return s;
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
static void bc_num_printChar(size_t num, size_t width, bool radix)
{
	(void) radix;
	bb_putchar((char) num);
	G.prog.nchars += width;
}
#endif

static void bc_num_printDigits(size_t num, size_t width, bool radix)
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

static void bc_num_printHex(size_t num, size_t width, bool radix)
{
	if (radix) {
		bc_num_printNewline();
		bb_putchar('.');
		G.prog.nchars += 1;
	}

	bc_num_printNewline();
	bb_putchar(bb_hexdigits_upcase[num]);
	G.prog.nchars += width;
}

static void bc_num_printDecimal(BcNum *n)
{
	size_t i, rdx = n->rdx - 1;

	if (n->neg) bb_putchar('-');
	G.prog.nchars += n->neg;

	for (i = n->len - 1; i < n->len; --i)
		bc_num_printHex((size_t) n->num[i], 1, i == rdx);
}

static BcStatus bc_num_printNum(BcNum *n, BcNum *base, size_t width, BcNumDigitOp print)
{
	BcStatus s;
	BcVec stack;
	BcNum intp, fracp, digit, frac_len;
	unsigned long dig, *ptr;
	size_t i;
	bool radix;

	if (n->len == 0) {
		print(0, width, false);
		return BC_STATUS_SUCCESS;
	}

	bc_vec_init(&stack, sizeof(long), NULL);
	bc_num_init(&intp, n->len);
	bc_num_init(&fracp, n->rdx);
	bc_num_init(&digit, width);
	bc_num_init(&frac_len, BC_NUM_INT(n));
	bc_num_copy(&intp, n);
	bc_num_one(&frac_len);

	bc_num_truncate(&intp, intp.rdx);
	s = bc_num_sub(n, &intp, &fracp, 0);
	if (s) goto err;

	while (intp.len != 0) {
		s = bc_num_divmod(&intp, base, &intp, &digit, 0);
		if (s) goto err;
		s = bc_num_ulong(&digit, &dig);
		if (s) goto err;
		bc_vec_push(&stack, &dig);
	}

	for (i = 0; i < stack.len; ++i) {
		ptr = bc_vec_item_rev(&stack, i);
		print(*ptr, width, false);
	}

	if (!n->rdx) goto err;

	for (radix = true; frac_len.len <= n->rdx; radix = false) {
		s = bc_num_mul(&fracp, base, &fracp, n->rdx);
		if (s) goto err;
		s = bc_num_ulong(&fracp, &dig);
		if (s) goto err;
		bc_num_ulong2num(&intp, dig);
		s = bc_num_sub(&fracp, &intp, &fracp, 0);
		if (s) goto err;
		print(dig, width, radix);
		s = bc_num_mul(&frac_len, base, &frac_len, 0);
		if (s) goto err;
	}

err:
	bc_num_free(&frac_len);
	bc_num_free(&digit);
	bc_num_free(&fracp);
	bc_num_free(&intp);
	bc_vec_free(&stack);
	return s;
}

static BcStatus bc_num_printBase(BcNum *n)
{
	BcStatus s;
	size_t width, i;
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
	}
	else {
		for (i = G.prog.ob_t - 1, width = 0; i != 0; i /= 10, ++width)
			continue;
		print = bc_num_printDigits;
	}

	s = bc_num_printNum(n, &G.prog.ob, width, print);
	n->neg = neg;

	return s;
}

#if ENABLE_DC
static BcStatus bc_num_stream(BcNum *n, BcNum *base)
{
	return bc_num_printNum(n, base, 1, bc_num_printChar);
}
#endif

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
	bool zero;

	len = strlen(val);
	if (len == 0)
		return;

	zero = true;
	for (i = 0; val[i]; ++i) {
		if (val[i] != '0' && val[i] != '.') {
			zero = false;
			break;
		}
	}
	bc_num_expand(n, len);

	ptr = strchr(val, '.');

	n->rdx = 0;
	if (ptr != NULL)
		n->rdx = (size_t)((val + len) - (ptr + 1));

	if (!zero) {
		i = len - 1;
		for (;;) {
			n->num[n->len] = val[i] - '0';
			++n->len;
 skip_dot:
			if ((ssize_t)--i == (ssize_t)-1) break;
			if (val[i] == '.') goto skip_dot;
		}
	}
}

// Note: n is already "bc_num_zero()"ed,
// leading zeroes in "val" are removed
static void bc_num_parseBase(BcNum *n, const char *val, BcNum *base)
{
	BcStatus s;
	BcNum temp, mult, result;
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

	for (;;) {
		c = *val++;
		if (c == '\0') goto int_err;
		if (c == '.') break;

		v = (unsigned long) (c <= '9' ? c - '0' : c - 'A' + 10);

		s = bc_num_mul(n, base, &mult, 0);
		if (s) goto int_err;
		bc_num_ulong2num(&temp, v);
		s = bc_num_add(&mult, &temp, n, 0);
		if (s) goto int_err;
	}

	bc_num_init(&result, base->len);
	//bc_num_zero(&result); - already is
	bc_num_one(&mult);

	digits = 0;
	for (;;) {
		c = *val++;
		if (c == '\0') break;
		digits++;

		v = (unsigned long) (c <= '9' ? c - '0' : c - 'A' + 10);

		s = bc_num_mul(&result, base, &result, 0);
		if (s) goto err;
		bc_num_ulong2num(&temp, v);
		s = bc_num_add(&result, &temp, &result, 0);
		if (s) goto err;
		s = bc_num_mul(&mult, base, &mult, 0);
		if (s) goto err;
	}

	s = bc_num_div(&result, &mult, &result, digits);
	if (s) goto err;
	s = bc_num_add(n, &result, n, digits);
	if (s) goto err;

	if (n->len != 0) {
		if (n->rdx < digits) bc_num_extend(n, digits - n->rdx);
	} else
		bc_num_zero(n);

err:
	bc_num_free(&result);
int_err:
	bc_num_free(&mult);
	bc_num_free(&temp);
}

static BcStatus bc_num_parse(BcNum *n, const char *val, BcNum *base,
                             size_t base_t)
{
	if (!bc_num_strValid(val, base_t))
		return bc_error("bad number string");

	bc_num_zero(n);
	while (*val == '0') val++;

	if (base_t == 10)
		bc_num_parseDecimal(n, val);
	else
		bc_num_parseBase(n, val, base);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_num_print(BcNum *n, bool newline)
{
	BcStatus s = BC_STATUS_SUCCESS;

	bc_num_printNewline();

	if (n->len == 0) {
		bb_putchar('0');
		++G.prog.nchars;
	}
	else if (G.prog.ob_t == 10)
		bc_num_printDecimal(n);
	else
		s = bc_num_printBase(n);

	if (newline) {
		bb_putchar('\n');
		G.prog.nchars = 0;
	}

	return s;
}

static BcStatus bc_num_add(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? bc_num_a : bc_num_s;
	(void) scale;
	return bc_num_binary(a, b, c, false, op, BC_NUM_AREQ(a, b));
}

static BcStatus bc_num_sub(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	BcNumBinaryOp op = (!a->neg == !b->neg) ? bc_num_s : bc_num_a;
	(void) scale;
	return bc_num_binary(a, b, c, true, op, BC_NUM_AREQ(a, b));
}

static BcStatus bc_num_mul(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	return bc_num_binary(a, b, c, scale, bc_num_m, req);
}

static BcStatus bc_num_div(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	return bc_num_binary(a, b, c, scale, bc_num_d, req);
}

static BcStatus bc_num_mod(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	size_t req = BC_NUM_MREQ(a, b, scale);
	return bc_num_binary(a, b, c, scale, bc_num_rem, req);
}

static BcStatus bc_num_pow(BcNum *a, BcNum *b, BcNum *c, size_t scale)
{
	return bc_num_binary(a, b, c, scale, bc_num_p, a->len * b->len + 1);
}

static BcStatus bc_num_sqrt(BcNum *a, BcNum *restrict b, size_t scale)
{
	BcStatus s;
	BcNum num1, num2, half, f, fprime, *x0, *x1, *temp;
	size_t pow, len, digs, digs1, resrdx, req, times = 0;
	ssize_t cmp = 1, cmp1 = SSIZE_MAX, cmp2 = SSIZE_MAX;

	req = BC_MAX(scale, a->rdx) + ((BC_NUM_INT(a) + 1) >> 1) + 1;
	bc_num_expand(b, req);

	if (a->len == 0) {
		bc_num_setToZero(b, scale);
		return BC_STATUS_SUCCESS;
	}
	else if (a->neg)
		return bc_error("negative number");
	else if (BC_NUM_ONE(a)) {
		bc_num_one(b);
		bc_num_extend(b, scale);
		return BC_STATUS_SUCCESS;
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

		s = bc_num_div(a, x0, &f, resrdx);
		if (s) goto err;
		s = bc_num_add(x0, &f, &fprime, resrdx);
		if (s) goto err;
		s = bc_num_mul(&fprime, &half, x1, resrdx);
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
	return s;
}

static BcStatus bc_num_divmod(BcNum *a, BcNum *b, BcNum *c, BcNum *d,
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
	}
	else {
		ptr_a = a;
		bc_num_expand(c, len);
	}

	s = bc_num_r(ptr_a, b, c, d, scale, ts);

	if (init) bc_num_free(&num2);

	return s;
}

#if ENABLE_DC
static BcStatus bc_num_modexp(BcNum *a, BcNum *b, BcNum *c, BcNum *restrict d)
{
	BcStatus s;
	BcNum base, exp, two, temp;

	if (c->len == 0)
		return bc_error("divide by zero");
	if (a->rdx || b->rdx || c->rdx)
		return bc_error("non integer number");
	if (b->neg)
		return bc_error("negative number");

	bc_num_expand(d, c->len);
	bc_num_init(&base, c->len);
	bc_num_init(&exp, b->len);
	bc_num_init_DEF_SIZE(&two);
	bc_num_init(&temp, b->len);

	bc_num_one(&two);
	two.num[0] = 2;
	bc_num_one(d);

	s = bc_num_rem(a, c, &base, 0);
	if (s) goto err;
	bc_num_copy(&exp, b);

	while (exp.len != 0) {

		s = bc_num_divmod(&exp, &two, &exp, &temp, 0);
		if (s) goto err;

		if (BC_NUM_ONE(&temp)) {
			s = bc_num_mul(d, &base, &temp, 0);
			if (s) goto err;
			s = bc_num_rem(&temp, c, d, 0);
			if (s) goto err;
		}

		s = bc_num_mul(&base, &base, &temp, 0);
		if (s) goto err;
		s = bc_num_rem(&temp, c, &base, 0);
		if (s) goto err;
	}

err:
	bc_num_free(&temp);
	bc_num_free(&two);
	bc_num_free(&exp);
	bc_num_free(&base);
	return s;
}
#endif // ENABLE_DC

#if ENABLE_BC
static BcStatus bc_func_insert(BcFunc *f, char *name, bool var)
{
	BcId a;
	size_t i;

	for (i = 0; i < f->autos.len; ++i) {
		if (strcmp(name, ((BcId *) bc_vec_item(&f->autos, i))->name) == 0)
			return bc_error("function parameter or auto var has the same name as another");
	}

	a.idx = var;
	a.name = name;

	bc_vec_push(&f->autos, &a);

	return BC_STATUS_SUCCESS;
}
#endif

static void bc_func_init(BcFunc *f)
{
	bc_char_vec_init(&f->code);
	bc_vec_init(&f->autos, sizeof(BcId), bc_id_free);
	bc_vec_init(&f->labels, sizeof(size_t), NULL);
	f->nparams = 0;
}

static void bc_func_free(void *func)
{
	BcFunc *f = (BcFunc *) func;
	bc_vec_free(&f->code);
	bc_vec_free(&f->autos);
	bc_vec_free(&f->labels);
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
	BcResultData data;

	if (a->size == sizeof(BcNum) && a->dtor == bc_num_free) {
		while (len > a->len) {
			bc_num_init_DEF_SIZE(&data.n);
			bc_vec_push(a, &data.n);
		}
	}
	else {
		while (len > a->len) {
			bc_array_init(&data.v, true);
			bc_vec_push(a, &data.v);
		}
	}
}

static void bc_array_copy(BcVec *d, const BcVec *s)
{
	size_t i;

	bc_vec_pop_all(d);
	bc_vec_expand(d, s->cap);
	d->len = s->len;

	for (i = 0; i < s->len; ++i) {
		BcNum *dnum = bc_vec_item(d, i), *snum = bc_vec_item(s, i);
		bc_num_init(dnum, snum->len);
		bc_num_copy(dnum, snum);
	}
}

static void bc_string_free(void *string)
{
	free(*((char **) string));
}

#if ENABLE_DC
static void bc_result_copy(BcResult *d, BcResult *src)
{
	d->t = src->t;

	switch (d->t) {

		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
		{
			bc_num_init(&d->d.n, src->d.n.len);
			bc_num_copy(&d->d.n, &src->d.n);
			break;
		}

		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
		{
			d->d.id.name = xstrdup(src->d.id.name);
			break;
		}

		case BC_RESULT_CONSTANT:
		case BC_RESULT_LAST:
		case BC_RESULT_ONE:
		case BC_RESULT_STR:
		{
			memcpy(&d->d.n, &src->d.n, sizeof(BcNum));
			break;
		}
	}
}
#endif // ENABLE_DC

static void bc_result_free(void *result)
{
	BcResult *r = (BcResult *) result;

	switch (r->t) {

		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
		{
			bc_num_free(&r->d.n);
			break;
		}

		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
		{
			free(r->d.id.name);
			break;
		}

		default:
		{
			// Do nothing.
			break;
		}
	}
}

static void bc_lex_lineComment(BcLex *l)
{
	l->t.t = BC_LEX_WHITESPACE;
	while (l->i < l->len && l->buf[l->i++] != '\n');
	--l->i;
}

static void bc_lex_whitespace(BcLex *l)
{
	char c;
	l->t.t = BC_LEX_WHITESPACE;
	for (c = l->buf[l->i]; c != '\n' && isspace(c); c = l->buf[++l->i]);
}

static BcStatus bc_lex_number(BcLex *l, char start)
{
	const char *buf = l->buf + l->i;
	size_t len, hits = 0, bslashes = 0, i = 0, j;
	char c = buf[i];
	bool last_pt, pt = start == '.';

	last_pt = pt;
	l->t.t = BC_LEX_NUMBER;

	while (c != 0 && (isdigit(c) || (c >= 'A' && c <= 'F') ||
	                  (c == '.' && !pt) || (c == '\\' && buf[i + 1] == '\n')))
	{
		if (c != '\\') {
			last_pt = c == '.';
			pt = pt || last_pt;
		}
		else {
			++i;
			bslashes += 1;
		}

		c = buf[++i];
	}

	len = i + !last_pt - bslashes * 2;
	// This check makes sense only if size_t is (much) larger than BC_MAX_NUM.
	if (SIZE_MAX > (BC_MAX_NUM | 0xff)) {
		if (len > BC_MAX_NUM)
			return bc_error("number too long: must be [1,"BC_MAX_NUM_STR"]");
	}

	bc_vec_pop_all(&l->t.v);
	bc_vec_expand(&l->t.v, len + 1);
	bc_vec_push(&l->t.v, &start);

	for (buf -= 1, j = 1; j < len + hits * 2; ++j) {

		c = buf[j];

		// If we have hit a backslash, skip it. We don't have
		// to check for a newline because it's guaranteed.
		if (hits < bslashes && c == '\\') {
			++hits;
			++j;
			continue;
		}

		bc_vec_push(&l->t.v, &c);
	}

	bc_vec_pushZeroByte(&l->t.v);
	l->i += i;

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_lex_name(BcLex *l)
{
	size_t i = 0;
	const char *buf = l->buf + l->i - 1;
	char c = buf[i];

	l->t.t = BC_LEX_NAME;

	while ((c >= 'a' && c <= 'z') || isdigit(c) || c == '_') c = buf[++i];

	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (i > BC_MAX_STRING)
			return bc_error("name too long: must be [1,"BC_MAX_STRING_STR"]");
	}
	bc_vec_string(&l->t.v, i, buf);

	// Increment the index. We minus 1 because it has already been incremented.
	l->i += i - 1;

	return BC_STATUS_SUCCESS;
}

static void bc_lex_init(BcLex *l, BcLexNext next)
{
	l->next = next;
	bc_char_vec_init(&l->t.v);
}

static void bc_lex_free(BcLex *l)
{
	bc_vec_free(&l->t.v);
}

static void bc_lex_file(BcLex *l)
{
	G.err_line = l->line = 1;
	l->newline = false;
}

static BcStatus bc_lex_next(BcLex *l)
{
	BcStatus s;

	l->t.last = l->t.t;
	if (l->t.last == BC_LEX_EOF) return bc_error("end of file");

	l->line += l->newline;
	G.err_line = l->line;
	l->t.t = BC_LEX_EOF;

	l->newline = (l->i == l->len);
	if (l->newline) return BC_STATUS_SUCCESS;

	// Loop until failure or we don't have whitespace. This
	// is so the parser doesn't get inundated with whitespace.
	do {
		s = l->next(l);
	} while (!s && l->t.t == BC_LEX_WHITESPACE);

	return s;
}

static BcStatus bc_lex_text(BcLex *l, const char *text)
{
	l->buf = text;
	l->i = 0;
	l->len = strlen(text);
	l->t.t = l->t.last = BC_LEX_INVALID;
	return bc_lex_next(l);
}

#if ENABLE_BC
static BcStatus bc_lex_identifier(BcLex *l)
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
		l->t.t = BC_LEX_KEY_1st_keyword + i;
		if (!bc_lex_kws_POSIX(i)) {
			s = bc_posix_error_fmt("%sthe '%.8s' keyword", "POSIX does not allow ", bc_lex_kws[i].name8);
			if (s) return s;
		}

		// We minus 1 because the index has already been incremented.
		l->i += j - 1;
		return BC_STATUS_SUCCESS;
	}

	s = bc_lex_name(l);
	if (s) return s;

	if (l->t.v.len > 2) {
		// Prevent this:
		// >>> qwe=1
		// bc: POSIX only allows one character names; the following is bad: 'qwe=1
		// '
		unsigned len = strchrnul(buf, '\n') - buf;
		s = bc_posix_error_fmt("POSIX only allows one character names; the following is bad: '%.*s'", len, buf);
	}

	return s;
}

static BcStatus bc_lex_string(BcLex *l)
{
	size_t len, nls = 0, i = l->i;
	char c;

	l->t.t = BC_LEX_STR;

	for (c = l->buf[i]; c != 0 && c != '"'; c = l->buf[++i]) nls += (c == '\n');

	if (c == '\0') {
		l->i = i;
		return bc_error("string end could not be found");
	}

	len = i - l->i;
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (len > BC_MAX_STRING)
			return bc_error("string too long: must be [1,"BC_MAX_STRING_STR"]");
	}
	bc_vec_string(&l->t.v, len, l->buf + l->i);

	l->i = i + 1;
	l->line += nls;
	G.err_line = l->line;

	return BC_STATUS_SUCCESS;
}

static void bc_lex_assign(BcLex *l, BcLexType with, BcLexType without)
{
	if (l->buf[l->i] == '=') {
		++l->i;
		l->t.t = with;
	}
	else
		l->t.t = without;
}

static BcStatus bc_lex_comment(BcLex *l)
{
	size_t i, nls = 0;
	const char *buf = l->buf;

	l->t.t = BC_LEX_WHITESPACE;
	i = ++l->i;
	for (;;) {
		char c = buf[i];
 check_star:
		if (c == '*') {
			c = buf[++i];
			if (c == '/')
				break;
			goto check_star;
		}
		if (c == '\0') {
			l->i = i;
			return bc_error("comment end could not be found");
		}
		nls += (c == '\n');
		i++;
	}

	l->i = i + 1;
	l->line += nls;
	G.err_line = l->line;

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_lex_token(BcLex *l)
{
	BcStatus s = BC_STATUS_SUCCESS;
	char c = l->buf[l->i++], c2;

	// This is the workhorse of the lexer.
	switch (c) {

		case '\0':
		case '\n':
		{
			l->newline = true;
			l->t.t = !c ? BC_LEX_EOF : BC_LEX_NLINE;
			break;
		}

		case '\t':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
		{
			bc_lex_whitespace(l);
			break;
		}

		case '!':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);

			if (l->t.t == BC_LEX_OP_BOOL_NOT) {
				s = bc_POSIX_does_not_allow_bool_ops_this_is_bad("!");
				if (s) return s;
			}

			break;
		}

		case '"':
		{
			s = bc_lex_string(l);
			break;
		}

		case '#':
		{
			s = bc_POSIX_does_not_allow("'#' script comments");
			if (s) return s;

			bc_lex_lineComment(l);

			break;
		}

		case '%':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MODULUS, BC_LEX_OP_MODULUS);
			break;
		}

		case '&':
		{
			c2 = l->buf[l->i];
			if (c2 == '&') {

				s = bc_POSIX_does_not_allow_bool_ops_this_is_bad("&&");
				if (s) return s;

				++l->i;
				l->t.t = BC_LEX_OP_BOOL_AND;
			}
			else {
				l->t.t = BC_LEX_INVALID;
				s = bc_error_bad_character('&');
			}

			break;
		}

		case '(':
		case ')':
		{
			l->t.t = (BcLexType)(c - '(' + BC_LEX_LPAREN);
			break;
		}

		case '*':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MULTIPLY, BC_LEX_OP_MULTIPLY);
			break;
		}

		case '+':
		{
			c2 = l->buf[l->i];
			if (c2 == '+') {
				++l->i;
				l->t.t = BC_LEX_OP_INC;
			}
			else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_PLUS, BC_LEX_OP_PLUS);
			break;
		}

		case ',':
		{
			l->t.t = BC_LEX_COMMA;
			break;
		}

		case '-':
		{
			c2 = l->buf[l->i];
			if (c2 == '-') {
				++l->i;
				l->t.t = BC_LEX_OP_DEC;
			}
			else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_MINUS, BC_LEX_OP_MINUS);
			break;
		}

		case '.':
		{
			if (isdigit(l->buf[l->i]))
				s = bc_lex_number(l, c);
			else {
				l->t.t = BC_LEX_KEY_LAST;
				s = bc_POSIX_does_not_allow("a period ('.') as a shortcut for the last result");
			}
			break;
		}

		case '/':
		{
			c2 = l->buf[l->i];
			if (c2 == '*')
				s = bc_lex_comment(l);
			else
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_DIVIDE, BC_LEX_OP_DIVIDE);
			break;
		}

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
		{
			s = bc_lex_number(l, c);
			break;
		}

		case ';':
		{
			l->t.t = BC_LEX_SCOLON;
			break;
		}

		case '<':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_LE, BC_LEX_OP_REL_LT);
			break;
		}

		case '=':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_EQ, BC_LEX_OP_ASSIGN);
			break;
		}

		case '>':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_GE, BC_LEX_OP_REL_GT);
			break;
		}

		case '[':
		case ']':
		{
			l->t.t = (BcLexType)(c - '[' + BC_LEX_LBRACKET);
			break;
		}

		case '\\':
		{
			if (l->buf[l->i] == '\n') {
				l->t.t = BC_LEX_WHITESPACE;
				++l->i;
			}
			else
				s = bc_error_bad_character(c);
			break;
		}

		case '^':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_POWER, BC_LEX_OP_POWER);
			break;
		}

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
		{
			s = bc_lex_identifier(l);
			break;
		}

		case '{':
		case '}':
		{
			l->t.t = (BcLexType)(c - '{' + BC_LEX_LBRACE);
			break;
		}

		case '|':
		{
			c2 = l->buf[l->i];

			if (c2 == '|') {
				s = bc_POSIX_does_not_allow_bool_ops_this_is_bad("||");
				if (s) return s;

				++l->i;
				l->t.t = BC_LEX_OP_BOOL_OR;
			}
			else {
				l->t.t = BC_LEX_INVALID;
				s = bc_error_bad_character(c);
			}

			break;
		}

		default:
		{
			l->t.t = BC_LEX_INVALID;
			s = bc_error_bad_character(c);
			break;
		}
	}

	return s;
}
#endif // ENABLE_BC

#if ENABLE_DC
static BcStatus dc_lex_register(BcLex *l)
{
	BcStatus s = BC_STATUS_SUCCESS;

	if (isspace(l->buf[l->i - 1])) {
		bc_lex_whitespace(l);
		++l->i;
		if (!G_exreg)
			s = bc_error("extended register");
		else
			s = bc_lex_name(l);
	}
	else {
		bc_vec_pop_all(&l->t.v);
		bc_vec_push(&l->t.v, &l->buf[l->i - 1]);
		bc_vec_pushZeroByte(&l->t.v);
		l->t.t = BC_LEX_NAME;
	}

	return s;
}

static BcStatus dc_lex_string(BcLex *l)
{
	size_t depth = 1, nls = 0, i = l->i;
	char c;

	l->t.t = BC_LEX_STR;
	bc_vec_pop_all(&l->t.v);

	for (c = l->buf[i]; c != 0 && depth; c = l->buf[++i]) {

		depth += (c == '[' && (i == l->i || l->buf[i - 1] != '\\'));
		depth -= (c == ']' && (i == l->i || l->buf[i - 1] != '\\'));
		nls += (c == '\n');

		if (depth) bc_vec_push(&l->t.v, &c);
	}

	if (c == '\0') {
		l->i = i;
		return bc_error("string end could not be found");
	}

	bc_vec_pushZeroByte(&l->t.v);
	// This check makes sense only if size_t is (much) larger than BC_MAX_STRING.
	if (SIZE_MAX > (BC_MAX_STRING | 0xff)) {
		if (i - l->i > BC_MAX_STRING)
			return bc_error("string too long: must be [1,"BC_MAX_STRING_STR"]");
	}

	l->i = i;
	l->line += nls;
	G.err_line = l->line;

	return BC_STATUS_SUCCESS;
}

static BcStatus dc_lex_token(BcLex *l)
{
	BcStatus s = BC_STATUS_SUCCESS;
	char c = l->buf[l->i++], c2;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dc_lex_regs); ++i) {
		if (l->t.last == dc_lex_regs[i])
			return dc_lex_register(l);
	}

	if (c >= '%' && c <= '~' &&
	    (l->t.t = dc_lex_tokens[(c - '%')]) != BC_LEX_INVALID)
	{
		return s;
	}

	// This is the workhorse of the lexer.
	switch (c) {

		case '\0':
		{
			l->t.t = BC_LEX_EOF;
			break;
		}

		case '\n':
		case '\t':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
		{
			l->newline = (c == '\n');
			bc_lex_whitespace(l);
			break;
		}

		case '!':
		{
			c2 = l->buf[l->i];

			if (c2 == '=')
				l->t.t = BC_LEX_OP_REL_NE;
			else if (c2 == '<')
				l->t.t = BC_LEX_OP_REL_LE;
			else if (c2 == '>')
				l->t.t = BC_LEX_OP_REL_GE;
			else
				return bc_error_bad_character(c);

			++l->i;
			break;
		}

		case '#':
		{
			bc_lex_lineComment(l);
			break;
		}

		case '.':
		{
			if (isdigit(l->buf[l->i]))
				s = bc_lex_number(l, c);
			else
				s = bc_error_bad_character(c);
			break;
		}

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
		{
			s = bc_lex_number(l, c);
			break;
		}

		case '[':
		{
			s = dc_lex_string(l);
			break;
		}

		default:
		{
			l->t.t = BC_LEX_INVALID;
			s = bc_error_bad_character(c);
			break;
		}
	}

	return s;
}
#endif // ENABLE_DC

static void bc_program_addFunc(char *name, size_t *idx);

static void bc_parse_addFunc(BcParse *p, char *name, size_t *idx)
{
	bc_program_addFunc(name, idx);
	p->func = bc_program_func(p->fidx);
}

#define bc_parse_push(p, i) bc_vec_pushByte(&(p)->func->code, (char) (i))

static void bc_parse_pushName(BcParse *p, char *name)
{
	size_t i = 0, len = strlen(name);

	for (; i < len; ++i) bc_parse_push(p, name[i]);
	bc_parse_push(p, BC_PARSE_STREND);

	free(name);
}

static void bc_parse_pushIndex(BcParse *p, size_t idx)
{
	unsigned char amt, i, nums[sizeof(size_t)];

	for (amt = 0; idx; ++amt) {
		nums[amt] = (char) idx;
		idx = (idx & ((unsigned long) ~(UCHAR_MAX))) >> sizeof(char) * CHAR_BIT;
	}

	bc_parse_push(p, amt);
	for (i = 0; i < amt; ++i) bc_parse_push(p, nums[i]);
}

static void bc_parse_number(BcParse *p, BcInst *prev, size_t *nexs)
{
	char *num = xstrdup(p->l.t.v.v);
	size_t idx = G.prog.consts.len;

	bc_vec_push(&G.prog.consts, &num);

	bc_parse_push(p, BC_INST_NUM);
	bc_parse_pushIndex(p, idx);

	++(*nexs);
	(*prev) = BC_INST_NUM;
}

static BcStatus bc_parse_text(BcParse *p, const char *text)
{
	BcStatus s;

	p->func = bc_program_func(p->fidx);

	if (!text[0] && !BC_PARSE_CAN_EXEC(p)) {
		p->l.t.t = BC_LEX_INVALID;
		s = p->parse(p);
		if (s) return s;
		if (!BC_PARSE_CAN_EXEC(p))
			return bc_error("file is not executable");
	}

	return bc_lex_text(&p->l, text);
}

// Called when parsing or execution detects a failure,
// resets execution structures.
static void bc_program_reset(void)
{
	BcFunc *f;
	BcInstPtr *ip;

	bc_vec_npop(&G.prog.stack, G.prog.stack.len - 1);
	bc_vec_pop_all(&G.prog.results);

	f = bc_program_func(0);
	ip = bc_vec_top(&G.prog.stack);
	ip->idx = f->code.len;
}

#define bc_parse_updateFunc(p, f) \
	((p)->func = bc_program_func((p)->fidx = (f)))

// Called when bc/dc_parse_parse() detects a failure,
// resets parsing structures.
static void bc_parse_reset(BcParse *p)
{
	if (p->fidx != BC_PROG_MAIN) {
		p->func->nparams = 0;
		bc_vec_pop_all(&p->func->code);
		bc_vec_pop_all(&p->func->autos);
		bc_vec_pop_all(&p->func->labels);

		bc_parse_updateFunc(p, BC_PROG_MAIN);
	}

	p->l.i = p->l.len;
	p->l.t.t = BC_LEX_EOF;
	p->auto_part = (p->nbraces = 0);

	bc_vec_npop(&p->flags, p->flags.len - 1);
	bc_vec_pop_all(&p->exits);
	bc_vec_pop_all(&p->conds);
	bc_vec_pop_all(&p->ops);

	bc_program_reset();
}

static void bc_parse_free(BcParse *p)
{
	bc_vec_free(&p->flags);
	bc_vec_free(&p->exits);
	bc_vec_free(&p->conds);
	bc_vec_free(&p->ops);
	bc_lex_free(&p->l);
}

static void bc_parse_create(BcParse *p, size_t func,
                            BcParseParse parse, BcLexNext next)
{
	memset(p, 0, sizeof(BcParse));

	bc_lex_init(&p->l, next);
	bc_vec_init(&p->flags, sizeof(uint8_t), NULL);
	bc_vec_init(&p->exits, sizeof(BcInstPtr), NULL);
	bc_vec_init(&p->conds, sizeof(size_t), NULL);
	bc_vec_pushZeroByte(&p->flags);
	bc_vec_init(&p->ops, sizeof(BcLexType), NULL);

	p->parse = parse;
	// p->auto_part = p->nbraces = 0; - already is
	bc_parse_updateFunc(p, func);
}

#if ENABLE_BC

#define BC_PARSE_TOP_OP(p) (*((BcLexType *) bc_vec_top(&(p)->ops)))
#define BC_PARSE_LEAF(p, rparen)                                \
	(((p) >= BC_INST_NUM && (p) <= BC_INST_SQRT) || (rparen) || \
	 (p) == BC_INST_INC_POST || (p) == BC_INST_DEC_POST)

// We can calculate the conversion between tokens and exprs by subtracting the
// position of the first operator in the lex enum and adding the position of the
// first in the expr enum. Note: This only works for binary operators.
#define BC_PARSE_TOKEN_INST(t) ((char) ((t) -BC_LEX_NEG + BC_INST_NEG))

static BcStatus bc_parse_else(BcParse *p);
static BcStatus bc_parse_stmt(BcParse *p);
static BcStatus bc_parse_expr(BcParse *p, uint8_t flags, BcParseNext next);
static BcStatus bc_parse_expr_empty_ok(BcParse *p, uint8_t flags, BcParseNext next);

static BcStatus bc_parse_operator(BcParse *p, BcLexType type, size_t start,
                                  size_t *nexprs, bool next)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcLexType t;
	char l, r = bc_parse_op_PREC(type - BC_LEX_OP_INC);
	bool left = bc_parse_op_LEFT(type - BC_LEX_OP_INC);

	while (p->ops.len > start) {

		t = BC_PARSE_TOP_OP(p);
		if (t == BC_LEX_LPAREN) break;

		l = bc_parse_op_PREC(t - BC_LEX_OP_INC);
		if (l >= r && (l != r || !left)) break;

		bc_parse_push(p, BC_PARSE_TOKEN_INST(t));
		bc_vec_pop(&p->ops);
		*nexprs -= t != BC_LEX_OP_BOOL_NOT && t != BC_LEX_NEG;
	}

	bc_vec_push(&p->ops, &type);
	if (next) s = bc_lex_next(&p->l);

	return s;
}

static BcStatus bc_parse_rightParen(BcParse *p, size_t ops_bgn, size_t *nexs)
{
	BcLexType top;

	if (p->ops.len <= ops_bgn)
		return bc_error_bad_expression();
	top = BC_PARSE_TOP_OP(p);

	while (top != BC_LEX_LPAREN) {

		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

		bc_vec_pop(&p->ops);
		*nexs -= top != BC_LEX_OP_BOOL_NOT && top != BC_LEX_NEG;

		if (p->ops.len <= ops_bgn)
			return bc_error_bad_expression();
		top = BC_PARSE_TOP_OP(p);
	}

	bc_vec_pop(&p->ops);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_params(BcParse *p, uint8_t flags)
{
	BcStatus s;
	bool comma = false;
	size_t nparams;

	s = bc_lex_next(&p->l);
	if (s) return s;

	for (nparams = 0; p->l.t.t != BC_LEX_RPAREN; ++nparams) {

		flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;
		s = bc_parse_expr(p, flags, bc_parse_next_param);
		if (s) return s;

		comma = p->l.t.t == BC_LEX_COMMA;
		if (comma) {
			s = bc_lex_next(&p->l);
			if (s) return s;
		}
	}

	if (comma) return bc_error_bad_token();
	bc_parse_push(p, BC_INST_CALL);
	bc_parse_pushIndex(p, nparams);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_call(BcParse *p, char *name, uint8_t flags)
{
	BcStatus s;
	BcId entry, *entry_ptr;
	size_t idx;

	entry.name = name;

	s = bc_parse_params(p, flags);
	if (s) goto err;

	if (p->l.t.t != BC_LEX_RPAREN) {
		s = bc_error_bad_token();
		goto err;
	}

	idx = bc_map_index(&G.prog.fn_map, &entry);

	if (idx == BC_VEC_INVALID_IDX) {
		name = xstrdup(entry.name);
		bc_parse_addFunc(p, name, &idx);
		idx = bc_map_index(&G.prog.fn_map, &entry);
		free(entry.name);
	}
	else
		free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, idx);
	bc_parse_pushIndex(p, entry_ptr->idx);

	return bc_lex_next(&p->l);

err:
	free(name);
	return s;
}

static BcStatus bc_parse_name(BcParse *p, BcInst *type, uint8_t flags)
{
	BcStatus s;
	char *name;

	name = xstrdup(p->l.t.v.v);
	s = bc_lex_next(&p->l);
	if (s) goto err;

	if (p->l.t.t == BC_LEX_LBRACKET) {

		s = bc_lex_next(&p->l);
		if (s) goto err;

		if (p->l.t.t == BC_LEX_RBRACKET) {

			if (!(flags & BC_PARSE_ARRAY)) {
				s = bc_error_bad_expression();
				goto err;
			}

			*type = BC_INST_ARRAY;
		}
		else {

			*type = BC_INST_ARRAY_ELEM;

			flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
			s = bc_parse_expr(p, flags, bc_parse_next_elem);
			if (s) goto err;
		}

		s = bc_lex_next(&p->l);
		if (s) goto err;
		bc_parse_push(p, *type);
		bc_parse_pushName(p, name);
	}
	else if (p->l.t.t == BC_LEX_LPAREN) {

		if (flags & BC_PARSE_NOCALL) {
			s = bc_error_bad_token();
			goto err;
		}

		*type = BC_INST_CALL;
		s = bc_parse_call(p, name, flags);
	}
	else {
		*type = BC_INST_VAR;
		bc_parse_push(p, BC_INST_VAR);
		bc_parse_pushName(p, name);
	}

	return s;

err:
	free(name);
	return s;
}

static BcStatus bc_parse_read(BcParse *p)
{
	BcStatus s;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN) return bc_error_bad_token();

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();

	bc_parse_push(p, BC_INST_READ);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_builtin(BcParse *p, BcLexType type, uint8_t flags,
                                 BcInst *prev)
{
	BcStatus s;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN) return bc_error_bad_token();

	flags = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) | BC_PARSE_ARRAY;

	s = bc_lex_next(&p->l);
	if (s) return s;

	s = bc_parse_expr(p, flags, bc_parse_next_rel);
	if (s) return s;

	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();

	*prev = (type == BC_LEX_KEY_LENGTH) ? BC_INST_LENGTH : BC_INST_SQRT;
	bc_parse_push(p, *prev);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_scale(BcParse *p, BcInst *type, uint8_t flags)
{
	BcStatus s;

	s = bc_lex_next(&p->l);
	if (s) return s;

	if (p->l.t.t != BC_LEX_LPAREN) {
		*type = BC_INST_SCALE;
		bc_parse_push(p, BC_INST_SCALE);
		return BC_STATUS_SUCCESS;
	}

	*type = BC_INST_SCALE_FUNC;
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);

	s = bc_lex_next(&p->l);
	if (s) return s;

	s = bc_parse_expr(p, flags, bc_parse_next_rel);
	if (s) return s;
	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();
	bc_parse_push(p, BC_INST_SCALE_FUNC);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_incdec(BcParse *p, BcInst *prev, bool *paren_expr,
                                size_t *nexprs, uint8_t flags)
{
	BcStatus s;
	BcLexType type;
	char inst;
	BcInst etype = *prev;

	if (etype == BC_INST_VAR || etype == BC_INST_ARRAY_ELEM ||
	    etype == BC_INST_SCALE || etype == BC_INST_LAST ||
	    etype == BC_INST_IBASE || etype == BC_INST_OBASE)
	{
		*prev = inst = BC_INST_INC_POST + (p->l.t.t != BC_LEX_OP_INC);
		bc_parse_push(p, inst);
		s = bc_lex_next(&p->l);
	}
	else {

		*prev = inst = BC_INST_INC_PRE + (p->l.t.t != BC_LEX_OP_INC);
		*paren_expr = true;

		s = bc_lex_next(&p->l);
		if (s) return s;
		type = p->l.t.t;

		// Because we parse the next part of the expression
		// right here, we need to increment this.
		*nexprs = *nexprs + 1;

		switch (type) {

			case BC_LEX_NAME:
			{
				s = bc_parse_name(p, prev, flags | BC_PARSE_NOCALL);
				break;
			}

			case BC_LEX_KEY_IBASE:
			case BC_LEX_KEY_LAST:
			case BC_LEX_KEY_OBASE:
			{
				bc_parse_push(p, type - BC_LEX_KEY_IBASE + BC_INST_IBASE);
				s = bc_lex_next(&p->l);
				break;
			}

			case BC_LEX_KEY_SCALE:
			{
				s = bc_lex_next(&p->l);
				if (s) return s;
				if (p->l.t.t == BC_LEX_LPAREN)
					s = bc_error_bad_token();
				else
					bc_parse_push(p, BC_INST_SCALE);
				break;
			}

			default:
			{
				s = bc_error_bad_token();
				break;
			}
		}

		if (!s) bc_parse_push(p, inst);
	}

	return s;
}

static BcStatus bc_parse_minus(BcParse *p, BcInst *prev, size_t ops_bgn,
                               bool rparen, size_t *nexprs)
{
	BcStatus s;
	BcLexType type;
	BcInst etype = *prev;

	s = bc_lex_next(&p->l);
	if (s) return s;

	type = rparen || etype == BC_INST_INC_POST || etype == BC_INST_DEC_POST ||
	               (etype >= BC_INST_NUM && etype <= BC_INST_SQRT) ?
	           BC_LEX_OP_MINUS :
	           BC_LEX_NEG;
	*prev = BC_PARSE_TOKEN_INST(type);

	// We can just push onto the op stack because this is the largest
	// precedence operator that gets pushed. Inc/dec does not.
	if (type != BC_LEX_OP_MINUS)
		bc_vec_push(&p->ops, &type);
	else
		s = bc_parse_operator(p, type, ops_bgn, nexprs, false);

	return s;
}

static BcStatus bc_parse_string(BcParse *p, char inst)
{
	char *str = xstrdup(p->l.t.v.v);

	bc_parse_push(p, BC_INST_STR);
	bc_parse_pushIndex(p, G.prog.strs.len);
	bc_vec_push(&G.prog.strs, &str);
	bc_parse_push(p, inst);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_print(BcParse *p)
{
	BcStatus s;
	BcLexType type;
	bool comma;

	s = bc_lex_next(&p->l);
	if (s) return s;

	type = p->l.t.t;

	if (type == BC_LEX_SCOLON || type == BC_LEX_NLINE)
		return bc_error("bad print statement");

	comma = false;
	while (type != BC_LEX_SCOLON && type != BC_LEX_NLINE) {

		if (type == BC_LEX_STR) {
			s = bc_parse_string(p, BC_INST_PRINT_POP);
			if (s) return s;
		} else {
			s = bc_parse_expr(p, 0, bc_parse_next_print);
			if (s) return s;
			bc_parse_push(p, BC_INST_PRINT_POP);
		}

		comma = p->l.t.t == BC_LEX_COMMA;
		if (comma) {
			s = bc_lex_next(&p->l);
			if (s) return s;
		}
		type = p->l.t.t;
	}

	if (comma) return bc_error_bad_token();

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_return(BcParse *p)
{
	BcStatus s;
	BcLexType t;
	bool paren;

	if (!BC_PARSE_FUNC(p)) return bc_error_bad_token();

	s = bc_lex_next(&p->l);
	if (s) return s;

	t = p->l.t.t;
	paren = t == BC_LEX_LPAREN;

	if (t == BC_LEX_NLINE || t == BC_LEX_SCOLON)
		bc_parse_push(p, BC_INST_RET0);
	else {

		s = bc_parse_expr_empty_ok(p, 0, bc_parse_next_expr);
		if (s == BC_STATUS_PARSE_EMPTY_EXP) {
			bc_parse_push(p, BC_INST_RET0);
			s = bc_lex_next(&p->l);
		}
		if (s) return s;

		if (!paren || p->l.t.last != BC_LEX_RPAREN) {
			s = bc_POSIX_requires("parentheses around return expressions");
			if (s) return s;
		}

		bc_parse_push(p, BC_INST_RET);
	}

	return s;
}

static BcStatus bc_parse_endBody(BcParse *p, bool brace)
{
	BcStatus s = BC_STATUS_SUCCESS;

	if (p->flags.len <= 1 || (brace && p->nbraces == 0))
		return bc_error_bad_token();

	if (brace) {

		if (p->l.t.t == BC_LEX_RBRACE) {
			if (!p->nbraces) return bc_error_bad_token();
			--p->nbraces;
			s = bc_lex_next(&p->l);
			if (s) return s;
		}
		else
			return bc_error_bad_token();
	}

	if (BC_PARSE_IF(p)) {

		uint8_t *flag_ptr;

		while (p->l.t.t == BC_LEX_NLINE) {
			s = bc_lex_next(&p->l);
			if (s) return s;
		}

		bc_vec_pop(&p->flags);

		flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);
		*flag_ptr = (*flag_ptr | BC_PARSE_FLAG_IF_END);

		if (p->l.t.t == BC_LEX_KEY_ELSE) s = bc_parse_else(p);
	}
	else if (BC_PARSE_ELSE(p)) {

		BcInstPtr *ip;
		size_t *label;

		bc_vec_pop(&p->flags);

		ip = bc_vec_top(&p->exits);
		label = bc_vec_item(&p->func->labels, ip->idx);
		*label = p->func->code.len;

		bc_vec_pop(&p->exits);
	}
	else if (BC_PARSE_FUNC_INNER(p)) {
		bc_parse_push(p, BC_INST_RET0);
		bc_parse_updateFunc(p, BC_PROG_MAIN);
		bc_vec_pop(&p->flags);
	}
	else {

		BcInstPtr *ip = bc_vec_top(&p->exits);
		size_t *label = bc_vec_top(&p->conds);

		bc_parse_push(p, BC_INST_JUMP);
		bc_parse_pushIndex(p, *label);

		label = bc_vec_item(&p->func->labels, ip->idx);
		*label = p->func->code.len;

		bc_vec_pop(&p->flags);
		bc_vec_pop(&p->exits);
		bc_vec_pop(&p->conds);
	}

	return s;
}

static void bc_parse_startBody(BcParse *p, uint8_t flags)
{
	uint8_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);
	flags |= (*flag_ptr & (BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_LOOP));
	flags |= BC_PARSE_FLAG_BODY;
	bc_vec_push(&p->flags, &flags);
}

static void bc_parse_noElse(BcParse *p)
{
	BcInstPtr *ip;
	size_t *label;
	uint8_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);

	*flag_ptr = (*flag_ptr & ~(BC_PARSE_FLAG_IF_END));

	ip = bc_vec_top(&p->exits);
	label = bc_vec_item(&p->func->labels, ip->idx);
	*label = p->func->code.len;

	bc_vec_pop(&p->exits);
}

static BcStatus bc_parse_if(BcParse *p)
{
	BcStatus s;
	BcInstPtr ip;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN) return bc_error_bad_token();

	s = bc_lex_next(&p->l);
	if (s) return s;
	s = bc_parse_expr(p, BC_PARSE_REL, bc_parse_next_rel);
	if (s) return s;
	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();

	s = bc_lex_next(&p->l);
	if (s) return s;
	bc_parse_push(p, BC_INST_JUMP_ZERO);

	ip.idx = p->func->labels.len;
	ip.func = ip.len = 0;

	bc_parse_pushIndex(p, ip.idx);
	bc_vec_push(&p->exits, &ip);
	bc_vec_push(&p->func->labels, &ip.idx);
	bc_parse_startBody(p, BC_PARSE_FLAG_IF);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_else(BcParse *p)
{
	BcInstPtr ip;

	if (!BC_PARSE_IF_END(p)) return bc_error_bad_token();

	ip.idx = p->func->labels.len;
	ip.func = ip.len = 0;

	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, ip.idx);

	bc_parse_noElse(p);

	bc_vec_push(&p->exits, &ip);
	bc_vec_push(&p->func->labels, &ip.idx);
	bc_parse_startBody(p, BC_PARSE_FLAG_ELSE);

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_while(BcParse *p)
{
	BcStatus s;
	BcInstPtr ip;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN) return bc_error_bad_token();
	s = bc_lex_next(&p->l);
	if (s) return s;

	ip.idx = p->func->labels.len;

	bc_vec_push(&p->func->labels, &p->func->code.len);
	bc_vec_push(&p->conds, &ip.idx);

	ip.idx = p->func->labels.len;
	ip.func = 1;
	ip.len = 0;

	bc_vec_push(&p->exits, &ip);
	bc_vec_push(&p->func->labels, &ip.idx);

	s = bc_parse_expr(p, BC_PARSE_REL, bc_parse_next_rel);
	if (s) return s;
	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();
	s = bc_lex_next(&p->l);
	if (s) return s;

	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, ip.idx);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_for(BcParse *p)
{
	BcStatus s;
	BcInstPtr ip;
	size_t cond_idx, exit_idx, body_idx, update_idx;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN) return bc_error_bad_token();
	s = bc_lex_next(&p->l);
	if (s) return s;

	if (p->l.t.t != BC_LEX_SCOLON)
		s = bc_parse_expr(p, 0, bc_parse_next_for);
	else
		s = bc_POSIX_does_not_allow_empty_X_expression_in_for("init");

	if (s) return s;
	if (p->l.t.t != BC_LEX_SCOLON) return bc_error_bad_token();
	s = bc_lex_next(&p->l);
	if (s) return s;

	cond_idx = p->func->labels.len;
	update_idx = cond_idx + 1;
	body_idx = update_idx + 1;
	exit_idx = body_idx + 1;

	bc_vec_push(&p->func->labels, &p->func->code.len);

	if (p->l.t.t != BC_LEX_SCOLON)
		s = bc_parse_expr(p, BC_PARSE_REL, bc_parse_next_for);
	else
		s = bc_POSIX_does_not_allow_empty_X_expression_in_for("condition");

	if (s) return s;
	if (p->l.t.t != BC_LEX_SCOLON) return bc_error_bad_token();

	s = bc_lex_next(&p->l);
	if (s) return s;

	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, exit_idx);
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, body_idx);

	ip.idx = p->func->labels.len;

	bc_vec_push(&p->conds, &update_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	if (p->l.t.t != BC_LEX_RPAREN)
		s = bc_parse_expr(p, 0, bc_parse_next_rel);
	else
		s = bc_POSIX_does_not_allow_empty_X_expression_in_for("update");

	if (s) return s;

	if (p->l.t.t != BC_LEX_RPAREN) return bc_error_bad_token();
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, cond_idx);
	bc_vec_push(&p->func->labels, &p->func->code.len);

	ip.idx = exit_idx;
	ip.func = 1;
	ip.len = 0;

	bc_vec_push(&p->exits, &ip);
	bc_vec_push(&p->func->labels, &ip.idx);
	bc_lex_next(&p->l);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_parse_loopExit(BcParse *p, BcLexType type)
{
	BcStatus s;
	size_t i;
	BcInstPtr *ip;

	if (!BC_PARSE_LOOP(p)) return bc_error_bad_token();

	if (type == BC_LEX_KEY_BREAK) {

		if (p->exits.len == 0) return bc_error_bad_token();

		i = p->exits.len - 1;
		ip = bc_vec_item(&p->exits, i);

		while (!ip->func && i < p->exits.len) ip = bc_vec_item(&p->exits, i--);
		if (i >= p->exits.len && !ip->func) return bc_error_bad_token();

		i = ip->idx;
	}
	else
		i = *((size_t *) bc_vec_top(&p->conds));

	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, i);

	s = bc_lex_next(&p->l);
	if (s) return s;

	if (p->l.t.t != BC_LEX_SCOLON && p->l.t.t != BC_LEX_NLINE)
		return bc_error_bad_token();

	return bc_lex_next(&p->l);
}

static BcStatus bc_parse_func(BcParse *p)
{
	BcStatus s;
	bool var, comma = false;
	uint8_t flags;
	char *name;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_NAME)
		return bc_error("bad function definition");

	name = xstrdup(p->l.t.v.v);
	bc_parse_addFunc(p, name, &p->fidx);

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_LPAREN)
		return bc_error("bad function definition");
	s = bc_lex_next(&p->l);
	if (s) return s;

	while (p->l.t.t != BC_LEX_RPAREN) {

		if (p->l.t.t != BC_LEX_NAME)
			return bc_error("bad function definition");

		++p->func->nparams;

		name = xstrdup(p->l.t.v.v);
		s = bc_lex_next(&p->l);
		if (s) goto err;

		var = p->l.t.t != BC_LEX_LBRACKET;

		if (!var) {

			s = bc_lex_next(&p->l);
			if (s) goto err;

			if (p->l.t.t != BC_LEX_RBRACKET) {
				s = bc_error("bad function definition");
				goto err;
			}

			s = bc_lex_next(&p->l);
			if (s) goto err;
		}

		comma = p->l.t.t == BC_LEX_COMMA;
		if (comma) {
			s = bc_lex_next(&p->l);
			if (s) goto err;
		}

		s = bc_func_insert(p->func, name, var);
		if (s) goto err;
	}

	if (comma) return bc_error("bad function definition");

	flags = BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_BODY;
	bc_parse_startBody(p, flags);

	s = bc_lex_next(&p->l);
	if (s) return s;

	if (p->l.t.t != BC_LEX_LBRACE)
		s = bc_POSIX_requires("the left brace be on the same line as the function header");

	return s;

err:
	free(name);
	return s;
}

static BcStatus bc_parse_auto(BcParse *p)
{
	BcStatus s;
	bool comma, var, one;
	char *name;

	if (!p->auto_part) return bc_error_bad_token();
	s = bc_lex_next(&p->l);
	if (s) return s;

	p->auto_part = comma = false;
	one = p->l.t.t == BC_LEX_NAME;

	while (p->l.t.t == BC_LEX_NAME) {

		name = xstrdup(p->l.t.v.v);
		s = bc_lex_next(&p->l);
		if (s) goto err;

		var = p->l.t.t != BC_LEX_LBRACKET;
		if (!var) {

			s = bc_lex_next(&p->l);
			if (s) goto err;

			if (p->l.t.t != BC_LEX_RBRACKET) {
				s = bc_error("bad function definition");
				goto err;
			}

			s = bc_lex_next(&p->l);
			if (s) goto err;
		}

		comma = p->l.t.t == BC_LEX_COMMA;
		if (comma) {
			s = bc_lex_next(&p->l);
			if (s) goto err;
		}

		s = bc_func_insert(p->func, name, var);
		if (s) goto err;
	}

	if (comma) return bc_error("bad function definition");
	if (!one) return bc_error("no auto variable found");

	if (p->l.t.t != BC_LEX_NLINE && p->l.t.t != BC_LEX_SCOLON)
		return bc_error_bad_token();

	return bc_lex_next(&p->l);

err:
	free(name);
	return s;
}

static BcStatus bc_parse_body(BcParse *p, bool brace)
{
	BcStatus s = BC_STATUS_SUCCESS;
	uint8_t *flag_ptr = bc_vec_top(&p->flags);

	*flag_ptr &= ~(BC_PARSE_FLAG_BODY);

	if (*flag_ptr & BC_PARSE_FLAG_FUNC_INNER) {

		if (!brace) return bc_error_bad_token();
		p->auto_part = p->l.t.t != BC_LEX_KEY_AUTO;

		if (!p->auto_part) {
			s = bc_parse_auto(p);
			if (s) return s;
		}

		if (p->l.t.t == BC_LEX_NLINE) s = bc_lex_next(&p->l);
	}
	else {
		s = bc_parse_stmt(p);
		if (!s && !brace) s = bc_parse_endBody(p, false);
	}

	return s;
}

static BcStatus bc_parse_stmt(BcParse *p)
{
	BcStatus s = BC_STATUS_SUCCESS;

	switch (p->l.t.t) {

		case BC_LEX_NLINE:
		{
			return bc_lex_next(&p->l);
		}

		case BC_LEX_KEY_ELSE:
		{
			p->auto_part = false;
			break;
		}

		case BC_LEX_LBRACE:
		{
			if (!BC_PARSE_BODY(p)) return bc_error_bad_token();

			++p->nbraces;
			s = bc_lex_next(&p->l);
			if (s) return s;

			return bc_parse_body(p, true);
		}

		case BC_LEX_KEY_AUTO:
		{
			return bc_parse_auto(p);
		}

		default:
		{
			p->auto_part = false;

			if (BC_PARSE_IF_END(p)) {
				bc_parse_noElse(p);
				return BC_STATUS_SUCCESS;
			}
			else if (BC_PARSE_BODY(p))
				return bc_parse_body(p, false);

			break;
		}
	}

	switch (p->l.t.t) {

		case BC_LEX_OP_INC:
		case BC_LEX_OP_DEC:
		case BC_LEX_OP_MINUS:
		case BC_LEX_OP_BOOL_NOT:
		case BC_LEX_LPAREN:
		case BC_LEX_NAME:
		case BC_LEX_NUMBER:
		case BC_LEX_KEY_IBASE:
		case BC_LEX_KEY_LAST:
		case BC_LEX_KEY_LENGTH:
		case BC_LEX_KEY_OBASE:
		case BC_LEX_KEY_READ:
		case BC_LEX_KEY_SCALE:
		case BC_LEX_KEY_SQRT:
		{
			s = bc_parse_expr(p, BC_PARSE_PRINT, bc_parse_next_expr);
			break;
		}

		case BC_LEX_KEY_ELSE:
		{
			s = bc_parse_else(p);
			break;
		}

		case BC_LEX_SCOLON:
		{
			while (!s && p->l.t.t == BC_LEX_SCOLON) s = bc_lex_next(&p->l);
			break;
		}

		case BC_LEX_RBRACE:
		{
			s = bc_parse_endBody(p, true);
			break;
		}

		case BC_LEX_STR:
		{
			s = bc_parse_string(p, BC_INST_PRINT_STR);
			break;
		}

		case BC_LEX_KEY_BREAK:
		case BC_LEX_KEY_CONTINUE:
		{
			s = bc_parse_loopExit(p, p->l.t.t);
			break;
		}

		case BC_LEX_KEY_FOR:
		{
			s = bc_parse_for(p);
			break;
		}

		case BC_LEX_KEY_HALT:
		{
			bc_parse_push(p, BC_INST_HALT);
			s = bc_lex_next(&p->l);
			break;
		}

		case BC_LEX_KEY_IF:
		{
			s = bc_parse_if(p);
			break;
		}

		case BC_LEX_KEY_LIMITS:
		{
			// "limits" is a compile-time command,
			// the output is produced at _parse time_.
			s = bc_lex_next(&p->l);
			if (s) return s;
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
			break;
		}

		case BC_LEX_KEY_PRINT:
		{
			s = bc_parse_print(p);
			break;
		}

		case BC_LEX_KEY_QUIT:
		{
			// "quit" is a compile-time command. For example,
			// "if (0 == 1) quit" terminates when parsing the statement,
			// not when it is executed
			QUIT_OR_RETURN_TO_MAIN;
		}

		case BC_LEX_KEY_RETURN:
		{
			s = bc_parse_return(p);
			break;
		}

		case BC_LEX_KEY_WHILE:
		{
			s = bc_parse_while(p);
			break;
		}

		default:
		{
			s = bc_error_bad_token();
			break;
		}
	}

	return s;
}

static BcStatus bc_parse_parse(BcParse *p)
{
	BcStatus s;

	if (p->l.t.t == BC_LEX_EOF)
		s = p->flags.len > 0 ? bc_error("block end could not be found") : bc_error("end of file");
	else if (p->l.t.t == BC_LEX_KEY_DEFINE) {
		if (!BC_PARSE_CAN_EXEC(p)) return bc_error_bad_token();
		s = bc_parse_func(p);
	}
	else
		s = bc_parse_stmt(p);

	if (s || G_interrupt) {
		bc_parse_reset(p);
		s = BC_STATUS_FAILURE;
	}

	return s;
}

static BcStatus bc_parse_expr_empty_ok(BcParse *p, uint8_t flags, BcParseNext next)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcInst prev = BC_INST_PRINT;
	BcLexType top, t = p->l.t.t;
	size_t nexprs = 0, ops_bgn = p->ops.len;
	unsigned nparens, nrelops;
	bool paren_first, paren_expr, rprn, done, get_token, assign, bin_last;

	paren_first = p->l.t.t == BC_LEX_LPAREN;
	nparens = nrelops = 0;
	paren_expr = rprn = done = get_token = assign = false;
	bin_last = true;

	for (; !G_interrupt && !s && !done && bc_parse_exprs(t); t = p->l.t.t) {
		switch (t) {

			case BC_LEX_OP_INC:
			case BC_LEX_OP_DEC:
			{
				s = bc_parse_incdec(p, &prev, &paren_expr, &nexprs, flags);
				rprn = get_token = bin_last = false;
				break;
			}

			case BC_LEX_OP_MINUS:
			{
				s = bc_parse_minus(p, &prev, ops_bgn, rprn, &nexprs);
				rprn = get_token = false;
				bin_last = prev == BC_INST_MINUS;
				break;
			}

			case BC_LEX_OP_ASSIGN_POWER:
			case BC_LEX_OP_ASSIGN_MULTIPLY:
			case BC_LEX_OP_ASSIGN_DIVIDE:
			case BC_LEX_OP_ASSIGN_MODULUS:
			case BC_LEX_OP_ASSIGN_PLUS:
			case BC_LEX_OP_ASSIGN_MINUS:
			case BC_LEX_OP_ASSIGN:
			{
				if (prev != BC_INST_VAR && prev != BC_INST_ARRAY_ELEM &&
				    prev != BC_INST_SCALE && prev != BC_INST_IBASE &&
				    prev != BC_INST_OBASE && prev != BC_INST_LAST)
				{
					s = bc_error("bad assignment:"
						" left side must be scale,"
						" ibase, obase, last, var,"
						" or array element"
					);
					break;
				}
			}
			// Fallthrough.
			case BC_LEX_OP_POWER:
			case BC_LEX_OP_MULTIPLY:
			case BC_LEX_OP_DIVIDE:
			case BC_LEX_OP_MODULUS:
			case BC_LEX_OP_PLUS:
			case BC_LEX_OP_REL_EQ:
			case BC_LEX_OP_REL_LE:
			case BC_LEX_OP_REL_GE:
			case BC_LEX_OP_REL_NE:
			case BC_LEX_OP_REL_LT:
			case BC_LEX_OP_REL_GT:
			case BC_LEX_OP_BOOL_NOT:
			case BC_LEX_OP_BOOL_OR:
			case BC_LEX_OP_BOOL_AND:
			{
				if (((t == BC_LEX_OP_BOOL_NOT) != bin_last)
				 || (t != BC_LEX_OP_BOOL_NOT && prev == BC_INST_BOOL_NOT)
				) {
					return bc_error_bad_expression();
				}

				nrelops += t >= BC_LEX_OP_REL_EQ && t <= BC_LEX_OP_REL_GT;
				prev = BC_PARSE_TOKEN_INST(t);
				s = bc_parse_operator(p, t, ops_bgn, &nexprs, true);
				rprn = get_token = false;
				bin_last = t != BC_LEX_OP_BOOL_NOT;

				break;
			}

			case BC_LEX_LPAREN:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				++nparens;
				paren_expr = rprn = bin_last = false;
				get_token = true;
				bc_vec_push(&p->ops, &t);

				break;
			}

			case BC_LEX_RPAREN:
			{
				if (bin_last || prev == BC_INST_BOOL_NOT)
					return bc_error_bad_expression();

				if (nparens == 0) {
					s = BC_STATUS_SUCCESS;
					done = true;
					get_token = false;
					break;
				}
				else if (!paren_expr)
					return BC_STATUS_PARSE_EMPTY_EXP;

				--nparens;
				paren_expr = rprn = true;
				get_token = bin_last = false;

				s = bc_parse_rightParen(p, ops_bgn, &nexprs);

				break;
			}

			case BC_LEX_NAME:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				paren_expr = true;
				rprn = get_token = bin_last = false;
				s = bc_parse_name(p, &prev, flags & ~BC_PARSE_NOCALL);
				++nexprs;

				break;
			}

			case BC_LEX_NUMBER:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				bc_parse_number(p, &prev, &nexprs);
				paren_expr = get_token = true;
				rprn = bin_last = false;

				break;
			}

			case BC_LEX_KEY_IBASE:
			case BC_LEX_KEY_LAST:
			case BC_LEX_KEY_OBASE:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				prev = (char) (t - BC_LEX_KEY_IBASE + BC_INST_IBASE);
				bc_parse_push(p, (char) prev);

				paren_expr = get_token = true;
				rprn = bin_last = false;
				++nexprs;

				break;
			}

			case BC_LEX_KEY_LENGTH:
			case BC_LEX_KEY_SQRT:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				s = bc_parse_builtin(p, t, flags, &prev);
				paren_expr = true;
				rprn = get_token = bin_last = false;
				++nexprs;

				break;
			}

			case BC_LEX_KEY_READ:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				else if (flags & BC_PARSE_NOREAD)
					s = bc_error_nested_read_call();
				else
					s = bc_parse_read(p);

				paren_expr = true;
				rprn = get_token = bin_last = false;
				++nexprs;
				prev = BC_INST_READ;

				break;
			}

			case BC_LEX_KEY_SCALE:
			{
				if (BC_PARSE_LEAF(prev, rprn))
					return bc_error_bad_expression();
				s = bc_parse_scale(p, &prev, flags);
				paren_expr = true;
				rprn = get_token = bin_last = false;
				++nexprs;
				prev = BC_INST_SCALE;

				break;
			}

			default:
			{
				s = bc_error_bad_token();
				break;
			}
		}

		if (!s && get_token) s = bc_lex_next(&p->l);
	}

	if (s) return s;
	if (G_interrupt) return BC_STATUS_FAILURE; // ^C: stop parsing

	while (p->ops.len > ops_bgn) {

		top = BC_PARSE_TOP_OP(p);
		assign = top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN;

		if (top == BC_LEX_LPAREN || top == BC_LEX_RPAREN)
			return bc_error_bad_expression();

		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

		nexprs -= top != BC_LEX_OP_BOOL_NOT && top != BC_LEX_NEG;
		bc_vec_pop(&p->ops);
	}

	if (prev == BC_INST_BOOL_NOT || nexprs != 1)
		return bc_error_bad_expression();

	// next is BcParseNext, byte array of up to 4 BC_LEX's, packed into 32-bit word
	for (;;) {
		if (t == (next & 0x7f))
			goto ok;
		if (next & 0x80) // last element?
			break;
		next >>= 8;
	}
	return bc_error_bad_expression();
 ok:

	if (!(flags & BC_PARSE_REL) && nrelops) {
		s = bc_POSIX_does_not_allow("comparison operators outside if or loops");
		if (s) return s;
	}
	else if ((flags & BC_PARSE_REL) && nrelops > 1) {
		s = bc_POSIX_requires("exactly one comparison operator per condition");
		if (s) return s;
	}

	if (flags & BC_PARSE_PRINT) {
		if (paren_first || !assign) bc_parse_push(p, BC_INST_PRINT);
		bc_parse_push(p, BC_INST_POP);
	}

	return s;
}

static BcStatus bc_parse_expr(BcParse *p, uint8_t flags, BcParseNext next)
{
	BcStatus s;

	s = bc_parse_expr_empty_ok(p, flags, next);
	if (s == BC_STATUS_PARSE_EMPTY_EXP)
		return bc_error("empty expression");
	return s;
}

static void bc_parse_init(BcParse *p, size_t func)
{
	bc_parse_create(p, func, bc_parse_parse, bc_lex_token);
}

static BcStatus bc_parse_expression(BcParse *p, uint8_t flags)
{
	return bc_parse_expr(p, flags, bc_parse_next_read);
}

#endif // ENABLE_BC

#if ENABLE_DC

#define DC_PARSE_BUF_LEN ((int) (sizeof(uint32_t) * CHAR_BIT))

static BcStatus dc_parse_register(BcParse *p)
{
	BcStatus s;
	char *name;

	s = bc_lex_next(&p->l);
	if (s) return s;
	if (p->l.t.t != BC_LEX_NAME) return bc_error_bad_token();

	name = xstrdup(p->l.t.v.v);
	bc_parse_pushName(p, name);

	return s;
}

static BcStatus dc_parse_string(BcParse *p)
{
	char *str, *name, b[DC_PARSE_BUF_LEN + 1];
	size_t idx, len = G.prog.strs.len;

	sprintf(b, "%0*zu", DC_PARSE_BUF_LEN, len);
	name = xstrdup(b);

	str = xstrdup(p->l.t.v.v);
	bc_parse_push(p, BC_INST_STR);
	bc_parse_pushIndex(p, len);
	bc_vec_push(&G.prog.strs, &str);
	bc_parse_addFunc(p, name, &idx);

	return bc_lex_next(&p->l);
}

static BcStatus dc_parse_mem(BcParse *p, uint8_t inst, bool name, bool store)
{
	BcStatus s;

	bc_parse_push(p, inst);
	if (name) {
		s = dc_parse_register(p);
		if (s) return s;
	}

	if (store) {
		bc_parse_push(p, BC_INST_SWAP);
		bc_parse_push(p, BC_INST_ASSIGN);
		bc_parse_push(p, BC_INST_POP);
	}

	return bc_lex_next(&p->l);
}

static BcStatus dc_parse_cond(BcParse *p, uint8_t inst)
{
	BcStatus s;

	bc_parse_push(p, inst);
	bc_parse_push(p, BC_INST_EXEC_COND);

	s = dc_parse_register(p);
	if (s) return s;

	s = bc_lex_next(&p->l);
	if (s) return s;

	if (p->l.t.t == BC_LEX_ELSE) {
		s = dc_parse_register(p);
		if (s) return s;
		s = bc_lex_next(&p->l);
	}
	else
		bc_parse_push(p, BC_PARSE_STREND);

	return s;
}

static BcStatus dc_parse_token(BcParse *p, BcLexType t, uint8_t flags)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcInst prev;
	uint8_t inst;
	bool assign, get_token = false;

	switch (t) {

		case BC_LEX_OP_REL_EQ:
		case BC_LEX_OP_REL_LE:
		case BC_LEX_OP_REL_GE:
		case BC_LEX_OP_REL_NE:
		case BC_LEX_OP_REL_LT:
		case BC_LEX_OP_REL_GT:
		{
			s = dc_parse_cond(p, t - BC_LEX_OP_REL_EQ + BC_INST_REL_EQ);
			break;
		}

		case BC_LEX_SCOLON:
		case BC_LEX_COLON:
		{
			s = dc_parse_mem(p, BC_INST_ARRAY_ELEM, true, t == BC_LEX_COLON);
			break;
		}

		case BC_LEX_STR:
		{
			s = dc_parse_string(p);
			break;
		}

		case BC_LEX_NEG:
		case BC_LEX_NUMBER:
		{
			if (t == BC_LEX_NEG) {
				s = bc_lex_next(&p->l);
				if (s) return s;
				if (p->l.t.t != BC_LEX_NUMBER)
					return bc_error_bad_token();
			}

			bc_parse_number(p, &prev, &p->nbraces);

			if (t == BC_LEX_NEG) bc_parse_push(p, BC_INST_NEG);
			get_token = true;

			break;
		}

		case BC_LEX_KEY_READ:
		{
			if (flags & BC_PARSE_NOREAD)
				s = bc_error_nested_read_call();
			else
				bc_parse_push(p, BC_INST_READ);
			get_token = true;
			break;
		}

		case BC_LEX_OP_ASSIGN:
		case BC_LEX_STORE_PUSH:
		{
			assign = t == BC_LEX_OP_ASSIGN;
			inst = assign ? BC_INST_VAR : BC_INST_PUSH_TO_VAR;
			s = dc_parse_mem(p, inst, true, assign);
			break;
		}

		case BC_LEX_LOAD:
		case BC_LEX_LOAD_POP:
		{
			inst = t == BC_LEX_LOAD_POP ? BC_INST_PUSH_VAR : BC_INST_LOAD;
			s = dc_parse_mem(p, inst, true, false);
			break;
		}

		case BC_LEX_STORE_IBASE:
		case BC_LEX_STORE_SCALE:
		case BC_LEX_STORE_OBASE:
		{
			inst = t - BC_LEX_STORE_IBASE + BC_INST_IBASE;
			s = dc_parse_mem(p, inst, false, true);
			break;
		}

		default:
		{
			s = bc_error_bad_token();
			get_token = true;
			break;
		}
	}

	if (!s && get_token) s = bc_lex_next(&p->l);

	return s;
}

static BcStatus dc_parse_expr(BcParse *p, uint8_t flags)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcInst inst;
	BcLexType t;

	if (flags & BC_PARSE_NOCALL) p->nbraces = G.prog.results.len;

	for (t = p->l.t.t; !s && t != BC_LEX_EOF; t = p->l.t.t) {

		inst = dc_parse_insts[t];

		if (inst != BC_INST_INVALID) {
			bc_parse_push(p, inst);
			s = bc_lex_next(&p->l);
		}
		else
			s = dc_parse_token(p, t, flags);
	}

	if (!s && p->l.t.t == BC_LEX_EOF && (flags & BC_PARSE_NOCALL))
		bc_parse_push(p, BC_INST_POP_EXEC);

	return s;
}

static BcStatus dc_parse_parse(BcParse *p)
{
	BcStatus s;

	if (p->l.t.t == BC_LEX_EOF)
		s = bc_error("end of file");
	else
		s = dc_parse_expr(p, 0);

	if (s || G_interrupt) {
		bc_parse_reset(p);
		s = BC_STATUS_FAILURE;
	}

	return s;
}

static void dc_parse_init(BcParse *p, size_t func)
{
	bc_parse_create(p, func, dc_parse_parse, dc_lex_token);
}

#endif // ENABLE_DC

static void common_parse_init(BcParse *p, size_t func)
{
	if (IS_BC) {
		IF_BC(bc_parse_init(p, func);)
	} else {
		IF_DC(dc_parse_init(p, func);)
	}
}

static BcStatus common_parse_expr(BcParse *p, uint8_t flags)
{
	if (IS_BC) {
		IF_BC(return bc_parse_expression(p, flags);)
	} else {
		IF_DC(return dc_parse_expr(p, flags);)
	}
}

static BcVec* bc_program_search(char *id, bool var)
{
	BcId e, *ptr;
	BcVec *v, *map;
	size_t i;
	BcResultData data;
	int new;

	v = var ? &G.prog.vars : &G.prog.arrs;
	map = var ? &G.prog.var_map : &G.prog.arr_map;

	e.name = id;
	e.idx = v->len;
	new = bc_map_insert(map, &e, &i); // 1 if insertion was successful

	if (new) {
		bc_array_init(&data.v, var);
		bc_vec_push(v, &data.v);
	}

	ptr = bc_vec_item(map, i);
	if (new) ptr->name = xstrdup(e.name);
	return bc_vec_item(v, ptr->idx);
}

static BcStatus bc_program_num(BcResult *r, BcNum **num, bool hex)
{
	BcStatus s = BC_STATUS_SUCCESS;

	switch (r->t) {

		case BC_RESULT_STR:
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
		{
			*num = &r->d.n;
			break;
		}

		case BC_RESULT_CONSTANT:
		{
			char **str = bc_vec_item(&G.prog.consts, r->d.id.idx);
			size_t base_t, len = strlen(*str);
			BcNum *base;

			bc_num_init(&r->d.n, len);

			hex = hex && len == 1;
			base = hex ? &G.prog.hexb : &G.prog.ib;
			base_t = hex ? BC_NUM_MAX_IBASE : G.prog.ib_t;
			s = bc_num_parse(&r->d.n, *str, base, base_t);

			if (s) {
				bc_num_free(&r->d.n);
				return s;
			}

			*num = &r->d.n;
			r->t = BC_RESULT_TEMP;

			break;
		}

		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
		{
			BcVec *v;

			v = bc_program_search(r->d.id.name, r->t == BC_RESULT_VAR);

			if (r->t == BC_RESULT_ARRAY_ELEM) {
				v = bc_vec_top(v);
				if (v->len <= r->d.id.idx) bc_array_expand(v, r->d.id.idx + 1);
				*num = bc_vec_item(v, r->d.id.idx);
			}
			else
				*num = bc_vec_top(v);

			break;
		}

		case BC_RESULT_LAST:
		{
			*num = &G.prog.last;
			break;
		}

		case BC_RESULT_ONE:
		{
			*num = &G.prog.one;
			break;
		}
	}

	return s;
}

static BcStatus bc_program_binOpPrep(BcResult **l, BcNum **ln,
                                     BcResult **r, BcNum **rn, bool assign)
{
	BcStatus s;
	bool hex;
	BcResultType lt, rt;

	if (!BC_PROG_STACK(&G.prog.results, 2))
		return bc_error_stack_has_too_few_elements();

	*r = bc_vec_item_rev(&G.prog.results, 0);
	*l = bc_vec_item_rev(&G.prog.results, 1);

	lt = (*l)->t;
	rt = (*r)->t;
	hex = assign && (lt == BC_RESULT_IBASE || lt == BC_RESULT_OBASE);

	s = bc_program_num(*l, ln, false);
	if (s) return s;
	s = bc_program_num(*r, rn, hex);
	if (s) return s;

	// We run this again under these conditions in case any vector has been
	// reallocated out from under the BcNums or arrays we had.
	if (lt == rt && (lt == BC_RESULT_VAR || lt == BC_RESULT_ARRAY_ELEM)) {
		s = bc_program_num(*l, ln, false);
		if (s) return s;
	}

	if (!BC_PROG_NUM((*l), (*ln)) && (!assign || (*l)->t != BC_RESULT_VAR))
		return bc_error_variable_is_wrong_type();
	if (!assign && !BC_PROG_NUM((*r), (*ln)))
		return bc_error_variable_is_wrong_type();

	return s;
}

static void bc_program_binOpRetire(BcResult *r)
{
	r->t = BC_RESULT_TEMP;
	bc_vec_pop(&G.prog.results);
	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.results, r);
}

static BcStatus bc_program_prep(BcResult **r, BcNum **n)
{
	BcStatus s;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();
	*r = bc_vec_top(&G.prog.results);

	s = bc_program_num(*r, n, false);
	if (s) return s;

	if (!BC_PROG_NUM((*r), (*n)))
		return bc_error_variable_is_wrong_type();

	return s;
}

static void bc_program_retire(BcResult *r, BcResultType t)
{
	r->t = t;
	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.results, r);
}

static BcStatus bc_program_op(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2 = NULL;

	s = bc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) return s;
	bc_num_init_DEF_SIZE(&res.d.n);

	s = bc_program_ops[inst - BC_INST_POWER](n1, n2, &res.d.n, G.prog.scale);
	if (s) goto err;
	bc_program_binOpRetire(&res);

	return s;

err:
	bc_num_free(&res.d.n);
	return s;
}

static BcStatus bc_program_read(void)
{
	const char *sv_file;
	BcStatus s;
	BcParse parse;
	BcVec buf;
	BcInstPtr ip;
	size_t i;
	BcFunc *f = bc_program_func(BC_PROG_READ);

	for (i = 0; i < G.prog.stack.len; ++i) {
		BcInstPtr *ip_ptr = bc_vec_item(&G.prog.stack, i);
		if (ip_ptr->func == BC_PROG_READ)
			return bc_error_nested_read_call();
	}

	bc_vec_pop_all(&f->code);
	bc_char_vec_init(&buf);

	sv_file = G.prog.file;
	G.prog.file = NULL;

	s = bc_read_line(&buf);
	if (s) goto io_err;

	common_parse_init(&parse, BC_PROG_READ);
	bc_lex_file(&parse.l);

	s = bc_parse_text(&parse, buf.v);
	if (s) goto exec_err;
	s = common_parse_expr(&parse, BC_PARSE_NOREAD);
	if (s) goto exec_err;

	if (parse.l.t.t != BC_LEX_NLINE && parse.l.t.t != BC_LEX_EOF) {
		s = bc_error("bad read() expression");
		goto exec_err;
	}

	ip.func = BC_PROG_READ;
	ip.idx = 0;
	ip.len = G.prog.results.len;

	// Update this pointer, just in case.
	f = bc_program_func(BC_PROG_READ);

	bc_vec_pushByte(&f->code, BC_INST_POP_EXEC);
	bc_vec_push(&G.prog.stack, &ip);

exec_err:
	G.prog.file = sv_file;
	bc_parse_free(&parse);
io_err:
	bc_vec_free(&buf);
	return s;
}

static size_t bc_program_index(char *code, size_t *bgn)
{
	char amt = code[(*bgn)++], i = 0;
	size_t res = 0;

	for (; i < amt; ++i, ++(*bgn))
		res |= (((size_t)((int) code[*bgn]) & UCHAR_MAX) << (i * CHAR_BIT));

	return res;
}

static char *bc_program_name(char *code, size_t *bgn)
{
	size_t i;
	char c, *s, *str = code + *bgn, *ptr = strchr(str, BC_PARSE_STREND);

	s = xmalloc(ptr - str + 1);
	c = code[(*bgn)++];

	for (i = 0; c != 0 && c != BC_PARSE_STREND; c = code[(*bgn)++], ++i)
		s[i] = c;

	s[i] = '\0';

	return s;
}

static void bc_program_printString(const char *str)
{
	size_t i, len = strlen(str);

#if ENABLE_DC
	if (len == 0) {
		bb_putchar('\0');
		return;
	}
#endif

	for (i = 0; i < len; ++i, ++G.prog.nchars) {

		int c = str[i];

		if (c != '\\' || i == len - 1)
			bb_putchar(c);
		else {

			c = str[++i];

			switch (c) {

				case 'a':
				{
					bb_putchar('\a');
					break;
				}

				case 'b':
				{
					bb_putchar('\b');
					break;
				}

				case '\\':
				case 'e':
				{
					bb_putchar('\\');
					break;
				}

				case 'f':
				{
					bb_putchar('\f');
					break;
				}

				case 'n':
				{
					bb_putchar('\n');
					G.prog.nchars = SIZE_MAX;
					break;
				}

				case 'r':
				{
					bb_putchar('\r');
					break;
				}

				case 'q':
				{
					bb_putchar('"');
					break;
				}

				case 't':
				{
					bb_putchar('\t');
					break;
				}

				default:
				{
					// Just print the backslash and following character.
					bb_putchar('\\');
					++G.prog.nchars;
					bb_putchar(c);
					break;
				}
			}
		}
	}
}

static BcStatus bc_program_print(char inst, size_t idx)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult *r;
	size_t len, i;
	char *str;
	BcNum *num = NULL;
	bool pop = inst != BC_INST_PRINT;

	if (!BC_PROG_STACK(&G.prog.results, idx + 1))
		return bc_error_stack_has_too_few_elements();

	r = bc_vec_item_rev(&G.prog.results, idx);
	s = bc_program_num(r, &num, false);
	if (s) return s;

	if (BC_PROG_NUM(r, num)) {
		s = bc_num_print(num, !pop);
		if (!s) bc_num_copy(&G.prog.last, num);
	}
	else {

		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : num->rdx;
		str = *bc_program_str(idx);

		if (inst == BC_INST_PRINT_STR) {
			for (i = 0, len = strlen(str); i < len; ++i) {
				char c = str[i];
				bb_putchar(c);
				if (c == '\n') G.prog.nchars = SIZE_MAX;
				++G.prog.nchars;
			}
		}
		else {
			bc_program_printString(str);
			if (inst == BC_INST_PRINT) bb_putchar('\n');
		}
	}

	if (!s && pop) bc_vec_pop(&G.prog.results);

	return s;
}

static BcStatus bc_program_negate(void)
{
	BcStatus s;
	BcResult res, *ptr;
	BcNum *num = NULL;

	s = bc_program_prep(&ptr, &num);
	if (s) return s;

	bc_num_init(&res.d.n, num->len);
	bc_num_copy(&res.d.n, num);
	if (res.d.n.len) res.d.n.neg = !res.d.n.neg;

	bc_program_retire(&res, BC_RESULT_TEMP);

	return s;
}

static BcStatus bc_program_logical(char inst)
{
	BcStatus s;
	BcResult *opd1, *opd2, res;
	BcNum *n1, *n2;
	bool cond = 0;
	ssize_t cmp;

	s = bc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) return s;
	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == BC_INST_BOOL_AND)
		cond = bc_num_cmp(n1, &G.prog.zero) && bc_num_cmp(n2, &G.prog.zero);
	else if (inst == BC_INST_BOOL_OR)
		cond = bc_num_cmp(n1, &G.prog.zero) || bc_num_cmp(n2, &G.prog.zero);
	else {

		cmp = bc_num_cmp(n1, n2);

		switch (inst) {

			case BC_INST_REL_EQ:
			{
				cond = cmp == 0;
				break;
			}

			case BC_INST_REL_LE:
			{
				cond = cmp <= 0;
				break;
			}

			case BC_INST_REL_GE:
			{
				cond = cmp >= 0;
				break;
			}

			case BC_INST_REL_NE:
			{
				cond = cmp != 0;
				break;
			}

			case BC_INST_REL_LT:
			{
				cond = cmp < 0;
				break;
			}

			case BC_INST_REL_GT:
			{
				cond = cmp > 0;
				break;
			}
		}
	}

	(cond ? bc_num_one : bc_num_zero)(&res.d.n);

	bc_program_binOpRetire(&res);

	return s;
}

#if ENABLE_DC
static BcStatus bc_program_assignStr(BcResult *r, BcVec *v,
                                     bool push)
{
	BcNum n2;
	BcResult res;

	memset(&n2, 0, sizeof(BcNum));
	n2.rdx = res.d.id.idx = r->d.id.idx;
	res.t = BC_RESULT_STR;

	if (!push) {
		if (!BC_PROG_STACK(&G.prog.results, 2))
			return bc_error_stack_has_too_few_elements();
		bc_vec_pop(v);
		bc_vec_pop(&G.prog.results);
	}

	bc_vec_pop(&G.prog.results);

	bc_vec_push(&G.prog.results, &res);
	bc_vec_push(v, &n2);

	return BC_STATUS_SUCCESS;
}
#endif // ENABLE_DC

static BcStatus bc_program_copyToVar(char *name, bool var)
{
	BcStatus s;
	BcResult *ptr, r;
	BcVec *v;
	BcNum *n;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();

	ptr = bc_vec_top(&G.prog.results);
	if ((ptr->t == BC_RESULT_ARRAY) != !var)
		return bc_error_variable_is_wrong_type();
	v = bc_program_search(name, var);

#if ENABLE_DC
	if (ptr->t == BC_RESULT_STR && !var)
		return bc_error_variable_is_wrong_type();
	if (ptr->t == BC_RESULT_STR) return bc_program_assignStr(ptr, v, true);
#endif

	s = bc_program_num(ptr, &n, false);
	if (s) return s;

	// Do this once more to make sure that pointers were not invalidated.
	v = bc_program_search(name, var);

	if (var) {
		bc_num_init_DEF_SIZE(&r.d.n);
		bc_num_copy(&r.d.n, n);
	}
	else {
		bc_array_init(&r.d.v, true);
		bc_array_copy(&r.d.v, (BcVec *) n);
	}

	bc_vec_push(v, &r.d);
	bc_vec_pop(&G.prog.results);

	return s;
}

static BcStatus bc_program_assign(char inst)
{
	BcStatus s;
	BcResult *left, *right, res;
	BcNum *l = NULL, *r = NULL;
	bool assign = inst == BC_INST_ASSIGN, ib, sc;

	s = bc_program_binOpPrep(&left, &l, &right, &r, assign);
	if (s) return s;

	ib = left->t == BC_RESULT_IBASE;
	sc = left->t == BC_RESULT_SCALE;

#if ENABLE_DC

	if (right->t == BC_RESULT_STR) {

		BcVec *v;

		if (left->t != BC_RESULT_VAR)
			return bc_error_variable_is_wrong_type();
		v = bc_program_search(left->d.id.name, true);

		return bc_program_assignStr(right, v, false);
	}
#endif

	if (left->t == BC_RESULT_CONSTANT || left->t == BC_RESULT_TEMP)
		return bc_error("bad assignment:"
				" left side must be scale,"
				" ibase, obase, last, var,"
				" or array element"
		);

#if ENABLE_BC
	if (inst == BC_INST_ASSIGN_DIVIDE && !bc_num_cmp(r, &G.prog.zero))
		return bc_error("divide by zero");

	if (assign)
		bc_num_copy(l, r);
	else
		s = bc_program_ops[inst - BC_INST_ASSIGN_POWER](l, r, l, G.prog.scale);

	if (s) return s;
#else
	bc_num_copy(l, r);
#endif

	if (ib || sc || left->t == BC_RESULT_OBASE) {
		static const char *const msg[] = {
			"bad ibase; must be [2,16]",                 //BC_RESULT_IBASE
			"bad scale; must be [0,"BC_MAX_SCALE_STR"]", //BC_RESULT_SCALE
			NULL, //can't happen                         //BC_RESULT_LAST
			NULL, //can't happen                         //BC_RESULT_CONSTANT
			NULL, //can't happen                         //BC_RESULT_ONE
			"bad obase; must be [2,"BC_MAX_OBASE_STR"]", //BC_RESULT_OBASE
		};
		size_t *ptr;
		unsigned long val, max;

		s = bc_num_ulong(l, &val);
		if (s)
			return s;
		s = left->t - BC_RESULT_IBASE;
		if (sc) {
			max = BC_MAX_SCALE;
			ptr = &G.prog.scale;
		}
		else {
			if (val < BC_NUM_MIN_BASE)
				return bc_error(msg[s]);
			max = ib ? BC_NUM_MAX_IBASE : BC_MAX_OBASE;
			ptr = ib ? &G.prog.ib_t : &G.prog.ob_t;
		}

		if (val > max)
			return bc_error(msg[s]);
		if (!sc)
			bc_num_copy(ib ? &G.prog.ib : &G.prog.ob, l);

		*ptr = (size_t) val;
		s = BC_STATUS_SUCCESS;
	}

	bc_num_init(&res.d.n, l->len);
	bc_num_copy(&res.d.n, l);
	bc_program_binOpRetire(&res);

	return s;
}

#if !ENABLE_DC
#define bc_program_pushVar(code, bgn, pop, copy) \
	bc_program_pushVar(code, bgn)
// for bc, 'pop' and 'copy' are always false
#endif
static BcStatus bc_program_pushVar(char *code, size_t *bgn,
                                   bool pop, bool copy)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult r;
	char *name = bc_program_name(code, bgn);

	r.t = BC_RESULT_VAR;
	r.d.id.name = name;

#if ENABLE_DC
	{
		BcVec *v = bc_program_search(name, true);
		BcNum *num = bc_vec_top(v);

		if (pop || copy) {

			if (!BC_PROG_STACK(v, 2 - copy)) {
				free(name);
				return bc_error_stack_has_too_few_elements();
			}

			free(name);
			name = NULL;

			if (!BC_PROG_STR(num)) {

				r.t = BC_RESULT_TEMP;

				bc_num_init_DEF_SIZE(&r.d.n);
				bc_num_copy(&r.d.n, num);
			}
			else {
				r.t = BC_RESULT_STR;
				r.d.id.idx = num->rdx;
			}

			if (!copy) bc_vec_pop(v);
		}
	}
#endif // ENABLE_DC

	bc_vec_push(&G.prog.results, &r);

	return s;
}

static BcStatus bc_program_pushArray(char *code, size_t *bgn,
                                     char inst)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult r;
	BcNum *num;

	r.d.id.name = bc_program_name(code, bgn);

	if (inst == BC_INST_ARRAY) {
		r.t = BC_RESULT_ARRAY;
		bc_vec_push(&G.prog.results, &r);
	}
	else {

		BcResult *operand;
		unsigned long temp;

		s = bc_program_prep(&operand, &num);
		if (s) goto err;
		s = bc_num_ulong(num, &temp);
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
	return s;
}

#if ENABLE_BC
static BcStatus bc_program_incdec(char inst)
{
	BcStatus s;
	BcResult *ptr, res, copy;
	BcNum *num = NULL;
	char inst2 = inst;

	s = bc_program_prep(&ptr, &num);
	if (s) return s;

	if (inst == BC_INST_INC_POST || inst == BC_INST_DEC_POST) {
		copy.t = BC_RESULT_TEMP;
		bc_num_init(&copy.d.n, num->len);
		bc_num_copy(&copy.d.n, num);
	}

	res.t = BC_RESULT_ONE;
	inst = inst == BC_INST_INC_PRE || inst == BC_INST_INC_POST ?
	           BC_INST_ASSIGN_PLUS :
	           BC_INST_ASSIGN_MINUS;

	bc_vec_push(&G.prog.results, &res);
	bc_program_assign(inst);

	if (inst2 == BC_INST_INC_POST || inst2 == BC_INST_DEC_POST) {
		bc_vec_pop(&G.prog.results);
		bc_vec_push(&G.prog.results, &copy);
	}

	return s;
}

static BcStatus bc_program_call(char *code, size_t *idx)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcInstPtr ip;
	size_t i, nparams = bc_program_index(code, idx);
	BcFunc *func;
	BcId *a;
	BcResultData param;
	BcResult *arg;

	ip.idx = 0;
	ip.func = bc_program_index(code, idx);
	func = bc_program_func(ip.func);

	if (func->code.len == 0) {
		return bc_error("undefined function");
	}
	if (nparams != func->nparams) {
		return bc_error_fmt("function has %u parameters, but called with %u", func->nparams, nparams);
	}
	ip.len = G.prog.results.len - nparams;

	for (i = 0; i < nparams; ++i) {

		a = bc_vec_item(&func->autos, nparams - 1 - i);
		arg = bc_vec_top(&G.prog.results);

		if ((!a->idx) != (arg->t == BC_RESULT_ARRAY) || arg->t == BC_RESULT_STR)
			return bc_error_variable_is_wrong_type();

		s = bc_program_copyToVar(a->name, a->idx);
		if (s) return s;
	}

	for (; i < func->autos.len; ++i) {
		BcVec *v;

		a = bc_vec_item(&func->autos, i);
		v = bc_program_search(a->name, a->idx);

		if (a->idx) {
			bc_num_init_DEF_SIZE(&param.n);
			bc_vec_push(v, &param.n);
		}
		else {
			bc_array_init(&param.v, true);
			bc_vec_push(v, &param.v);
		}
	}

	bc_vec_push(&G.prog.stack, &ip);

	return BC_STATUS_SUCCESS;
}

static BcStatus bc_program_return(char inst)
{
	BcStatus s;
	BcResult res;
	BcFunc *f;
	size_t i;
	BcInstPtr *ip = bc_vec_top(&G.prog.stack);

	if (!BC_PROG_STACK(&G.prog.results, ip->len + inst == BC_INST_RET))
		return bc_error_stack_has_too_few_elements();

	f = bc_program_func(ip->func);
	res.t = BC_RESULT_TEMP;

	if (inst == BC_INST_RET) {

		BcNum *num;
		BcResult *operand = bc_vec_top(&G.prog.results);

		s = bc_program_num(operand, &num, false);
		if (s) return s;
		bc_num_init(&res.d.n, num->len);
		bc_num_copy(&res.d.n, num);
	}
	else {
		bc_num_init_DEF_SIZE(&res.d.n);
		//bc_num_zero(&res.d.n); - already is
	}

	// We need to pop arguments as well, so this takes that into account.
	for (i = 0; i < f->autos.len; ++i) {

		BcVec *v;
		BcId *a = bc_vec_item(&f->autos, i);

		v = bc_program_search(a->name, a->idx);
		bc_vec_pop(v);
	}

	bc_vec_npop(&G.prog.results, G.prog.results.len - ip->len);
	bc_vec_push(&G.prog.results, &res);
	bc_vec_pop(&G.prog.stack);

	return BC_STATUS_SUCCESS;
}
#endif // ENABLE_BC

static unsigned long bc_program_scale(BcNum *n)
{
	return (unsigned long) n->rdx;
}

static unsigned long bc_program_len(BcNum *n)
{
	unsigned long len = n->len;
	size_t i;

	if (n->rdx != n->len) return len;
	for (i = n->len - 1; i < n->len && n->num[i] == 0; --len, --i);

	return len;
}

static BcStatus bc_program_builtin(char inst)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num = NULL;
	BcResult res;
	bool len = inst == BC_INST_LENGTH;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();
	opnd = bc_vec_top(&G.prog.results);

	s = bc_program_num(opnd, &num, false);
	if (s) return s;

#if ENABLE_DC
	if (!BC_PROG_NUM(opnd, num) && !len)
		return bc_error_variable_is_wrong_type();
#endif

	bc_num_init_DEF_SIZE(&res.d.n);

	if (inst == BC_INST_SQRT) s = bc_num_sqrt(num, &res.d.n, G.prog.scale);
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
		BcProgramBuiltIn f = len ? bc_program_len : bc_program_scale;
		bc_num_ulong2num(&res.d.n, f(num));
	}

	bc_program_retire(&res, BC_RESULT_TEMP);

	return s;
}

#if ENABLE_DC
static BcStatus bc_program_divmod(void)
{
	BcStatus s;
	BcResult *opd1, *opd2, res, res2;
	BcNum *n1, *n2 = NULL;

	s = bc_program_binOpPrep(&opd1, &n1, &opd2, &n2, false);
	if (s) return s;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_init(&res2.d.n, n2->len);

	s = bc_num_divmod(n1, n2, &res2.d.n, &res.d.n, G.prog.scale);
	if (s) goto err;

	bc_program_binOpRetire(&res2);
	res.t = BC_RESULT_TEMP;
	bc_vec_push(&G.prog.results, &res);

	return s;

err:
	bc_num_free(&res2.d.n);
	bc_num_free(&res.d.n);
	return s;
}

static BcStatus bc_program_modexp(void)
{
	BcStatus s;
	BcResult *r1, *r2, *r3, res;
	BcNum *n1, *n2, *n3;

	if (!BC_PROG_STACK(&G.prog.results, 3))
		return bc_error_stack_has_too_few_elements();
	s = bc_program_binOpPrep(&r2, &n2, &r3, &n3, false);
	if (s) return s;

	r1 = bc_vec_item_rev(&G.prog.results, 2);
	s = bc_program_num(r1, &n1, false);
	if (s) return s;
	if (!BC_PROG_NUM(r1, n1))
		return bc_error_variable_is_wrong_type();

	// Make sure that the values have their pointers updated, if necessary.
	if (r1->t == BC_RESULT_VAR || r1->t == BC_RESULT_ARRAY_ELEM) {

		if (r1->t == r2->t) {
			s = bc_program_num(r2, &n2, false);
			if (s) return s;
		}

		if (r1->t == r3->t) {
			s = bc_program_num(r3, &n3, false);
			if (s) return s;
		}
	}

	bc_num_init(&res.d.n, n3->len);
	s = bc_num_modexp(n1, n2, n3, &res.d.n);
	if (s) goto err;

	bc_vec_pop(&G.prog.results);
	bc_program_binOpRetire(&res);

	return s;

err:
	bc_num_free(&res.d.n);
	return s;
}

static void bc_program_stackLen(void)
{
	BcResult res;
	size_t len = G.prog.results.len;

	res.t = BC_RESULT_TEMP;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, len);
	bc_vec_push(&G.prog.results, &res);
}

static BcStatus bc_program_asciify(void)
{
	BcStatus s;
	BcResult *r, res;
	BcNum *num, n;
	char *str, *str2, c;
	size_t len = G.prog.strs.len, idx;
	unsigned long val;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();
	r = bc_vec_top(&G.prog.results);

	num = NULL; // TODO: is this NULL needed?
	s = bc_program_num(r, &num, false);
	if (s) return s;

	if (BC_PROG_NUM(r, num)) {

		bc_num_init_DEF_SIZE(&n);
		bc_num_copy(&n, num);
		bc_num_truncate(&n, n.rdx);

		s = bc_num_mod(&n, &G.prog.strmb, &n, 0);
		if (s) goto num_err;
		s = bc_num_ulong(&n, &val);
		if (s) goto num_err;

		c = (char) val;

		bc_num_free(&n);
	}
	else {
		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : num->rdx;
		str2 = *bc_program_str(idx);
		c = str2[0];
	}

	str = xmalloc(2);
	str[0] = c;
	str[1] = '\0';

	str2 = xstrdup(str);
	bc_program_addFunc(str2, &idx);

	if (idx != len + BC_PROG_REQ_FUNCS) {

		for (idx = 0; idx < G.prog.strs.len; ++idx) {
			if (strcmp(*bc_program_str(idx), str) == 0) {
				len = idx;
				break;
			}
		}

		free(str);
	}
	else
		bc_vec_push(&G.prog.strs, &str);

	res.t = BC_RESULT_STR;
	res.d.id.idx = len;
	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.results, &res);

	return BC_STATUS_SUCCESS;

num_err:
	bc_num_free(&n);
	return s;
}

static BcStatus bc_program_printStream(void)
{
	BcStatus s;
	BcResult *r;
	BcNum *n = NULL;
	size_t idx;
	char *str;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();
	r = bc_vec_top(&G.prog.results);

	s = bc_program_num(r, &n, false);
	if (s) return s;

	if (BC_PROG_NUM(r, n))
		s = bc_num_stream(n, &G.prog.strmb);
	else {
		idx = (r->t == BC_RESULT_STR) ? r->d.id.idx : n->rdx;
		str = *bc_program_str(idx);
		printf("%s", str);
	}

	return s;
}

static BcStatus bc_program_nquit(void)
{
	BcStatus s;
	BcResult *opnd;
	BcNum *num = NULL;
	unsigned long val;

	s = bc_program_prep(&opnd, &num);
	if (s) return s;
	s = bc_num_ulong(num, &val);
	if (s) return s;

	bc_vec_pop(&G.prog.results);

	if (G.prog.stack.len < val)
		return bc_error_stack_has_too_few_elements();
	if (G.prog.stack.len == val) {
		QUIT_OR_RETURN_TO_MAIN;
	}

	bc_vec_npop(&G.prog.stack, val);

	return s;
}

static BcStatus bc_program_execStr(char *code, size_t *bgn,
                                   bool cond)
{
	BcStatus s = BC_STATUS_SUCCESS;
	BcResult *r;
	char **str;
	BcFunc *f;
	BcParse prs;
	BcInstPtr ip;
	size_t fidx, sidx;

	if (!BC_PROG_STACK(&G.prog.results, 1))
		return bc_error_stack_has_too_few_elements();

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
			s = bc_program_num(r, &n, false);
			if (s || !BC_PROG_STR(n)) goto exit;
			sidx = n->rdx;
		} else
			goto exit;
	}

	fidx = sidx + BC_PROG_REQ_FUNCS;

	str = bc_program_str(sidx);
	f = bc_program_func(fidx);

	if (f->code.len == 0) {
		common_parse_init(&prs, fidx);
		s = bc_parse_text(&prs, *str);
		if (s) goto err;
		s = common_parse_expr(&prs, BC_PARSE_NOCALL);
		if (s) goto err;

		if (prs.l.t.t != BC_LEX_EOF) {
			s = bc_error_bad_expression();
			goto err;
		}

		bc_parse_free(&prs);
	}

	ip.idx = 0;
	ip.len = G.prog.results.len;
	ip.func = fidx;

	bc_vec_pop(&G.prog.results);
	bc_vec_push(&G.prog.stack, &ip);

	return BC_STATUS_SUCCESS;

err:
	bc_parse_free(&prs);
	f = bc_program_func(fidx);
	bc_vec_pop_all(&f->code);
exit:
	bc_vec_pop(&G.prog.results);
	return s;
}
#endif // ENABLE_DC

static void bc_program_pushGlobal(char inst)
{
	BcResult res;
	unsigned long val;

	res.t = inst - BC_INST_IBASE + BC_RESULT_IBASE;
	if (inst == BC_INST_IBASE)
		val = (unsigned long) G.prog.ib_t;
	else if (inst == BC_INST_SCALE)
		val = (unsigned long) G.prog.scale;
	else
		val = (unsigned long) G.prog.ob_t;

	bc_num_init_DEF_SIZE(&res.d.n);
	bc_num_ulong2num(&res.d.n, val);
	bc_vec_push(&G.prog.results, &res);
}

static void bc_program_addFunc(char *name, size_t *idx)
{
	BcId entry, *entry_ptr;
	BcFunc f;
	int inserted;

	entry.name = name;
	entry.idx = G.prog.fns.len;

	inserted = bc_map_insert(&G.prog.fn_map, &entry, idx);
	if (!inserted) free(name);

	entry_ptr = bc_vec_item(&G.prog.fn_map, *idx);
	*idx = entry_ptr->idx;

	if (!inserted) {

		BcFunc *func = bc_program_func(entry_ptr->idx);

		// We need to reset these, so the function can be repopulated.
		func->nparams = 0;
		bc_vec_pop_all(&func->autos);
		bc_vec_pop_all(&func->code);
		bc_vec_pop_all(&func->labels);
	}
	else {
		bc_func_init(&f);
		bc_vec_push(&G.prog.fns, &f);
	}
}

static BcStatus bc_program_exec(void)
{
	BcResult r, *ptr;
	BcNum *num;
	BcInstPtr *ip = bc_vec_top(&G.prog.stack);
	BcFunc *func = bc_program_func(ip->func);
	char *code = func->code.v;
	bool cond = false;

	while (ip->idx < func->code.len) {
		BcStatus s;
		char inst = code[(ip->idx)++];

		switch (inst) {
#if ENABLE_BC
			case BC_INST_JUMP_ZERO:
				s = bc_program_prep(&ptr, &num);
				if (s) return s;
				cond = !bc_num_cmp(num, &G.prog.zero);
				bc_vec_pop(&G.prog.results);
				// Fallthrough.
			case BC_INST_JUMP: {
				size_t *addr;
				size_t idx = bc_program_index(code, &ip->idx);
				addr = bc_vec_item(&func->labels, idx);
				if (inst == BC_INST_JUMP || cond) ip->idx = *addr;
				break;
			}
			case BC_INST_CALL:
				s = bc_program_call(code, &ip->idx);
				break;
			case BC_INST_INC_PRE:
			case BC_INST_DEC_PRE:
			case BC_INST_INC_POST:
			case BC_INST_DEC_POST:
				s = bc_program_incdec(inst);
				break;
			case BC_INST_HALT:
				QUIT_OR_RETURN_TO_MAIN;
				break;
			case BC_INST_RET:
			case BC_INST_RET0:
				s = bc_program_return(inst);
				break;
			case BC_INST_BOOL_OR:
			case BC_INST_BOOL_AND:
#endif // ENABLE_BC
			case BC_INST_REL_EQ:
			case BC_INST_REL_LE:
			case BC_INST_REL_GE:
			case BC_INST_REL_NE:
			case BC_INST_REL_LT:
			case BC_INST_REL_GT:
				s = bc_program_logical(inst);
				break;
			case BC_INST_READ:
				s = bc_program_read();
				break;
			case BC_INST_VAR:
				s = bc_program_pushVar(code, &ip->idx, false, false);
				break;
			case BC_INST_ARRAY_ELEM:
			case BC_INST_ARRAY:
				s = bc_program_pushArray(code, &ip->idx, inst);
				break;
			case BC_INST_LAST:
				r.t = BC_RESULT_LAST;
				bc_vec_push(&G.prog.results, &r);
				break;
			case BC_INST_IBASE:
			case BC_INST_SCALE:
			case BC_INST_OBASE:
				bc_program_pushGlobal(inst);
				break;
			case BC_INST_SCALE_FUNC:
			case BC_INST_LENGTH:
			case BC_INST_SQRT:
				s = bc_program_builtin(inst);
				break;
			case BC_INST_NUM:
				r.t = BC_RESULT_CONSTANT;
				r.d.id.idx = bc_program_index(code, &ip->idx);
				bc_vec_push(&G.prog.results, &r);
				break;
			case BC_INST_POP:
				if (!BC_PROG_STACK(&G.prog.results, 1))
					s = bc_error_stack_has_too_few_elements();
				else
					bc_vec_pop(&G.prog.results);
				break;
			case BC_INST_POP_EXEC:
				bc_vec_pop(&G.prog.stack);
				break;
			case BC_INST_PRINT:
			case BC_INST_PRINT_POP:
			case BC_INST_PRINT_STR:
				s = bc_program_print(inst, 0);
				break;
			case BC_INST_STR:
				r.t = BC_RESULT_STR;
				r.d.id.idx = bc_program_index(code, &ip->idx);
				bc_vec_push(&G.prog.results, &r);
				break;
			case BC_INST_POWER:
			case BC_INST_MULTIPLY:
			case BC_INST_DIVIDE:
			case BC_INST_MODULUS:
			case BC_INST_PLUS:
			case BC_INST_MINUS:
				s = bc_program_op(inst);
				break;
			case BC_INST_BOOL_NOT:
				s = bc_program_prep(&ptr, &num);
				if (s) return s;
				bc_num_init_DEF_SIZE(&r.d.n);
				if (!bc_num_cmp(num, &G.prog.zero))
					bc_num_one(&r.d.n);
				//else bc_num_zero(&r.d.n); - already is
				bc_program_retire(&r, BC_RESULT_TEMP);
				break;
			case BC_INST_NEG:
				s = bc_program_negate();
				break;
#if ENABLE_BC
			case BC_INST_ASSIGN_POWER:
			case BC_INST_ASSIGN_MULTIPLY:
			case BC_INST_ASSIGN_DIVIDE:
			case BC_INST_ASSIGN_MODULUS:
			case BC_INST_ASSIGN_PLUS:
			case BC_INST_ASSIGN_MINUS:
#endif
			case BC_INST_ASSIGN:
				s = bc_program_assign(inst);
				break;
#if ENABLE_DC
			case BC_INST_MODEXP:
				s = bc_program_modexp();
				break;
			case BC_INST_DIVMOD:
				s = bc_program_divmod();
				break;
			case BC_INST_EXECUTE:
			case BC_INST_EXEC_COND:
				cond = inst == BC_INST_EXEC_COND;
				s = bc_program_execStr(code, &ip->idx, cond);
				break;
			case BC_INST_PRINT_STACK: {
				size_t idx;
				for (idx = 0; idx < G.prog.results.len; ++idx) {
					s = bc_program_print(BC_INST_PRINT, idx);
					if (s) break;
				}
				break;
			}
			case BC_INST_CLEAR_STACK:
				bc_vec_pop_all(&G.prog.results);
				break;
			case BC_INST_STACK_LEN:
				bc_program_stackLen();
				break;
			case BC_INST_DUPLICATE:
				if (!BC_PROG_STACK(&G.prog.results, 1))
					return bc_error_stack_has_too_few_elements();
				ptr = bc_vec_top(&G.prog.results);
				bc_result_copy(&r, ptr);
				bc_vec_push(&G.prog.results, &r);
				break;
			case BC_INST_SWAP: {
				BcResult *ptr2;
				if (!BC_PROG_STACK(&G.prog.results, 2))
					return bc_error_stack_has_too_few_elements();
				ptr = bc_vec_item_rev(&G.prog.results, 0);
				ptr2 = bc_vec_item_rev(&G.prog.results, 1);
				memcpy(&r, ptr, sizeof(BcResult));
				memcpy(ptr, ptr2, sizeof(BcResult));
				memcpy(ptr2, &r, sizeof(BcResult));
				break;
			}
			case BC_INST_ASCIIFY:
				s = bc_program_asciify();
				break;
			case BC_INST_PRINT_STREAM:
				s = bc_program_printStream();
				break;
			case BC_INST_LOAD:
			case BC_INST_PUSH_VAR: {
				bool copy = inst == BC_INST_LOAD;
				s = bc_program_pushVar(code, &ip->idx, true, copy);
				break;
			}
			case BC_INST_PUSH_TO_VAR: {
				char *name = bc_program_name(code, &ip->idx);
				s = bc_program_copyToVar(name, true);
				free(name);
				break;
			}
			case BC_INST_QUIT:
				if (G.prog.stack.len <= 2)
					QUIT_OR_RETURN_TO_MAIN;
				bc_vec_npop(&G.prog.stack, 2);
				break;
			case BC_INST_NQUIT:
				s = bc_program_nquit();
				break;
#endif // ENABLE_DC
		}

		if (s || G_interrupt) {
			bc_program_reset();
			return s;
		}

		// If the stack has changed, pointers may be invalid.
		ip = bc_vec_top(&G.prog.stack);
		func = bc_program_func(ip->func);
		code = func->code.v;
	}

	return BC_STATUS_SUCCESS;
}

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
#endif // ENABLE_BC

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

static BcStatus bc_vm_process(const char *text)
{
	BcStatus s = bc_parse_text(&G.prs, text);

	if (s) return s;

	while (G.prs.l.t.t != BC_LEX_EOF) {
		s = G.prs.parse(&G.prs);
		if (s) return s;
	}

	if (BC_PARSE_CAN_EXEC(&G.prs)) {
		s = bc_program_exec();
		fflush_and_check();
		if (s)
			bc_program_reset();
	}

	return s;
}

static BcStatus bc_vm_file(const char *file)
{
	const char *sv_file;
	char *data;
	BcStatus s;
	BcFunc *main_func;
	BcInstPtr *ip;

	data = bc_read_file(file);
	if (!data) return bc_error_fmt("file '%s' is not text", file);

	sv_file = G.prog.file;
	G.prog.file = file;
	bc_lex_file(&G.prs.l);
	s = bc_vm_process(data);
	if (s) goto err;

	main_func = bc_program_func(BC_PROG_MAIN);
	ip = bc_vec_item(&G.prog.stack, 0);

	if (main_func->code.len < ip->idx)
		s = bc_error_fmt("file '%s' is not executable", file);

err:
	G.prog.file = sv_file;
	free(data);
	return s;
}

static BcStatus bc_vm_stdin(void)
{
	BcStatus s;
	BcVec buf, buffer;
	size_t len, i, str = 0;
	bool comment = false;

	G.prog.file = NULL;
	bc_lex_file(&G.prs.l);

	bc_char_vec_init(&buffer);
	bc_char_vec_init(&buf);
	bc_vec_pushZeroByte(&buffer);

	// This loop is complex because the vm tries not to send any lines that end
	// with a backslash to the parser. The reason for that is because the parser
	// treats a backslash+newline combo as whitespace, per the bc spec. In that
	// case, and for strings and comments, the parser will expect more stuff.
	while ((s = bc_read_line(&buf)) == BC_STATUS_SUCCESS) {

		char *string = buf.v;

		len = buf.len - 1;

		if (len == 1) {
			if (str && buf.v[0] == G.send)
				str -= 1;
			else if (buf.v[0] == G.sbgn)
				str += 1;
		}
		else if (len > 1 || comment) {

			for (i = 0; i < len; ++i) {

				bool notend = len > i + 1;
				char c = string[i];

				if (i - 1 > len || string[i - 1] != '\\') {
					if (G.sbgn == G.send)
						str ^= c == G.sbgn;
					else if (c == G.send)
						str -= 1;
					else if (c == G.sbgn)
						str += 1;
				}

				if (c == '/' && notend && !comment && string[i + 1] == '*') {
					comment = true;
					break;
				}
				else if (c == '*' && notend && comment && string[i + 1] == '/')
					comment = false;
			}

			if (str || comment || string[len - 2] == '\\') {
				bc_vec_concat(&buffer, buf.v);
				continue;
			}
		}

		bc_vec_concat(&buffer, buf.v);
		s = bc_vm_process(buffer.v);
		if (s) {
			if (ENABLE_FEATURE_CLEAN_UP && !G_ttyin) {
				// Debug config, non-interactive mode:
				// return all the way back to main.
				// Non-debug builds do not come here, they exit.
				break;
			}
		}

		bc_vec_pop_all(&buffer);
	}
	if (s == BC_STATUS_EOF) // input EOF (^D) is not an error
		s = BC_STATUS_SUCCESS;

	if (str) {
		s = bc_error("string end could not be found");
	}
	else if (comment) {
		s = bc_error("comment end could not be found");
	}

	bc_vec_free(&buf);
	bc_vec_free(&buffer);
	return s;
}

#if ENABLE_BC
static const char bc_lib[] = {
	"scale=20"
"\n"	"define e(x){"
"\n"		"auto b,s,n,r,d,i,p,f,v"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"if(x<0){"
"\n"			"n=1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"s=scale"
"\n"		"r=6+s+0.44*x"
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
"\n"		"for(i=2;v!=0;++i){"
"\n"			"p*=x"
"\n"			"f*=i"
"\n"			"v=p/f"
"\n"			"r+=v"
"\n"		"}"
"\n"		"while((d--)!=0)r*=r"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(n!=0)return(1/r)"
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
"\n"		"while(x<=0.5){"
"\n"			"p*=2"
"\n"			"x=sqrt(x)"
"\n"		"}"
"\n"		"r=a=(x-1)/(x+1)"
"\n"		"q=a*a"
"\n"			"v=1"
"\n"		"for(i=3;v!=0;i+=2){"
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
"\n"		"auto b,s,r,n,a,q,i"
"\n"		"b=ibase"
"\n"		"ibase=A"
"\n"		"s=scale"
"\n"		"scale=1.1*s+2"
"\n"		"a=a(1)"
"\n"		"if(x<0){"
"\n"			"n=1"
"\n"			"x=-x"
"\n"		"}"
"\n"		"scale=0"
"\n"		"q=(x/a+2)/4"
"\n"		"x=x-4*q*a"
"\n"		"if(q%2!=0)x=-x"
"\n"		"scale=s+2"
"\n"		"r=a=x"
"\n"		"q=-x*x"
"\n"		"for(i=3;a!=0;i+=2){"
"\n"			"a*=q/(i*(i-1))"
"\n"			"r+=a"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(n!=0)return(-r/1)"
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
"\n"		"if(x==1){"
"\n"			"if(scale<65){"
"\n"				"return(.7853981633974483096156608458198757210492923498437764552437361480/n)"
"\n"			"}"
"\n"		"}"
"\n"		"if(x==.2){"
"\n"			"if(scale<65){"
"\n"				"return(.1973955598498807583700497651947902934475851037878521015176889402/n)"
"\n"			"}"
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
"\n"		"for(i=3;t!=0;i+=2){"
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
"\n"			"if(n%2==1)o=1"
"\n"		"}"
"\n"		"a=1"
"\n"		"for(i=2;i<=n;++i)a*=i"
"\n"		"scale=1.5*s"
"\n"		"a=(x^n)/2^n/a"
"\n"		"r=v=1"
"\n"		"f=-x*x/4"
"\n"		"scale=scale+length(a)-scale(a)"
"\n"		"for(i=1;v!=0;++i){"
"\n"			"v=v*f/i/(n+i)"
"\n"			"r+=v"
"\n"		"}"
"\n"		"scale=s"
"\n"		"ibase=b"
"\n"		"if(o!=0)a=-a"
"\n"		"return(a*r/1)"
"\n"	"}"
};
#endif // ENABLE_BC

static BcStatus bc_vm_exec(void)
{
	BcStatus s;
	size_t i;

#if ENABLE_BC
	if (option_mask32 & BC_FLAG_L) {

		// We know that internal library is not buggy,
		// thus error checking is normally disabled.
# define DEBUG_LIB 0
		bc_lex_file(&G.prs.l);
		s = bc_parse_text(&G.prs, bc_lib);
		if (DEBUG_LIB && s) return s;

		while (G.prs.l.t.t != BC_LEX_EOF) {
			s = G.prs.parse(&G.prs);
			if (DEBUG_LIB && s) return s;
		}
		s = bc_program_exec();
		if (DEBUG_LIB && s) return s;
	}
#endif

	s = BC_STATUS_SUCCESS;
	for (i = 0; !s && i < G.files.len; ++i)
		s = bc_vm_file(*((char **) bc_vec_item(&G.files, i)));
	if (ENABLE_FEATURE_CLEAN_UP && s && !G_ttyin) {
		// Debug config, non-interactive mode:
		// return all the way back to main.
		// Non-debug builds do not come here, they exit.
		return s;
	}

	if (IS_BC || (option_mask32 & BC_FLAG_I)) 
		s = bc_vm_stdin();

	if (!s && !BC_PARSE_CAN_EXEC(&G.prs))
		s = bc_vm_process("");

	return s;
}

#if ENABLE_FEATURE_CLEAN_UP
static void bc_program_free(void)
{
	bc_num_free(&G.prog.ib);
	bc_num_free(&G.prog.ob);
	bc_num_free(&G.prog.hexb);
# if ENABLE_DC
	bc_num_free(&G.prog.strmb);
# endif
	bc_vec_free(&G.prog.fns);
	bc_vec_free(&G.prog.fn_map);
	bc_vec_free(&G.prog.vars);
	bc_vec_free(&G.prog.var_map);
	bc_vec_free(&G.prog.arrs);
	bc_vec_free(&G.prog.arr_map);
	bc_vec_free(&G.prog.strs);
	bc_vec_free(&G.prog.consts);
	bc_vec_free(&G.prog.results);
	bc_vec_free(&G.prog.stack);
	bc_num_free(&G.prog.last);
	bc_num_free(&G.prog.zero);
	bc_num_free(&G.prog.one);
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
	size_t idx;
	BcInstPtr ip;

	/* memset(&G.prog, 0, sizeof(G.prog)); - already is */
	memset(&ip, 0, sizeof(BcInstPtr));

	/* G.prog.nchars = G.prog.scale = 0; - already is */
	bc_num_init_DEF_SIZE(&G.prog.ib);
	bc_num_ten(&G.prog.ib);
	G.prog.ib_t = 10;

	bc_num_init_DEF_SIZE(&G.prog.ob);
	bc_num_ten(&G.prog.ob);
	G.prog.ob_t = 10;

	bc_num_init_DEF_SIZE(&G.prog.hexb);
	bc_num_ten(&G.prog.hexb);
	G.prog.hexb.num[0] = 6;

#if ENABLE_DC
	bc_num_init_DEF_SIZE(&G.prog.strmb);
	bc_num_ulong2num(&G.prog.strmb, UCHAR_MAX + 1);
#endif

	bc_num_init_DEF_SIZE(&G.prog.last);
	//bc_num_zero(&G.prog.last); - already is

	bc_num_init_DEF_SIZE(&G.prog.zero);
	//bc_num_zero(&G.prog.zero); - already is

	bc_num_init_DEF_SIZE(&G.prog.one);
	bc_num_one(&G.prog.one);

	bc_vec_init(&G.prog.fns, sizeof(BcFunc), bc_func_free);
	bc_vec_init(&G.prog.fn_map, sizeof(BcId), bc_id_free);

	bc_program_addFunc(xstrdup("(main)"), &idx);
	bc_program_addFunc(xstrdup("(read)"), &idx);

	bc_vec_init(&G.prog.vars, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.var_map, sizeof(BcId), bc_id_free);

	bc_vec_init(&G.prog.arrs, sizeof(BcVec), bc_vec_free);
	bc_vec_init(&G.prog.arr_map, sizeof(BcId), bc_id_free);

	bc_vec_init(&G.prog.strs, sizeof(char *), bc_string_free);
	bc_vec_init(&G.prog.consts, sizeof(char *), bc_string_free);
	bc_vec_init(&G.prog.results, sizeof(BcResult), bc_result_free);
	bc_vec_init(&G.prog.stack, sizeof(BcInstPtr), NULL);
	bc_vec_push(&G.prog.stack, &ip);
}

static int bc_vm_init(const char *env_len)
{
#if ENABLE_FEATURE_EDITING
	G.line_input_state = new_line_input_t(DO_HISTORY);
#endif
	G.prog.len = bc_vm_envLen(env_len);

	bc_vec_init(&G.files, sizeof(char *), NULL);
	if (IS_BC)
		IF_BC(bc_vm_envArgs();)
	bc_program_init();
	if (IS_BC) {
		IF_BC(bc_parse_init(&G.prs, BC_PROG_MAIN);)
	} else {
		IF_DC(dc_parse_init(&G.prs, BC_PROG_MAIN);)
	}

	if (isatty(0)) {
#if ENABLE_FEATURE_BC_SIGNALS
		G_ttyin = 1;
		// With SA_RESTART, most system calls will restart
		// (IOW: they won't fail with EINTR).
		// In particular, this means ^C won't cause
		// stdout to get into "error state" if SIGINT hits
		// within write() syscall.
		// The downside is that ^C while line input is taken
		// will only be handled after [Enter] since read()
		// from stdin is not interrupted by ^C either,
		// it restarts, thus fgetc() does not return on ^C.
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
	BcStatus st = bc_vm_exec();
#if ENABLE_FEATURE_CLEAN_UP
	if (G_exiting) // it was actually "halt" or "quit"
		st = EXIT_SUCCESS;
	bc_vm_free();
# if ENABLE_FEATURE_EDITING
	free_line_input_t(G.line_input_state);
# endif
	FREE_G();
#endif
	return st;
}

#if ENABLE_BC
int bc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bc_main(int argc UNUSED_PARAM, char **argv)
{
	int is_tty;

	INIT_G();
	G.sbgn = G.send = '"';

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
	G.sbgn = '[';
	G.send = ']';
	/*
	 * TODO: dc (GNU bc 1.07.1) 1.4.1 seems to use width
	 * 1 char wider than bc from the same package.
	 * Both default width, and xC_LINE_LENGTH=N are wider:
	 * "DC_LINE_LENGTH=5 dc -e'123456 p'" prints:
	 *	1234\
	 *	56
	 * "echo '123456' | BC_LINE_LENGTH=5 bc" prints:
	 *	123\
	 *	456
	 * Do the same, or it's a bug?
	 */
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
			n = bc_vm_process(optarg);
			if (n) return n;
			break;
		case 'f':
			noscript = 0;
			bc_vm_file(optarg);
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
