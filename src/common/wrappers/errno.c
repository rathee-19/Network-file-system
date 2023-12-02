#include <errno.h>
#include <stdlib.h>
#include "../colors.h"
#include "../utilities.h"

extern logfile_t* logfile;

void perror_t(const char* s)
{
  pthread_mutex_lock_tx(&(logfile->lock));
  timestamp(stderr);
  fprintf(stderr, RED);
  perror(s);
  fprintf(stderr, RESET);
  fflush(stderr);
  pthread_mutex_unlock_tx(&(logfile->lock));
}

void perror_tx(const char* s)
{
  perror_t(s);
  exit(1);
}

void perror_tpx(request_t* req, const char* s)
{
  perror_t(s);
  reqfree(req);
  pthread_exit(NULL);
}
