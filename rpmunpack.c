/*
 * rpmunpack for busybox
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
 
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h" 

/*
 * Some general definitions
 */

#define RPM_MAGIC	"\355\253\356\333"
#define GZ_MAGIC_1	'\037'
#define GZ_MAGIC_2	'\213'

/*
 * Global variables
 */
static char *progname;
static int infile, outfile;

/*
 * Read a specified number of bytes from input file
 */
static void myread(int num, char *buffer)
{
  int err;

  if ((err = read(infile, buffer, num)) != num) {
	if (err < 0)
		perror_msg_and_die(progname);
	else
		error_msg_and_die("Unexpected end of input file!");
  }
}

/*
 * Main program
 */
int rpmunpack_main(int argc, char **argv)
{
  int len, status = 0;
  RESERVE_BB_BUFFER(buffer, BUFSIZ);

  /* Get our own program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
	progname = argv[0];
  else
	progname++;

  /* Check for command line parameters */
	if (argc>=2 && *argv[1]=='-') {
           show_usage();
	}

  /* Open input file */
  if (argc == 1)
	infile = STDIN_FILENO;
  else if ((infile = open(argv[1], O_RDONLY)) < 0)
	perror_msg_and_die("%s", argv[1]);

  /* Read magic ID and output filename */
  myread(4, buffer);
  if (strncmp(buffer, RPM_MAGIC, 4)) {
	fprintf(stderr, "Input file is not in RPM format!\n");
	exit(1);
  }
  myread(6, buffer);		/* Skip flags */
  myread(64, buffer);
  buffer[64] = '\0';

  /* Open output file */
  strcat(buffer, ".cpio.gz");
  if (infile == STDIN_FILENO)
	outfile = STDOUT_FILENO;
  else if ((outfile = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
	  perror_msg_and_die("%s", buffer);

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
  myread(0x200 - 74, buffer);
  while (status < 2) {
	myread(1, buffer);
	if (status == 0 && buffer[0] == GZ_MAGIC_1)
		status++;
	else if (status == 1 && buffer[0] == GZ_MAGIC_2)
		status++;
	else
		status = 0;
  }
  buffer[0] = GZ_MAGIC_1;
  buffer[1] = GZ_MAGIC_2;
  if (write(outfile, buffer, 2) < 0)
	perror_msg_and_die("write");

  /* Now simply copy the GZIP archive into the output file */
  while ((len = read(infile, buffer, BUFSIZ)) > 0) {
	if (write(outfile, buffer, len) < 0)
		perror_msg_and_die("write");
  }
  if (len < 0)
    perror_msg_and_die("read");
  return EXIT_SUCCESS;
}
