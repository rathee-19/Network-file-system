#include <pthread.h>
#include "../utilities.h"

int pthread_create_tx(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg)
{
  int ret = pthread_create(thread, attr, start_routine, arg);
  if (ret != 0)
    perror_tx("pthread");
  return ret;
}

int pthread_mutex_init_tx(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr)
{
  int ret = pthread_mutex_init(mutex, attr);
  if (ret != 0)
    perror_tx("pthread_mutex_init");
  return ret;
}

int pthread_mutex_lock_tx(pthread_mutex_t* mutex)
{
  int ret = pthread_mutex_lock(mutex);
  if (ret != 0)
    perror_tx("pthread_mutex_lock");
  return ret;
}

int pthread_mutex_unlock_tx(pthread_mutex_t* mutex)
{
  int ret = pthread_mutex_unlock(mutex);
  if (ret != 0)
    perror_tx("pthread_mutex_unlock");
  return ret;
}
