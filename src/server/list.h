#ifndef __LIST_H
#define __LIST_H

#include "defs.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct __snode {
  storage_t st;
  int down;
  struct __snode* prev;
  struct __snode* next;
} snode_t;

typedef struct __list {
  snode_t* head;
  snode_t* last;
  int num;
  pthread_mutex_t lock;
} list_t;

void list_init(list_t* L);
snode_t* list_insert(list_t* L, storage_t st);
void list_delete(list_t* L, snode_t* x);
snode_t* list_search(list_t* L, storage_t st);
snode_t* list_random(list_t* L);
int stequal(storage_t a, storage_t b);

#endif
