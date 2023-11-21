#ifndef __QUEUE_H
#define __QUEUE_H

#include "defs.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct __qnode {
  fnode_t* fn;
  struct __qnode* next;
} qnode_t;

typedef struct __queue {
  qnode_t* head;
  qnode_t* tail;
  pthread_mutex_t lock;
} queue_t;

void queue_init(queue_t* Q);
void queue_insert(queue_t* Q, fnode_t* fn);
fnode_t* queue_pop(queue_t* Q);

#endif
