#include "defs.h"
#include "queue.h"

void queue_init(queue_t* Q)
{
  Q->head = (qnode_t*) calloc(1, sizeof(qnode_t));
  Q->tail = Q->head;
  pthread_mutex_init_tx(&Q->lock, NULL);
}

void queue_insert(queue_t*  Q, fnode_t* fn)
{
  qnode_t* x = (qnode_t*) calloc(1, sizeof(qnode_t));
  x->fn = fn;

  pthread_mutex_lock_tx(&Q->lock);
  Q->tail->next = x;
  Q->tail = x;
  pthread_mutex_unlock_tx(&Q->lock);
}

fnode_t* queue_pop(queue_t *Q)
{
  if (Q->head == Q->tail)
    return NULL;

  pthread_mutex_lock_tx(&Q->lock);
  qnode_t* node = Q->head->next;
  Q->head->next = node->next;
  if (node == Q->tail)
    Q->tail = Q->head;
  pthread_mutex_unlock_tx(&Q->lock);

  return node->fn;
}
