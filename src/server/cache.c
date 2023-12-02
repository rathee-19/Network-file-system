#include "defs.h"
#include "cache.h"

void cache_init(cache_t* C)
{
  for (int i = 0; i < CACHE_SIZE; i++)
    C->files[i] = NULL;
  pthread_mutex_init_tx(&(C->lock), NULL);
}

void cache_insert(cache_t* C, fnode_t* node)
{
  int max = 0;
  
  pthread_mutex_lock_tx(&(C->lock));
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (C->files[i] == NULL) {
      max = i;
      break;
    }
    if (C->idle[i] > C->idle[max])
      max = i;
  }

  C->files[max] = node;
  C->idle[max] = 0;
  pthread_mutex_unlock_tx(&(C->lock));
}

fnode_t* cache_search(cache_t* C, char* path)
{
  int i;
  fnode_t* node = NULL;

  pthread_mutex_lock_tx(&(C->lock));
  for (i = 0; i < CACHE_SIZE; i++) {
    if (C->files[i] != NULL) {
      if (strcmp(C->files[i]->file.path, path) == 0) {
        C->idle[i] = 0;
        node = C->files[i];
        goto ret_search;
      }
      C->idle[i]++;
    }
  }

ret_search:
  for (; i < CACHE_SIZE; i++)
    if (C->files[i] != NULL)
      C->idle[i]++;
  pthread_mutex_unlock_tx(&(C->lock));
  return node;
}
