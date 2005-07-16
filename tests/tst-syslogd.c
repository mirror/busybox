/*
 *  tst-syslogd.c - tests concurrent threads calling syslog
 *
 *  build with: gcc -Wall tst-syslogd.c -lpthread
 */

#include <stdio.h>
#include <pthread.h>
#include <syslog.h>
#include <unistd.h>

void *log_func(void *arg)
{
	int i;
	int thrid = (int)arg;

	openlog(NULL, LOG_PERROR | LOG_PID, LOG_USER);
	for (i = 0; i < 10; i++) {
		syslog(LOG_DEBUG, "thread %i iter %i\n", thrid, i);
		sleep(thrid); /* this mixes things up a bit */
	}
	closelog();

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t thr1, thr2, thr3;
	int id1 = 1;
	int id2 = 2;
	int id3 = 3;

	pthread_create(&thr1, NULL, log_func, (void *)id1);
	pthread_create(&thr2, NULL, log_func, (void *)id2);
	pthread_create(&thr3, NULL, log_func, (void *)id3);

	pthread_join(thr1, NULL);
	pthread_join(thr2, NULL);
	pthread_join(thr3, NULL);

	return 0;
}

