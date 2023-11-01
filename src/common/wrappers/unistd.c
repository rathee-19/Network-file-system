#include <unistd.h>
#include "../utilities.h"

int close_tx(int sockfd)
{
  int ret = close(sockfd);
  if (ret < 0)
    perror_tx("close");
  return ret;
}
