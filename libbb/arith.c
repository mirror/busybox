/* Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>
   
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
   
   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* This is my infix parser/evaluator. It is optimized for size, intended
 * as a replacement for yacc-based parsers. However, it may well be faster
 * than a comparable parser writen in yacc. The supported operators are
 * listed in #defines below. Parens, order of operations, and error handling
 * are supported. This code is threadsafe. The exact expression format should
 * be that which POSIX specifies for shells. */

/* The code uses a simple two-stack algorithm. See
 * http://www.onthenet.com.au/~grahamis/int2008/week02/lect02.html
 * for a detailed explaination of the infix-to-postfix algorithm on which
 * this is based (this code differs in that it applies operators immediately
 * to the stack instead of adding them to a queue to end up with an
 * expression). */

/* To use the routine, call it with an expression string and error return
 * pointer */

/*
 * Aug 24, 2001              Manuel Novoa III
 *
 * Reduced the generated code size by about 30% (i386) and fixed several bugs.
 *
 * 1) In arith_apply():
 *    a) Cached values of *numptr and &(numptr[-1]).
 *    b) Removed redundant test for zero denominator.
 *
 * 2) In arith():
 *    a) Eliminated redundant code for processing operator tokens by moving
 *       to a table-based implementation.  Also folded handling of parens
 *       into the table.
 *    b) Combined all 3 loops which called arith_apply to reduce generated
 *       code size at the cost of speed.
 *
 * 3) The following expressions were treated as valid by the original code:
 *       1()  ,    0!  ,    1 ( *3 )   .
 *    These bugs have been fixed by internally enclosing the expression in
 *    parens and then checking that all binary ops and right parens are
 *    preceded by a valid expression (NUM_TOKEN).
 *
 * Note: It may be desireable to replace Aaron's test for whitespace with
 * ctype's isspace() if it is used by another busybox applet or if additional
 * whitespace chars should be considered.  Look below the "#include"s for a
 * precompiler test.
 */

/*
 * Aug 26, 2001              Manuel Novoa III
 *
 * Return 0 for null expressions.  Pointed out by vodz.
 *
 * Merge in Aaron's comments previously posted to the busybox list,
 * modified slightly to take account of my changes to the code.
 *
 * TODO: May want to allow access to variables in the arith code.
 *       This would:
 *       1) allow us to evaluate $A as 0 if A isn't set (although this
 *          would require changes to ash.c too).
 *       2) allow us to write expressions as $(( A + 2 )).
 *       This could be done using a callback function passed to the
 *       arith() function of by requiring such a function with fixed
 *       name as an extern.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include "libbb.h"

/* 
 * Use "#if 1" below for Aaron's original test for whitespace.
 * Use "#if 0" for ctype's isspace().
 * */
#if 1
#undef isspace
#define isspace(arithval) \
	(arithval == ' ' || arithval == '\n' || arithval == '\t')
#endif

typedef char operator;

/* An operator's token id is a bit of a bitfield. The lower 5 bits are the
 * precedence, and high 3 are an ID unique accross operators of that
 * precedence. The ID portion is so that multiple operators can have the
 * same precedence, ensuring that the leftmost one is evaluated first.
 * Consider * and /. */

#define tok_decl(prec,id) (((id)<<5)|(prec))
#define PREC(op) ((op)&0x1F)

#define TOK_LPAREN tok_decl(0,0)

#define TOK_OR tok_decl(1,0)

#define TOK_AND tok_decl(2,0)

#define TOK_BOR tok_decl(3,0)

#define TOK_BXOR tok_decl(4,0)

#define TOK_BAND tok_decl(5,0)

#define TOK_EQ tok_decl(6,0)
#define TOK_NE tok_decl(6,1)

#define TOK_LT tok_decl(7,0)
#define TOK_GT tok_decl(7,1)
#define TOK_GE tok_decl(7,2)
#define TOK_LE tok_decl(7,3)

#define TOK_LSHIFT tok_decl(8,0)
#define TOK_RSHIFT tok_decl(8,1)

#define TOK_ADD tok_decl(9,0)
#define TOK_SUB tok_decl(9,1)

#define TOK_MUL tok_decl(10,0)
#define TOK_DIV tok_decl(10,1)
#define TOK_REM tok_decl(10,2)

/* For now all unary operators have the same precedence, and that's used to
 * identify them as unary operators */
#define UNARYPREC 14
#define TOK_BNOT tok_decl(UNARYPREC,0)
#define TOK_NOT tok_decl(UNARYPREC,1)
#define TOK_UMINUS tok_decl(UNARYPREC,2)
#define TOK_UPLUS tok_decl(UNARYPREC,3)

#define TOK_NUM tok_decl(15,0)
#define TOK_RPAREN tok_decl(15,1)
#define TOK_ERROR  tok_decl(15,2)	/* just a place-holder really */

#define ARITH_APPLY(op) arith_apply(op, numstack, &numstackptr)
#define NUMPTR (*numstackptr)

/* "applying" a token means performing it on the top elements on the integer
 * stack. For a unary operator it will only change the top element, but a
 * binary operator will pop two arguments and push a result */
static short arith_apply(operator    op, long *numstack, long **numstackptr)
{
	long numptr_val;
	long *NUMPTR_M1;

	if (NUMPTR == numstack) goto err; /* There is no operator that can work
										 without arguments */
	NUMPTR_M1 = NUMPTR - 1;
	if (op == TOK_UMINUS)
		*NUMPTR_M1 *= -1;
	else if (op == TOK_NOT)
		*NUMPTR_M1 = !(*NUMPTR_M1);
	else if (op == TOK_BNOT)
		*NUMPTR_M1 = ~(*NUMPTR_M1);
	else if (op != TOK_UPLUS) {
		/* Binary operators */
	if (NUMPTR_M1 == numstack) goto err; /* ... and binary operators need two
										   arguments */
	numptr_val = *--NUMPTR;         /* ... and they pop one */
		NUMPTR_M1 = NUMPTR - 1;
	if (op == TOK_BOR)
			*NUMPTR_M1 |= numptr_val;
	else if (op == TOK_OR)
			*NUMPTR_M1 = numptr_val || *NUMPTR_M1;
	else if (op == TOK_BAND)
			*NUMPTR_M1 &= numptr_val;
	else if (op == TOK_AND)
			*NUMPTR_M1 = *NUMPTR_M1 && numptr_val;
	else if (op == TOK_EQ)
			*NUMPTR_M1 = (*NUMPTR_M1 == numptr_val);
	else if (op == TOK_NE)
			*NUMPTR_M1 = (*NUMPTR_M1 != numptr_val);
	else if (op == TOK_GE)
			*NUMPTR_M1 = (*NUMPTR_M1 >= numptr_val);
	else if (op == TOK_RSHIFT)
			*NUMPTR_M1 >>= numptr_val;
	else if (op == TOK_LSHIFT)
			*NUMPTR_M1 <<= numptr_val;
	else if (op == TOK_GT)
			*NUMPTR_M1 = (*NUMPTR_M1 > numptr_val);
	else if (op == TOK_LT)
			*NUMPTR_M1 = (*NUMPTR_M1 < numptr_val);
	else if (op == TOK_LE)
			*NUMPTR_M1 = (*NUMPTR_M1 <= numptr_val);
	else if (op == TOK_MUL)
			*NUMPTR_M1 *= numptr_val;
	else if (op == TOK_ADD)
			*NUMPTR_M1 += numptr_val;
	else if (op == TOK_SUB)
			*NUMPTR_M1 -= numptr_val;
	else if(numptr_val==0)          /* zero divisor check */
			return -2;
	else if (op == TOK_DIV)
			*NUMPTR_M1 /= numptr_val;
	else if (op == TOK_REM)
			*NUMPTR_M1 %= numptr_val;
		/* WARNING!!!  WARNING!!!  WARNING!!! */
		/* Any new operators should be added BEFORE the zero divisor check! */
	}
	return 0;
err: return(-1);
}

static const char endexpression[] = ")";

/* + and - (in that order) must be last */
static const char op_char[] = "!<>=|&*/%~()+-";
static const char op_token[] = {
	/* paired with equal */
	TOK_NE, TOK_LE, TOK_GE,
	/* paired with self -- note: ! is special-cased below*/
	TOK_ERROR, TOK_LSHIFT, TOK_RSHIFT, TOK_EQ, TOK_OR, TOK_AND,
	/* singles */
	TOK_NOT, TOK_LT, TOK_GT, TOK_ERROR, TOK_BOR, TOK_BAND,
	TOK_MUL, TOK_DIV, TOK_REM, TOK_BNOT, TOK_LPAREN, TOK_RPAREN,
	TOK_ADD, TOK_SUB, TOK_UPLUS, TOK_UMINUS
};

#define NUM_PAIR_EQUAL  3
#define NUM_PAIR_SAME   6

extern long arith (const char *expr, int *errcode)
{
	register char arithval;	/* Current character under analysis */
	operator    lasttok, op;
	unsigned char prec;

	const char *p = endexpression;

	size_t datasizes = strlen(expr) + 2;

	/* Stack of integers */
	/* The proof that there can be no more than strlen(startbuf)/2+1 integers
	 * in any given correct or incorrect expression is left as an excersize to
	 * the reader. */
	long *numstack = alloca(((datasizes)/2)*sizeof(long)),
		*numstackptr = numstack;
	/* Stack of operator tokens */
	operator *stack = alloca((datasizes) * sizeof(operator)),
		*stackptr = stack;

	*numstack = 0;
	*stackptr++ = lasttok = TOK_LPAREN;     /* start off with a left paren */

  loop:
	if ((arithval = *expr) == 0) {
		if (p == endexpression) { /* Null expression. */
			*errcode = 0;
			return *numstack;
		}

		/* This is only reached after all tokens have been extracted from the
		 * input stream. If there are still tokens on the operator stack, they
		 * are to be applied in order. At the end, there should be a final
		 * result on the integer stack */

		if (expr != endexpression + 1) {	/* If we haven't done so already, */
			expr = endexpression;	/* append a closing right paren */
			goto loop;	/* and let the loop process it. */
		}
		/* At this point, we're done with the expression. */
		if (numstackptr != numstack+1) {/* ... but if there isn't, it's bad */
		  err:
			return (*errcode = -1);
			/* NOTREACHED */
		}
		return *numstack;
	} else {
		/* Continue processing the expression.  */
		if (isspace(arithval)) {
			goto prologue;          /* Skip whitespace */
		}
		if ((unsigned)arithval-'0' <= 9) /* isdigit */ {
			*numstackptr++ = strtol(expr, (char **) &expr, 10);
			lasttok = TOK_NUM;
			goto loop;
		}
#if 1
		if ((p = strchr(op_char, arithval)) == NULL) {
			goto err;
		}
#else
	    for ( p=op_char ; *p != arithval ; p++ ) {
			if (!*p) {
				goto err;
			}
		}
#endif
		p = op_token + (int)(p - op_char);
		++expr;
		if ((p >= op_token + NUM_PAIR_EQUAL) || (*expr != '=')) {
			p += NUM_PAIR_EQUAL;
			if ((p >= op_token + NUM_PAIR_SAME + NUM_PAIR_EQUAL)
				|| (*expr != arithval) || (arithval == '!')) {
				--expr;
				if (arithval == '=') { /* single = */
					goto err;
				}
				p += NUM_PAIR_SAME;
				/* Plus and minus are binary (not unary) _only_ if the last
				 * token was as number, or a right paren (which pretends to be
				 * a number, since it evaluates to one). Think about it.
				 * It makes sense. */
				if ((lasttok != TOK_NUM)
					&& (p >= op_token + NUM_PAIR_SAME + NUM_PAIR_EQUAL
						+ sizeof(op_char) - 2)) {
					p += 2; /* Unary plus or minus */
				}
			}
		}
		op = *p;

		/* We don't want a unary operator to cause recursive descent on the
		 * stack, because there can be many in a row and it could cause an
		 * operator to be evaluated before its argument is pushed onto the
		 * integer stack. */
		/* But for binary operators, "apply" everything on the operator
		 * stack until we find an operator with a lesser priority than the
		 * one we have just extracted. */
		/* Left paren is given the lowest priority so it will never be
		 * "applied" in this way */
		prec = PREC(op);
		if ((prec > 0) && (prec != UNARYPREC)) { /* not left paren or unary */
			if (lasttok != TOK_NUM) { /* binary op must be preceded by a num */
				goto err;
			}
			while (stackptr != stack) {
				if (op == TOK_RPAREN) {
					/* The algorithm employed here is simple: while we don't
					 * hit an open paren nor the bottom of the stack, pop
					 * tokens and apply them */
					if (stackptr[-1] == TOK_LPAREN) {
						--stackptr;
						lasttok = TOK_NUM; /* Any operator directly after a */
						/* close paren should consider itself binary */
						goto prologue;
					}
				} else if (PREC(stackptr[-1]) < prec) {
					break;
				}
				*errcode = ARITH_APPLY(*--stackptr);
				if(*errcode) return *errcode;
			}
			if (op == TOK_RPAREN) {
				goto err;
			}
		}

		/* Push this operator to the stack and remember it. */
		*stackptr++ = lasttok = op;

	  prologue:
		++expr;
		goto loop;
	}
}
