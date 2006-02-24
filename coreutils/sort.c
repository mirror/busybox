/* vi: set sw=4 ts=4: */
/*
 * SuS3 compliant sort implementation for busybox
 *
 * Copyright (C) 2004 by Rob Landley <rob@landley.net>
 *
 * MAINTAINER: Rob Landley <rob@landley.net>
 * 
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * See SuS3 sort standard at:
 * http://www.opengroup.org/onlinepubs/007904975/utilities/sort.html
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "busybox.h"

static int global_flags;

/*
	sort [-m][-o output][-bdfinru][-t char][-k keydef]... [file...]
	sort -c [-bdfinru][-t char][-k keydef][file]
*/

/* These are sort types */
#define FLAG_n			1		/* Numeric sort */
#define FLAG_g			2		/* Sort using strtod() */
#define FLAG_M			4		/* Sort date */
/* ucsz apply to root level only, not keys.  b at root level implies bb */
#define FLAG_u			8		/* Unique */
#define FLAG_c			16		/* Check: no output, exit(!ordered) */
#define FLAG_s			32		/* Stable sort, no ascii fallback at end */
#define FLAG_z			64		/* Input is null terminated, not \n */
/* These can be applied to search keys, the previous four can't */
#define FLAG_b			128		/* Ignore leading blanks */
#define FLAG_r			256		/* Reverse */
#define FLAG_d			512		/* Ignore !(isalnum()|isspace()) */
#define FLAG_f			1024	/* Force uppercase */
#define FLAG_i			2048	/* Ignore !isprint() */
#define FLAG_bb			32768	/* Ignore trailing blanks  */


#ifdef CONFIG_FEATURE_SORT_BIG
static char key_separator;

static struct sort_key
{
	struct sort_key *next_key;	/* linked list */
	unsigned short range[4];	/* start word, start char, end word, end char */
	int flags;
} *key_list;

static char *get_key(char *str, struct sort_key *key, int flags)
{
	int start=0,end,len,i,j;

	/* Special case whole string, so we don't have to make a copy */
	if(key->range[0]==1 && !key->range[1] && !key->range[2] && !key->range[3]
		&& !(flags&(FLAG_b&FLAG_d&FLAG_f&FLAG_i&FLAG_bb))) return str;
	/* Find start of key on first pass, end on second pass*/
	len=strlen(str);

	for(j=0;j<2;j++) {
		if(!key->range[2*j]) end=len;
		/* Loop through fields */
		else {
			end=0;
			for(i=1;i<key->range[2*j]+j;i++) {
				/* Skip leading blanks or first separator */
				if(str[end]) {
					if(key_separator) {
						if(str[end]==key_separator) end++;
					} else if(isspace(str[end]))
						while(isspace(str[end])) end++;
				}
				/* Skip body of key */
				for(;str[end];end++) {
					if(key_separator) {
						if(str[end]==key_separator) break;
					} else if(isspace(str[end])) break;
				}
			}
		}
		if(!j) start=end;
	}
	/* Key with explicit separator starts after separator */
	if(key_separator && str[start]==key_separator) start++;
	/* Strip leading whitespace if necessary */
	if(flags&FLAG_b) while(isspace(str[start])) start++;
	/* Strip trailing whitespace if necessary */
	if(flags&FLAG_bb) while(end>start && isspace(str[end-1])) end--;
	/* Handle offsets on start and end */
	if(key->range[3]) {
		end+=key->range[3]-1;
		if(end>len) end=len;
	}
	if(key->range[1]) {
		start+=key->range[1]-1;
		if(start>len) start=len;
	}
	/* Make the copy */
	if(end<start) end=start;
	str=bb_xstrndup(str+start,end-start);
	/* Handle -d */
	if(flags&FLAG_d) {
		for(start=end=0;str[end];end++)
			if(isspace(str[end]) || isalnum(str[end])) str[start++]=str[end];
		str[start]=0;
	}
	/* Handle -i */
	if(flags&FLAG_i) {
		for(start=end=0;str[end];end++)
			if(isprint(str[end])) str[start++]=str[end];
		str[start]=0;
	}
	/* Handle -f */
	if(flags*FLAG_f) for(i=0;str[i];i++) str[i]=toupper(str[i]);

	return str;
}

static struct sort_key *add_key(void)
{
	struct sort_key **pkey=&key_list;
	while(*pkey) pkey=&((*pkey)->next_key);
	return *pkey=xcalloc(1,sizeof(struct sort_key));
}

#define GET_LINE(fp) (global_flags&FLAG_z) ? bb_get_chunk_from_file(fp,NULL) \
										   : bb_get_chomped_line_from_file(fp)
#else
#define GET_LINE(fp)		bb_get_chomped_line_from_file(fp)
#endif

/* Iterate through keys list and perform comparisons */
static int compare_keys(const void *xarg, const void *yarg)
{
	int flags=global_flags,retval=0;
	char *x,*y;

#ifdef CONFIG_FEATURE_SORT_BIG
	struct sort_key *key;

	for(key=key_list;!retval && key;key=key->next_key) {
		flags=(key->flags) ? key->flags : global_flags;
		/* Chop out and modify key chunks, handling -dfib */
		x=get_key(*(char **)xarg,key,flags);
		y=get_key(*(char **)yarg,key,flags);
#else
	/* This curly bracket serves no purpose but to match the nesting
	   level of the for() loop we're not using */
	{
		x=*(char **)xarg;
		y=*(char **)yarg;
#endif
		/* Perform actual comparison */
		switch(flags&7) {
			default:
				bb_error_msg_and_die("Unknown sort type.");
				break;
			/* Ascii sort */
			case 0:
				retval=strcmp(x,y);
				break;
#ifdef CONFIG_FEATURE_SORT_BIG
			case FLAG_g:
			{
				char *xx,*yy;
				double dx=strtod(x,&xx), dy=strtod(y,&yy);
				/* not numbers < NaN < -infinity < numbers < +infinity) */
				if(x==xx) retval=(y==yy ? 0 : -1);
				else if(y==yy) retval=1;
				else if(isnan(dx)) retval=isnan(dy) ? 0 : -1;
				else if(isnan(dy)) retval=1;
				else if(isinf(dx)) {
					if(dx<0) retval=((isinf(dy) && dy<0) ? 0 : -1);
					else retval=((isinf(dy) && dy>0) ? 0 : 1);
				} else if(isinf(dy)) retval=dy<0 ? 1 : -1;
				else retval=dx>dy ? 1 : (dx<dy ? -1 : 0);
				break;
			}
			case FLAG_M:
			{
				struct tm thyme;
				int dx;
				char *xx,*yy;

				xx=strptime(x,"%b",&thyme);
				dx=thyme.tm_mon;
				yy=strptime(y,"%b",&thyme);
				if(!xx) retval=(!yy ? 0 : -1);
				else if(!yy) retval=1;
				else retval=(dx==thyme.tm_mon ? 0 : dx-thyme.tm_mon);
				break;
			}
			/* Full floating point version of -n */
			case FLAG_n:
			{
				double dx=atof(x),dy=atof(y);
				retval=dx>dy ? 1 : (dx<dy ? -1 : 0);
				break;
			}
		}
		/* Free key copies. */
		if(x!=*(char **)xarg) free(x);
		if(y!=*(char **)yarg) free(y);
		if(retval) break;
#else
			/* Integer version of -n for tiny systems */
			case FLAG_n:
				retval=atoi(x)-atoi(y);
				break;
		}
#endif
	}
	/* Perform fallback sort if necessary */
	if(!retval && !(global_flags&FLAG_s))
			retval=strcmp(*(char **)xarg, *(char **)yarg);
//dprintf(2,"reverse=%d\n",flags&FLAG_r);
	return ((flags&FLAG_r)?-1:1)*retval;
}

int sort_main(int argc, char **argv)
{
	FILE *fp,*outfile=NULL;
	int linecount=0,i,flag;
	char *line,**lines=NULL,*optlist="ngMucszbrdfimS:T:o:k:t:";
	int c;

	bb_default_error_retval = 2;
	/* Parse command line options */
	while((c=getopt(argc,argv,optlist))>0) {
		line=index(optlist,c);
		if(!line) bb_show_usage();
		switch(*line) {
#ifdef CONFIG_FEATURE_SORT_BIG
			case 'o':
				if(outfile) bb_error_msg_and_die("Too many -o.");
				outfile=bb_xfopen(optarg,"w");
				break;
			case 't':
				if(key_separator || optarg[1])
					bb_error_msg_and_die("Too many -t.");
				key_separator=*optarg;
				break;
			/* parse sort key */
			case 'k':
			{
				struct sort_key *key=add_key();
				char *temp, *temp2;

				temp=optarg;
				for(i=0;*temp;) {
					/* Start of range */
					key->range[2*i]=(unsigned short)strtol(temp,&temp,10);
					if(*temp=='.')
						key->range[(2*i)+1]=(unsigned short)strtol(temp+1,&temp,10);
					for(;*temp;temp++) {
						if(*temp==',' && !i++) {
							temp++;
							break;
						} /* no else needed: fall through to syntax error
							 because comma isn't in optlist */
						temp2=index(optlist,*temp);
						flag=(1<<(temp2-optlist));
						if(!temp2 || (flag>FLAG_M && flag<FLAG_b))
							bb_error_msg_and_die("Unknown key option.");
						/* b after , means strip _trailing_ space */
						if(i && flag==FLAG_b) flag=FLAG_bb;
						key->flags|=flag;
					}
				}
				break;
			}
#endif
			default:
				global_flags|=(1<<(line-optlist));
				/* global b strips leading and trailing spaces */
				if(global_flags&FLAG_b) global_flags|=FLAG_bb;
				break;
		}
	}
	/* Open input files and read data */
	for(i=argv[optind] ? optind : optind-1;argv[i];i++) {
		if(i<optind || (*argv[i]=='-' && !argv[i][1])) fp=stdin;
		else fp=bb_xfopen(argv[i],"r");
		for(;;) {
			line=GET_LINE(fp);
			if(!line) break;
			if(!(linecount&63))
				lines=xrealloc(lines, sizeof(char *)*(linecount+64));
			lines[linecount++]=line;
		}
		fclose(fp);
	}
#ifdef CONFIG_FEATURE_SORT_BIG
	/* if no key, perform alphabetic sort */
    if(!key_list) add_key()->range[0]=1;
	/* handle -c */
	if(global_flags&FLAG_c) {
		int j=(global_flags&FLAG_u) ? -1 : 0;
		for(i=1;i<linecount;i++)
			if(compare_keys(&lines[i-1],&lines[i])>j) {
				fprintf(stderr,"Check line %d\n",i);
				return 1;
			}
		return 0;
	}
#endif
	/* Perform the actual sort */
	qsort(lines,linecount,sizeof(char *),compare_keys);
	/* handle -u */
	if(global_flags&FLAG_u) {
		for(flag=0,i=1;i<linecount;i++) {
			if(!compare_keys(&lines[flag],&lines[i])) free(lines[i]);
			else lines[++flag]=lines[i];
		}
		if(linecount) linecount=flag+1;
	}
	/* Print it */
	if(!outfile) outfile=stdout;
	for(i=0;i<linecount;i++) fprintf(outfile,"%s\n",lines[i]);
	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
