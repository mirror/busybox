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
 * are supported. This code is threadsafe. */

/* To use the routine, call it with an expression string. It returns an
 * integer result. You will also need to define an "error" function
 * that takes printf arguments and _does not return_, or modify the code
 * to use another error mechanism. */

#include <stdlib.h>
#include <string.h>
#include "libbb.h"

typedef char operator;

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

#define UNARYPREC 14
#define TOK_BNOT tok_decl(UNARYPREC,0)
#define TOK_NOT tok_decl(UNARYPREC,1)
#define TOK_UMINUS tok_decl(UNARYPREC,2)

#define TOK_NUM tok_decl(15,0)

#define ARITH_APPLY(op) arith_apply(op, numstack, &numstackptr)
#define NUMPTR (*numstackptr)
static short arith_apply(operator op, long *numstack, long **numstackptr)
{
	if (NUMPTR == numstack) goto err;
	if (op == TOK_UMINUS)
		NUMPTR[-1] *= -1;
	else if (op == TOK_NOT)
		NUMPTR[-1] = !(NUMPTR[-1]);
	else if (op == TOK_BNOT)
		NUMPTR[-1] = ~(NUMPTR[-1]);

	/* Binary operators */
	else {
	if (NUMPTR-1 == numstack) goto err;
	--NUMPTR;
	if (op == TOK_BOR)
		NUMPTR[-1] |= *NUMPTR;
	else if (op == TOK_OR)
		NUMPTR[-1] = *NUMPTR || NUMPTR[-1];
	else if (op == TOK_BAND)
		NUMPTR[-1] &= *NUMPTR;
	else if (op == TOK_AND)
		NUMPTR[-1] = NUMPTR[-1] && *NUMPTR;
	else if (op == TOK_EQ)
		NUMPTR[-1] = (NUMPTR[-1] == *NUMPTR);
	else if (op == TOK_NE)
		NUMPTR[-1] = (NUMPTR[-1] != *NUMPTR);
	else if (op == TOK_GE)
		NUMPTR[-1] = (NUMPTR[-1] >= *NUMPTR);
	else if (op == TOK_RSHIFT)
		NUMPTR[-1] >>= *NUMPTR;
	else if (op == TOK_LSHIFT)
		NUMPTR[-1] <<= *NUMPTR;
	else if (op == TOK_GT)
		NUMPTR[-1] = (NUMPTR[-1] > *NUMPTR);
	else if (op == TOK_LT)
		NUMPTR[-1] = (NUMPTR[-1] < *NUMPTR);
	else if (op == TOK_LE)
		NUMPTR[-1] = (NUMPTR[-1] <= *NUMPTR);
	else if (op == TOK_MUL)
		NUMPTR[-1] *= *NUMPTR;
	else if (op == TOK_DIV) {
		if(*NUMPTR==0)
			return -2;
		NUMPTR[-1] /= *NUMPTR;
		}
	else if (op == TOK_REM) {
		if(*NUMPTR==0)
			return -2;
		NUMPTR[-1] %= *NUMPTR;
		}
	else if (op == TOK_ADD)
		NUMPTR[-1] += *NUMPTR;
	else if (op == TOK_SUB)
		NUMPTR[-1] -= *NUMPTR;
	}
	return 0;
err: return(-1);
}

extern long arith (const char *startbuf, int *errcode)
{
	register char arithval;
	const char *expr = startbuf;

	operator lasttok = TOK_MUL, op;
	size_t datasizes = strlen(startbuf);
	unsigned char prec;

	long *numstack, *numstackptr;
	operator *stack = alloca(datasizes * sizeof(operator)), *stackptr = stack;

	*errcode = 0;
	numstack = alloca((datasizes/2+1)*sizeof(long)), numstackptr = numstack;

	while ((arithval = *expr)) {
		if (arithval == ' ' || arithval == '\n' || arithval == '\t')
			goto prologue;
		if ((unsigned)arithval-'0' <= 9) /* isdigit */ {
			*numstackptr++ = strtol(expr, (char **) &expr, 10);
			lasttok = TOK_NUM;
			continue;
		} if (arithval == '(') {
			*stackptr++ = TOK_LPAREN;
			lasttok = TOK_LPAREN;
			goto prologue;
		} if (arithval == ')') {
			lasttok = TOK_NUM;
			while (stackptr != stack) {
				op = *--stackptr;
				if (op == TOK_LPAREN)
					goto prologue;
				*errcode = ARITH_APPLY(op);
				if(*errcode) return *errcode;
			}
			goto err; /* Mismatched parens */
		} if (arithval == '|') {
			if (*++expr == '|')
				op = TOK_OR;
			else {
				--expr;
				op = TOK_BOR;
			}
		} else if (arithval == '&') {
			if (*++expr == '&')
				op = TOK_AND;
			else {
				--expr;
				op = TOK_BAND;
			}
		} else if (arithval == '=') {
			if (*++expr != '=') goto err; /* Unknown token */
			op = TOK_EQ;
		} else if (arithval == '!') {
			if (*++expr == '=')
				op = TOK_NE;
			else {
				--expr;
				op = TOK_NOT;
			}
		} else if (arithval == '>') {
			switch (*++expr) {
				case '=':
					op = TOK_GE;
					break;
				case '>':
					op = TOK_RSHIFT;
					break;
				default:
					--expr;
					op = TOK_GT;
			}
		} else if (arithval == '<') {
			switch (*++expr) {
				case '=':
					op = TOK_LE;
					break;
				case '<':
					op = TOK_LSHIFT;
					break;
				default:
					--expr;
					op = TOK_LT;
			}
		} else if (arithval == '*')
			op = TOK_MUL;
		else if (arithval == '/')
			op = TOK_DIV;
		else if (arithval == '%')
			op = TOK_REM;
		else if (arithval == '+') {
			if (lasttok != TOK_NUM) goto prologue; /* Unary plus */
			op = TOK_ADD;
		} else if (arithval == '-')
			op = (lasttok == TOK_NUM) ? TOK_SUB : TOK_UMINUS;
		else if (arithval == '~')
			op = TOK_BNOT;
		else goto err; /* Unknown token */

		prec = PREC(op);
		if (prec != UNARYPREC)
			while (stackptr != stack && PREC(stackptr[-1]) >= prec) {
				*errcode = ARITH_APPLY(*--stackptr);
				if(*errcode) return *errcode;
			}
		*stackptr++ = op;
		lasttok = op;
prologue: ++expr;
	} /* yay */

	while (stackptr != stack) {
		*errcode = ARITH_APPLY(*--stackptr);
		if(*errcode) return *errcode;
	}
	if (numstackptr != numstack+1) {
err: 
	    *errcode = -1;
	    return -1;
	 /* NOTREACHED */
	}

	return *numstack;
}
