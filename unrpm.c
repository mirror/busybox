/*
 * Mini unrpm implementation for busybox
 *
 * rpmunpack.c  -  Utility program to unpack an RPM archive
 *
 * Gero Kuhlmann <gero@gkminix.han.de> 1998
 *
 *  This program is public domain software; you can do whatever you like
 *  with this source, including modifying and redistributing it.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
 
#include "internal.h" 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/*
 * Some general definitions
 */
#define BUFSIZE		512
#define RPM_MAGIC	"\355\253\356\333"
#define GZ_MAGIC_1	'\037'
#define GZ_MAGIC_2	'\213'

/*
 * Global variables
 */
static char buffer[BUFSIZE];
static char *progname;
static int infile, outfile;

/*
 * Read a specified number of bytes from input file
 */
static void myread(int num)
{
  int err;

  if ((err = read(infile, buffer, num)) != num) {
	if (err < 0)
		perror(progname);
	else
		fprintf(stderr, "unexpected end of input file\n");
	exit(1);
  }
}

/*
 * Main program
 */
int unrpm_main(int argc, char **argv)
{
  int len, status = 0;

  /* Get our own program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
	progname = argv[0];
  else
	progname++;

  /* Check for command line parameters */
	if (argc>=2 && *argv[1]=='-') {
           usage(unrpm_usage);
	}

  /* Open input file */
  if (argc == 1)
	infile = STDIN_FILENO;
  else if ((infile = open(argv[1], O_RDONLY)) < 0) {
	perror(progname);
	exit(1);
  }

  /* Read magic ID and output filename */
  myread(4);
  if (strncmp(buffer, RPM_MAGIC, 4)) {
	fprintf(stderr, "input file is not in RPM format\n");
	exit(1);
  }
  myread(6);		/* Skip flags */
  myread(64);
  buffer[64] = '\0';

  /* Open output file */
  strcat(buffer, ".cpio.gz");
  if (infile == STDIN_FILENO)
	outfile = STDOUT_FILENO;
  else if ((outfile = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
	perror(progname);
	exit(1);
  }

  /*
   * Now search for the GZIP signature. This is rather awkward, but I don't
   * know any other way how to find out the exact starting position of the
   * archive within the input file. There are a couple of data structures
   * and texts (obviously descriptions, installation shell scripts etc.)
   * coming before the archive, but even they start at different offsets
   * with different RPM files. However, it looks like the GZIP signature
   * never appears before offset 0x200, so we skip these first couple of
   * bytes to make the signature scan a little more reliable.
   */
  myread(0x200 - 74);
  while (status < 2) {
	myread(1);
	if (status == 0 && buffer[0] == GZ_MAGIC_1)
		status++;
	else if (status == 1 && buffer[0] == GZ_MAGIC_2)
		status++;
	else
		status = 0;
  }
  buffer[0] = GZ_MAGIC_1;
  buffer[1] = GZ_MAGIC_2;
  if (write(outfile, buffer, 2) < 0) {
	perror(progname);
	exit(1);
  }

  /* Now simply copy the GZIP archive into the output file */
  while ((len = read(infile, buffer, BUFSIZE)) > 0) {
	if (write(outfile, buffer, len) < 0) {
		perror(progname);
		exit(1);
	}
  }
  if (len < 0) {
	perror(progname);
	exit(1);
  }
  close(outfile);
  close(infile);
  exit(0);
}
