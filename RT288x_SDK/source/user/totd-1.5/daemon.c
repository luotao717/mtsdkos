/* 	$Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/totd-1.5/daemon.c#1 $	*/

#include "totd.h"

#if !HAVE_DAEMON
int daemon(int nochdir, int noclose) {
  int i, fd;

  i = fork();
  if(i < 0)
    return (-1);
  if(i > 0)
    exit(0);

  if(!nochdir)
    chdir("/");

  if(!noclose) {
    fd = open("/dev/null", O_RDWR);
    close(0);
    close(1);
    close(2);
    if(fd > 0) {
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
    }
  }

  return (0);
}
#endif
