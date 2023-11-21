#include "defs.h"

extern list_t storage;
extern trie_t files;
extern cache_t cache;
extern logfile_t* logfile;

void* handle_create_dir(void* arg)
{
  return NULL;
}

void* handle_create_file(void* arg)
{

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
