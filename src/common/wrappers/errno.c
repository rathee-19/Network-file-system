#include <errno.h>
#include <stdlib.h>
#include "../utilities.h"

void perror_t(const char* s)
{
  timestamp(stderr);
  perror(s);
}

void perror_tx(const char* s)
{
  timestamp(stderr);
  perror(s);
  exit(1);
}
