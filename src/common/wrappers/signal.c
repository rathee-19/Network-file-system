#include <signal.h>
#include "../utilities.h"

sighandler_t signal_tx(int signum, sighandler_t handler)
{
  sighandler_t ret = signal(signum, handler);
  if (ret == SIG_ERR)
    perror_tx("signal");
  return ret;
}
