#include <syslog.h>

int do_log(char* msg, int delay)
{
  openlog("testlog", LOG_PID, LOG_DAEMON);
  while(1) {
    syslog(LOG_ERR, "%s: testing one, two, three\n", msg);
    sleep(delay);
  }
  closelog();
  return(0);
};

int main(void)
{
  if (fork()==0)
    do_log("A", 2);
  do_log("B", 3);
}
