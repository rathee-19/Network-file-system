#ifndef __TRIE_H
#define __TRIE_H

#include "defs.h"
#include "list.h"
#include <stdlib.h>
#include <pthread.h>

#define CHARSET 256

enum t_ret {T_INVALID, T_EXISTS, T_NOTFOUND, T_LOCK, T_SUCCESS};

typedef struct __fnode {
  metadata_t file;
  snode_t* loc;
  snode_t* bkp1;
  snode_t* bkp2;
  snode_t* writer;
  int valid;
  int rd;
  int wr;
  struct __fnode* child[CHARSET];
  pthread_mutex_t lock;
} fnode_t;

typedef struct __trie {
  fnode_t* head;
  int num;
  pthread_mutex_t lock;
} trie_t;

fnode_t* trie_node(void);
void trie_init(trie_t* T);
enum t_ret trie_insert(trie_t* T, metadata_t* file, snode_t* loc);
enum t_ret trie_update(trie_t* T, metadata_t *file);
fnode_t* trie_search(trie_t* T, char* path);
metadata_t* preorder_traversal(trie_t* T, int* bytes);
void preorder_worker(fnode_t* head, metadata_t* data, int* idx);

void invalidate_file(trie_t* T, fnode_t *node);
void invalidate_dir(trie_t* T, fnode_t *node);
void invalidate_worker(trie_t* T, fnode_t *head);
void mark_rdonly(trie_t* T, snode_t* loc);
void mark_rdonly_worker(fnode_t* head, snode_t* loc);
void unmark_rdonly(trie_t* T, snode_t* loc);
void unmark_rdonly_worker(fnode_t* head, snode_t* loc);

snode_t* available_server(fnode_t* node);

/*
fnode_t* check_ghost_files(trie_t* T);
fnode_t* check_ghost_worker(fnode_t* head);
fnode_t* check_vulnerable_files(trie_t* T);
fnode_t* check_vulnerable_worker(fnode_t* head);
void trie_prune(trie_t* T);
int trie_prune_worker(trie_t* T, fnode_t* head);
*/

#endif
