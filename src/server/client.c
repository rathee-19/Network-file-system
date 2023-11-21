#include "defs.h"

extern list_t storage;
extern trie_t files;
extern queue_t qdel;
extern queue_t qrep;
extern cache_t cache;
extern logfile_t* logfile;

void* handle_create_dir(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received create directory request from %s:%d, for %s\n", ip, port, msg.data);
  
  fnode_t* node = cache_search(&cache, msg.data);
  if (node == NULL) {
    node = trie_search(&files, msg.data);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  int ss_sock;
  snode_t* snode;
  char ss_ip[INET_ADDRSTRLEN];
  int ss_port;

  if (node != NULL) {
    msg.type = EXISTS;
    logns(logfile, FAILURE, "Returning create directory request from %s:%d, due to collision\n", ip, port);
    goto respond_create_dir;
  }

  char parent[PATH_MAX];
  get_parent_dir(parent, msg.data);

  node = cache_search(&cache, parent);
  if (node == NULL) {
    node = trie_search(&files, parent);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  if (node == NULL || (S_ISDIR(node->file.mode) == 0)) {
    msg.type = NOTFOUND;
    logns(logfile, FAILURE, "Returning create directory request from %s:%d, for unknown parent directory %s\n", ip, port, parent);
    goto respond_create_dir;
  }
  else if (node->loc && node->loc->down == 0)
    snode = node->loc;
  else if (node->bkp1 && node->bkp1->down == 0)
    snode = node->bkp1;
  else if (node->bkp2 && node->bkp2->down == 0)
    snode = node->bkp2;
  else {
    msg.type = UNAVAILABLE;
    logns(logfile, FAILURE, "Returning create directory from %s:%d, due to %s being unavailable\n", ip, port, msg.data);
    goto respond_create_dir;
  }

  msg.type = CREATE_DIR;
  sprintf(ss_ip, "%s", snode->st.ip);
  ss_port = snode->st.nsport;
  logns(logfile, PROGRESS, "Redirecting create directory request from %s:%d, to %s:%d\n", ip, port, ss_ip, ss_port);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ss_port);
  addr.sin_addr.s_addr = inet_addr_tx(ss_ip);

  ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));
  
  send_tx(ss_sock, &msg, sizeof(msg), 0);
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  metadata_t* info;

  switch(msg.type)
  {
    case CREATE_DIR + 1:
      bytes = atoi(msg.data);
      logns(logfile, COMPLETION, "Received create directory acknowledgment from %s:%d\n", ss_ip, ss_port);
      break;
    default:
      goto respond_create_dir;
  }

  info = (metadata_t*) malloc(bytes);
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logns(logfile, COMPLETION, "Received %d bytes of metadata from %s:%d\n", bytes, ss_ip, ss_port);
        trie_insert(&files, info, snode);
        msg.type = CREATE_DIR + 1;
        goto respond_create_dir;

      case CREATE_DIR + 1:
        if (end >= ptr + BUFSIZE) {
          memcpy(ptr, msg.data, BUFSIZE);
          ptr += BUFSIZE;
          continue;
        }
        else if (end > ptr) {
          memcpy(ptr, msg.data, (void*) end - ptr);
          ptr = end;
          continue;
        }

      default:
        msg.type = UNAVAILABLE;
        goto respond_create_dir;
    }
  }
  close_tx(ss_sock);

respond_create_dir:
  send_tx(sock, &msg, sizeof(msg), 0);
  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_create_file(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received create file request from %s:%d, for %s\n", ip, port, msg.data);
  
  fnode_t* node = cache_search(&cache, msg.data);
  if (node == NULL) {
    node = trie_search(&files, msg.data);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  int ss_sock;
  snode_t* snode;
  char ss_ip[INET_ADDRSTRLEN];
  int ss_port;

  if (node != NULL) {
    msg.type = EXISTS;
    logns(logfile, FAILURE, "Returning create file request from %s:%d, due to collision\n", ip, port);
    goto respond_create_file;
  }

  char parent[PATH_MAX];
  get_parent_dir(parent, msg.data);

  node = cache_search(&cache, parent);
  if (node == NULL) {
    node = trie_search(&files, parent);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  if (node == NULL || (S_ISDIR(node->file.mode) == 0)) {
    msg.type = NOTFOUND;
    logns(logfile, FAILURE, "Returning create file request from %s:%d, for unknown parent directory %s\n", ip, port, parent);
    goto respond_create_file;
  }
  else if (node->loc && node->loc->down == 0)
    snode = node->loc;
  else if (node->bkp1 && node->bkp1->down == 0)
    snode = node->bkp1;
  else if (node->bkp2 && node->bkp2->down == 0)
    snode = node->bkp2;
  else {
    msg.type = UNAVAILABLE;
    logns(logfile, FAILURE, "Returning create file from %s:%d, due to %s being unavailable\n", ip, port, msg.data);
    goto respond_create_file;
  }

  msg.type = CREATE_FILE;
  sprintf(ss_ip, "%s", snode->st.ip);
  ss_port = snode->st.nsport;
  logns(logfile, PROGRESS, "Redirecting create file request from %s:%d, to %s:%d\n", ip, port, ss_ip, ss_port);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ss_port);
  addr.sin_addr.s_addr = inet_addr_tx(ss_ip);

  ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));
  
  send_tx(ss_sock, &msg, sizeof(msg), 0);
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  metadata_t* info;

  switch(msg.type)
  {
    case CREATE_FILE + 1:
      bytes = atoi(msg.data);
      logns(logfile, COMPLETION, "Received create file acknowledgment from %s:%d", ss_ip, ss_port);
      break;
    default:
      goto respond_create_file;
  }

  info = (metadata_t*) malloc(bytes);
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logns(logfile, COMPLETION, "Received %d bytes of metadata from %s:%d\n", bytes, ss_ip, ss_port);
        trie_insert(&files, info, snode);
        msg.type = CREATE_FILE + 1;
        goto respond_create_file;

      case CREATE_FILE + 1:
        if (end >= ptr + BUFSIZE) {
          memcpy(ptr, msg.data, BUFSIZE);
          ptr += BUFSIZE;
          continue;
        }
        else if (end > ptr) {
          memcpy(ptr, msg.data, (void*) end - ptr);
          ptr = end;
          continue;
        }

      default:
        msg.type = UNAVAILABLE;
        goto respond_create_file;
    }
  }
  close_tx(ss_sock);

respond_create_file:
  send_tx(sock, &msg, sizeof(msg), 0);
  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_delete(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received delete request from %s:%d, for %s\n", ip, port, msg.data);
  
  fnode_t* node = cache_search(&cache, msg.data);
  if (node == NULL) {
    node = trie_search(&files, msg.data);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  if (node == NULL) {
    msg.type = NOTFOUND;
    logns(logfile, FAILURE, "Returning info request from %s:%d, for unknown file %s\n", ip, port, msg.data);
  }
  else if (S_ISDIR(node->file.mode)) {
    invalidate_dir(&files, node);
    msg.type = DELETE + 1;
    logns(logfile, COMPLETION, "Sending delete confirmation to %s:%d\n", ip, port);
  }
  else {
    pthread_mutex_lock(&(node->lock));
    if (node->rd > 0) {
      msg.type = BEING_READ;
      logns(logfile, FAILURE, "Returning write request from %s:%d, due to %s being read\n", ip, port, msg.data);
    }
    else if (node->wr > 0) {
      msg.type = XLOCK;
      logns(logfile, FAILURE, "Returning write request from %s:%d, due to %s being locked\n", ip, port, msg.data);
    }
    else {
      invalidate_file(&files, node);
      msg.type = DELETE + 1;
      logns(logfile, COMPLETION, "Sending delete confirmation to %s:%d\n", ip, port);
    }
    pthread_mutex_unlock(&(node->lock));
  }
  
  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_info(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received info request from %s:%d, for %s\n", ip, port, msg.data);

  fnode_t* node = cache_search(&cache, msg.data);
  if (node == NULL) {
    node = trie_search(&files, msg.data);
    if (node != NULL)
      cache_insert(&cache, node);
  }

  if (node == 0) {
    msg.type = NOTFOUND;
    send_tx(sock, &msg, sizeof(msg), 0);
    logns(logfile, FAILURE, "Returning info request from %s:%d, for unknown file %s\n", ip, port, msg.data);
  }
  else {
    pthread_mutex_lock(&(node->lock));
    metadata_t info = node->file;
    pthread_mutex_unlock(&(node->lock));
    
    msg.type = INFO + 1;
    int bytes = sizeof(metadata_t);
    sprintf(msg.data, "%ld", sizeof(metadata_t));
    send_tx(sock, &msg, sizeof(msg), 0);
    logns(logfile, PROGRESS, "Sending %d bytes to %s:%d\n", bytes, ip, port);

    int sent = 0;
    void* ptr = &info;
    while (sent < bytes) {
      if (bytes - sent >= BUFSIZE) {
        memcpy(msg.data, ptr + sent, BUFSIZE);
        send_tx(sock, &msg, sizeof(msg), 0);
        sent += BUFSIZE;
      }
      else {
        bzero(msg.data, BUFSIZE);
        memcpy(msg.data, ptr + sent, bytes - sent);
        send_tx(sock, &msg, sizeof(msg), 0);
        sent = bytes;
      }
    }

    msg.type = STOP;
    send_tx(sock, &msg, sizeof(msg), 0);
    logns(logfile, COMPLETION, "Sent %d bytes to %s:%d\n", bytes, ip, port);
  }

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_invalid(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received invalid request from %s:%d\n", ip, port);

  msg.type = INVALID;
  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_list(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received list request from %s:%d\n", ip, port);

  int bytes = 0;
  void* paths = (void*) preorder_traversal(&files, &bytes);

  msg.type = LIST + 1;
  bzero(msg.data, BUFSIZE);
  sprintf(msg.data, "%d", bytes);
  send_tx(sock, &msg, sizeof(msg), 0);
  logns(logfile, PROGRESS, "Sending %d bytes to %s:%d\n", bytes, ip, port);

  int sent = 0;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, paths + sent, BUFSIZE);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, paths + sent, bytes - sent);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);
  logns(logfile, COMPLETION, "Sent %d bytes to %s:%d\n", bytes, ip, port);

  close_tx(sock);
  free(paths);
  free(req);
  return NULL;
}
