#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"

extern int gz_open(FILE *compressed_file, int *pid)
{
	int unzip_pipe[2];

	if (pipe(unzip_pipe)!=0) {
		error_msg("pipe error");
		return(EXIT_FAILURE);
	}
	if ((*pid = fork()) == -1) {
		error_msg("fork failured");
		return(EXIT_FAILURE);
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
	return(unzip_pipe[0]);
}