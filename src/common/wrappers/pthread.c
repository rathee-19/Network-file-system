#include <pthread.h>
#include "../utilities.h"

void pthread_create_tx(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg)
{
  if (pthread_create(thread, attr, start_routine, arg) != 0)
    perror_tx("pthread");
}
