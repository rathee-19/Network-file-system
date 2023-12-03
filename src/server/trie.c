#include "defs.h"
#include "trie.h"

extern queue_t qdel;
extern queue_t qrep;
extern logfile_t* logfile;

fnode_t* trie_node(void)
{
  fnode_t* node = (fnode_t*) calloc(1, sizeof(fnode_t));
  node->valid = 0;
  pthread_mutex_init_tx(&node->lock, NULL);
  return node;
}

void trie_init(trie_t* T)
{
  T->head = trie_node();
  strcpy(T->head->file.path, "");
  pthread_mutex_init_tx(&T->lock, NULL);
}

enum t_ret trie_insert(trie_t* T, metadata_t* file, snode_t* loc)
{
  int len = strlen(file->path);
  if (len == 0) {
    logns(PROGRESS, "Invalid file from %s:%d", loc->st.ip, loc->st.nsport);
    return T_INVALID;
  }

  if (len > 1 && file->path[len - 1] == '/') {
    file->path[len - 1] = 0;
    len--;
  }

  pthread_mutex_lock_tx(&T->lock);
  fnode_t* temp = T->head;
  for (int i = 0; i < len; i++) {
    int ch = (unsigned char) file->path[i];
    if (temp->child[ch] == 0) {
      temp->child[ch] = trie_node();
      strcpy(temp->child[ch]->file.path, temp->file.path);
      temp->child[ch]->file.path[i] = file->path[i];
      temp->child[ch]->file.path[i + 1] = 0;
    }
    temp = temp->child[ch];
  }

  if (temp->valid == 1) {
    pthread_mutex_unlock_tx(&T->lock);
    logns(PROGRESS, "File already exists: %s", file->path);
    return T_EXISTS;
  }
  
  temp->file = *file;
  temp->loc = loc;
  temp->valid = 1;
  T->num++;
  pthread_mutex_unlock_tx(&T->lock);
  logns(PROGRESS, "Added file: %s", temp->file.path);
  queue_insert(&qrep, temp);
  return T_SUCCESS;
}

enum t_ret trie_update(trie_t* T, metadata_t* file)
{
  int len = strlen(file->path);
  if (len == 0)
    return T_INVALID;

  if (file->path[len - 1] == '/') {
    file->path[len - 1] = 0;
    len--;
  }

  pthread_mutex_lock_tx(&T->lock);
  fnode_t* temp = T->head;
  for (int i = 0; i < len; i++) {
    int ch = (unsigned char) file->path[i];
    if (temp->child[ch] == 0) {
      temp->child[ch] = trie_node();
      strcpy(temp->child[ch]->file.path, temp->file.path);
      temp->child[ch]->file.path[i] = file->path[i];
      temp->child[ch]->file.path[i + 1] = 0;
    }
    temp = temp->child[ch];
  }

  if (temp->valid == 1) {
    pthread_mutex_unlock_tx(&T->lock);
    pthread_mutex_lock_tx(&(temp->lock));
    temp->file = *file;
    pthread_mutex_unlock_tx(&(temp->lock));
    logns(PROGRESS, "Updated file: %s", file->path);
    return T_SUCCESS;
  }
  
  pthread_mutex_unlock_tx(&T->lock);
  return T_NOTFOUND;
}

fnode_t* trie_search(trie_t* T, char* path)
{
  int len = strlen(path);
  if (len != 0 && path[len - 1] == '/') {
    path[len - 1] = 0;
    len--;
  }

  pthread_mutex_lock_tx(&T->lock);
  fnode_t* temp = T->head;
  for (int i = 0; i < strlen(path); i++) {
    int ch = (unsigned char) path[i];
    if (temp->child[ch] == 0) {
      pthread_mutex_unlock_tx(&T->lock);
      return 0;
    }
    temp = temp->child[ch];
  }

  if (temp->valid == 0) {
    pthread_mutex_unlock_tx(&T->lock);
    return 0;
  }
  
  pthread_mutex_unlock_tx(&T->lock);
  return temp;
}

metadata_t* preorder_traversal(trie_t* T, int* bytes)
{
  pthread_mutex_lock_tx(&T->lock);
  int idx = 0;
  *bytes = T->num * sizeof(metadata_t);
  metadata_t* data = (metadata_t*) calloc(T->num, sizeof(metadata_t));
  preorder_worker(T->head, data, &idx);
  pthread_mutex_unlock_tx(&T->lock);
  return data;
}

void preorder_worker(fnode_t* head, metadata_t* data, int* idx)
{
  if (head->valid) {
    metadata_t *info = (metadata_t*) ((void*) data + ((*idx) * sizeof(metadata_t)));
    memcpy(info, &(head->file), sizeof(metadata_t));
    (*idx)++;
  }
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      preorder_worker(head->child[i], data, idx);
}

void invalidate_file(trie_t* T, fnode_t *node)
{
  pthread_mutex_lock_tx(&T->lock);
  if (node->valid) {
    node->valid = 0;
    T->num--;
  }
  pthread_mutex_unlock_tx(&T->lock);
  queue_insert(&qdel, node);
}

void invalidate_dir(trie_t* T, fnode_t *node)
{
  pthread_mutex_lock_tx(&T->lock);
  if (node->valid)
    invalidate_worker(T, node);
  pthread_mutex_unlock_tx(&T->lock);
}

void invalidate_worker(trie_t* T, fnode_t *head)
{
  if (head->valid) {
    head->valid = 0;
    T->num--;
    queue_insert(&qdel, head);
  }
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      invalidate_worker(T, head->child[i]);
}

void mark_rdonly(trie_t* T, snode_t* loc)
{
  pthread_mutex_lock_tx(&T->lock);
  mark_rdonly_worker(T->head, loc);
  pthread_mutex_unlock_tx(&T->lock);
}

void mark_rdonly_worker(fnode_t* head, snode_t* loc)
{
  if (head->valid && stequal(head->loc->st, loc->st) && S_ISDIR(head->file.mode))
    head->wr = -1;
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      mark_rdonly_worker(head->child[i], loc);
}

void unmark_rdonly(trie_t* T, snode_t* loc)
{
  pthread_mutex_lock_tx(&T->lock);
  unmark_rdonly_worker(T->head, loc);
  pthread_mutex_unlock_tx(&T->lock);
}

void unmark_rdonly_worker(fnode_t* head, snode_t* loc)
{
  if (head->valid && stequal(head->loc->st, loc->st) && S_ISDIR(head->file.mode))
    head->wr = 0;
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      unmark_rdonly_worker(head->child[i], loc);
}

snode_t* available_server(fnode_t* node)
{
  snode_t* snode = NULL;
  if (node->loc && node->loc->down == 0)
    snode = node->loc;
  else if (node->bkp1 && node->bkp1->down == 0)
    snode = node->bkp1;
  else if (node->bkp2 && node->bkp2->down == 0)
    snode = node->bkp2;
  return snode;
}

/*
fnode_t* check_ghost_files(trie_t* T)
{
  fnode_t* node = NULL;
  pthread_mutex_lock_tx(&(T->lock));
  node = check_ghost_worker(T->head);
  pthread_mutex_unlock_tx(&(T->lock));
  return node;
}

fnode_t* check_ghost_worker(fnode_t* head)
{
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      if (check_ghost_worker(head->child[i]))
        return head->child[i];

  if ((head->valid == 0) && (head->rd <= 0) && (head->wr <= 0) && (head->loc || head->bkp1 || head->bkp2))
    return head;

  return NULL;
}

fnode_t* check_vulnerable_files(trie_t* T)
{
  fnode_t* node = NULL;
  pthread_mutex_lock_tx(&(T->lock));
  node = check_vulnerable_worker(T->head);
  pthread_mutex_unlock_tx(&(T->lock));
  return node;
}

fnode_t* check_vulnerable_worker(fnode_t* head)
{
  if (head->valid && (S_ISDIR(head->file.mode) == 0) &&
     ((head->loc != 0) || (head->bkp1 != 0) || (head->bkp2 != 0)))
    return head;

  fnode_t* node = NULL;
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != 0)
      if ((node = check_vulnerable_worker(head->child[i])) != NULL)
        return node;

  return NULL;
}

void trie_prune(trie_t* T)
{
  pthread_mutex_lock_tx(&T->lock);
  trie_prune_worker(T, T->head);
  pthread_mutex_unlock_tx(&T->lock);
}

int trie_prune_worker(trie_t* T, fnode_t* head)
{
  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != NULL)
      if (trie_prune_worker(T, head->child[i])) {
        free(head->child[i]);
        head->child[i] = 0;
      }

  for (int i = 0; i < CHARSET; i++)
    if (head->child[i] != NULL)
      return 0;
  
  if (head->loc || head->bkp1 || head->bkp2)
    if (head != T->head)
      return 1;

  return 0;
}
*/