#ifndef __CACHE_H
#define __CACHE_H

#include "defs.h"
#include <stdlib.h>
#include <pthread.h>

#define CACHE_SIZE 32

typedef struct __cache {
  fnode_t* files[CACHE_SIZE];
  int idle[CACHE_SIZE];
  pthread_mutex_t lock;
} cache_t;

void cache_init(cache_t* C);
void cache_insert(cache_t* C, fnode_t* node);
fnode_t* cache_search(cache_t* C, char* path);

#endif
