#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"

extern FILE *gz_open(FILE *compressed_file, int *pid)
{
	int unzip_pipe[2];

	if (pipe(unzip_pipe)!=0) {
		error_msg("pipe error");
		return(NULL);
	}
	if ((*pid = fork()) == -1) {
		error_msg("fork failed");
		return(NULL);
	}
	if (*pid==0) {
		/* child process */
		close(unzip_pipe[0]);
		unzip(compressed_file, fdopen(unzip_pipe[1], "w"));
		fflush(NULL);
		fclose(compressed_file);
		close(unzip_pipe[1]);
		exit(EXIT_SUCCESS);
	}
	close(unzip_pipe[1]);
	if (unzip_pipe[0] == -1) {
		error_msg("gzip stream init failed");
	}
	return(fdopen(unzip_pipe[0], "r"));
}
