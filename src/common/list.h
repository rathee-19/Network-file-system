#include "api.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct __snode {
  storage_t st;
  struct __snode* prev;
  struct __snode* next;
} snode_t;

typedef struct __list {
  snode_t *head;
  pthread_mutex_t lock;
} list_t;

void init_list(list_t* L);
void insert(list_t *L, storage_t st);
void delete(list_t *L, snode_t *x);
