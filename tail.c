/* vi: set sw=4 ts=4: */
/* tail -- output the last part of file(s)
   Copyright (C) 89, 90, 91, 95, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Original version by Paul Rubin <phr@ocf.berkeley.edu>.
   Extensions by David MacKenzie <djm@gnu.ai.mit.edu>.
   tail -f for multiple files by Ian Lance Taylor <ian@airs.com>.  

   Rewrote the option parser, removed locales support,
   and generally busyboxed, Erik Andersen <andersen@lineo.com>

   Removed superfluous options and associated code ("-c", "-n", "-q").
   Removed "tail -f" support for multiple files.
   Both changes by Friedrich Vedder <fwv@myrtle.lahn.de>.

   Compleate Rewrite to correctly support "-NUM", "+NUM", and "-s" by
   E.Allen Soard (esp@espsw.net).

 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "internal.h"

#define STDIN "standard input"
#define LINES 0
#define BYTES 1

static int n_files = 0;
static char **files = NULL;

static char follow=0;

#ifdef BB_FEATURE_SIMPLE_TAIL
static const char unit_type=LINES;
static const char sleep_int=1;
#else
static char unit_type=LINES;
static int sleep_int=1;
static char verbose = 0;
#endif

//static off_t units=-11;
static off_t units=0;

int tail_stream(int file_id)
{
	int fd;
	ssize_t bytes_read=0;
	ssize_t bs=BUFSIZ;
	ssize_t startpoint=bs;
	ssize_t endpoint=0;
	ssize_t count=0;
	ssize_t filesize=0;
	ssize_t filelocation=0;
	char direction=1;
	char * buffer;
	char pipe;


	if (!strcmp(files[file_id], STDIN))
		fd = 0;
	else
		fd = open(files[file_id], O_RDONLY);
	if (fd == -1)
		fatalError("Unable to open file %s.\n", files[file_id]);

	buffer=malloc(bs);

	filesize=lseek(fd, -1, SEEK_END)+1;
	pipe=(filesize<=0);

	if(units>=0)
		lseek(fd,0,SEEK_SET);
	else {
		direction=-1;
		count=1;
	}
	while(units != 0) {
		if (pipe) {
			char * line;
			ssize_t f_size=0;

			bs=BUFSIZ;
			line=malloc(bs);
			while(1) {
				bytes_read=read(fd,line,bs);
				if(bytes_read<=0)
					break;
				buffer=realloc(buffer,f_size+bytes_read);
				memcpy(&buffer[f_size],line,bytes_read);
				filelocation=f_size+=bytes_read;
			}
			bs=f_size;
			if(direction<0)
				bs--;
			if (line)
				free(line);
		} else {
			filelocation = lseek(fd, 0, SEEK_CUR);
			if(direction<0) {
				if(filelocation<bs)
					bs=filelocation;
				filelocation = lseek(fd, -bs, SEEK_CUR);
			}
			bytes_read = read(fd, buffer, bs);
			if (bytes_read <= 0)
				break;
			bs=bytes_read;
		}
		startpoint=bs;
		if(direction>0) {
			endpoint=startpoint;
			startpoint=0;
		}
		for(;startpoint!=endpoint;startpoint+=direction) {
#ifndef BB_FEATURE_SIMPLE_TAIL
			if(unit_type==BYTES)
				count++;
			else
#endif
				if(buffer[startpoint-1]=='\n')
					count++;
			if (!pipe)
				filelocation=lseek(fd,0,SEEK_CUR);
			if(count==abs(units))
				break;
		}
		if((count==abs(units)) | pipe)
			break;
		if(direction<0){
			filelocation = lseek(fd, -bytes_read, SEEK_CUR);
			if(filelocation==0)
				break;
		}
	}
	if(pipe && (direction<0))
		bs++;
	bytes_read=bs-startpoint;
	memcpy(&buffer[0],&buffer[startpoint],bytes_read);

	bs=BUFSIZ;
	while (1) {
		if((filelocation>0 || pipe)){
			write(1,buffer,bytes_read);
		}
		bytes_read = read(fd, buffer, bs);
		filelocation+=bytes_read;
		if (bytes_read <= 0) {
			if (!follow) {
				close(fd);
				break;
			}
			sleep(sleep_int);
		}
		usleep(sleep_int * 1000);
	}
	if (buffer)
		free(buffer);
	return 0;
}

void add_file(char *name)
{
	++n_files;
	files = realloc(files, n_files);
	files[n_files - 1] = (char *) malloc(strlen(name) + 1);
	strcpy(files[n_files - 1], name);
}


int tail_main(int argc, char **argv)
{
	int show_headers = 1;
	int test;
	int c;
	int nargs=0;
	char **argn=NULL;

	opterr = 0;
	
	for(c=0;c<argc;c++){
		test=atoi(argv[c]);
		if(test){
			units=test;
			if(units<0)
				units=units-1;
		}else{
			nargs++;
			argn = realloc(argn, nargs);
			argn[nargs - 1] = (char *) malloc(strlen(argv[c]) + 1);
			strcpy(argn[nargs - 1], argv[c]);
		}
	}
	while (1) {
		int opt_index = 0;

		c = getopt_long_only(nargs, argn,
			"c:fhn:s:qv", NULL, &opt_index);
		if (c == -1)
			break;
		switch (c) {

#ifndef BB_FEATURE_SIMPLE_TAIL

		case 'c':
			unit_type = BYTES;
			test = atoi(optarg);
			if(test==0)
				usage(tail_usage);
			if(optarg[strlen(optarg)-1]>'9') {
				switch (optarg[strlen(optarg)-1]) {
				case 'b':
					test *= 512;
					break;
				case 'k':
					test *= 1024;
					break;
				case 'm':
					test *= (1024 * 1024);
					break;
				default:
					fprintf(stderr,"Size must be b,k, or m.");
					usage(tail_usage);
				}
			}
			if(optarg[0]=='+')
				units=test+1;
			else
				units=-(test+1);
			break;
		case 'q':
			show_headers = 0;
			break;
		case 's':
			sleep_int = atoi(optarg);
			if(sleep_int<1)
				sleep_int=1;
			break;
		case 'v':
			verbose = 1;
			break;
#endif
		case 'f':
			follow = 1;
			break;
		case 'h':
			usage(tail_usage);
			break;
		case 'n':
			test = atoi(optarg);
			if (test) {
				if (optarg[0] == '+')
					units = test;
				else
					units = -(test+1);
			} else
				usage(tail_usage);
			break;
		default:
			errorMsg("\nUnknown arg: %c.\n\n",c);
			usage(tail_usage);
		}
	}
	while (optind < nargs) {
		if (!strcmp(argn[optind], "-"))
			add_file(STDIN);
		else
			add_file(argn[optind]);
		optind++;
	}
	if(units==0)
		units=-11;
	if(units>0)
		units--;
	if (n_files == 0)
		add_file(STDIN);
	if (n_files == 1)
#ifndef BB_FEATURE_SIMPLE_TAIL
		if (!verbose)
#endif
			show_headers = 0;
	for (test = 0; test < n_files; test++) {
		if (show_headers)
			printf("==> %s <==\n", files[test]);
		tail_stream(test);
	}
	if(files)
		free(files);
	if(argn)
		free(argn);
	return 0;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
