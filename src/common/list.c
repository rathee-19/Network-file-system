#include "list.h"

void init_list(list_t* L)
{
  L->head = (snode_t*) malloc(sizeof(snode_t));
  L->head->st.ip[0] = 0;
  L->head->st.nsport = -1;                                  // TODO: what is an invalid port in libc
  L->head->st.clport = -1;
  L->head->st.stport = -1;
  L->head->prev = L->head;
  L->head->next = L->head;
  pthread_mutex_init(&L->lock, NULL);
}

void insert(list_t *L, storage_t st)
{
  snode_t* x = (snode_t*) malloc(sizeof(snode_t));
  x->st = st;

  pthread_mutex_lock(&L->lock);
  L->head->prev->next = x;
  x->prev = L->head->prev;
  L->head->prev = x;
  x->next = L->head;
  pthread_mutex_unlock(&L->lock);
}

void delete(list_t *L, snode_t *x)
{
  if (L->head == x)
    return;

  pthread_mutex_lock(&L->lock);
  x->prev->next = x->next;
  x->next->prev = x->prev;
  pthread_mutex_unlock(&L->lock);

  free(x);
}

int get_number(list_t* L) {
  int num_elements=0;

  snode_t* node=L->head->next;

  do {
    num_elements++;
    node = node->next;
  } while(node != L->head);

  return num_elements;
}

int search(list_t* L, storage_t* st) {
  int result=0;

  snode_t* node=L->head->next;

  while(node!=L->head) {
    if(node->st.nsport!=st->nsport) {
      result=1;
      break;
    }
    node-node->next;
  }

  return result;
}
