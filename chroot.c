#include "internal.h"
#include <stdio.h>
#include <unistd.h>


const char chroot_usage[] = "chroot directory [command]\n"
  "Run a command with special root directory.\n";

extern int
chroot_main (struct FileInfo *i, int argc, char **argv)
{
  char *prog;

  if (chroot (argv[1]))
    {
      name_and_error ("cannot chroot to that directory");
      return 1;
    }
  if (argc > 2)
    {
      execvp (argv[2], argv + 2);
    }
  else
    {
      prog = getenv ("SHELL");
      if (!prog)
	prog = "/bin/sh";
      execlp (prog, prog, NULL);
    }
  name_and_error ("cannot exec");
  return 1;
}
