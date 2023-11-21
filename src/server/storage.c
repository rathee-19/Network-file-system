#include "defs.h"

extern list_t storage;
extern trie_t files;
extern cache_t cache;
extern logfile_t* logfile;

void* stping(void* arg)
{
  while (1)
  {
    snode_t* node = storage.head->next;
    while (node != storage.head)
    {
      int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

      struct sockaddr_in addr;
      memset(&addr, '\0', sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(node->st.nsport);
      addr.sin_addr.s_addr = inet_addr_tx(node->st.ip);

      if (connect_tb(sock, (struct sockaddr*) &addr, sizeof(addr), PING_TIMEOUT)) {
        (node->down)++;
        if (node->down == PING_TOLERANCE)
          mark_rdonly(&files, node);
        goto ping_next;
      }

      message_t msg;
      msg.type = PING;
      send_tx(sock, &msg, sizeof(msg), 0);
      recv_tx(sock, &msg, sizeof(msg), 0);

      switch (msg.type)
      {
        case PING + 1:
          if (node->down < PING_TOLERANCE)
            unmark_rdonly(&files, node);
          node->down = 0;
          logns(logfile, STATUS, "Received ping acknowledgment from storage server %s:%d\n", node->st.ip, node->st.nsport);
          break;

        default:
          logns(logfile, STATUS, "Failed to receive a ping acknowledgment from storage server %s:%d\n", node->st.ip, node->st.nsport);
      }

ping_next:
      node = node->next;
      close_tx(sock);
    }

    sleep(PING_SLEEP);
  }

  return NULL;
}

void* filecrawl(void* arg)
{
  while (1)
  {
    /*fnode_t* file = NULL;
    if ((file = check_ghost_files(&files)) != NULL)
      request_delete(file);
    else if ((file = check_vulnerable_files(&files)) != NULL)
      request_replicate(file);
    else {
      trie_prune(&files);*/
      sleep(CRAWL_SLEEP);
    //}
  }

  return NULL;
}

void* handle_join(void* arg)
{ 
  request_t* req = arg;
  message_t buffer = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received join request from %s:%d\n", ip, port);

  storage_t st;
  strcpy(st.ip, buffer.data);
  st.nsport = atoi(buffer.data + 32);
  st.clport = atoi(buffer.data + 64);
  st.stport = atoi(buffer.data + 96);
  int bytes = atoi(buffer.data + 128);

  metadata_t** data = (metadata_t**) malloc(bytes);
  void* ptr = data;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(sock, &buffer, sizeof(buffer), 0);
    
    switch (buffer.type)
    {
      case STOP:
        logns(logfile, PROGRESS, "Received %d bytes from %s:%d\n", bytes, ip, port);
        goto process_join;

      case JOIN:
        if (end >= ptr + BUFSIZE - sizeof(int32_t)) {
          memcpy(ptr, buffer.data, BUFSIZE);
          ptr += BUFSIZE;
          continue;
        }
        else if (end > ptr) {
          memcpy(ptr, buffer.data, (void*) data + bytes - ptr);
          ptr = end;
          continue;
        }

      default:
        logns(logfile, EVENT, "Received invalid request from %s/%d\n", ip, port);
        buffer.type = INVALID;
        send_tx(sock, &buffer, sizeof(buffer), 0);
        goto ret_join;
    }
  }

process_join:
  snode_t* stnode = list_insert(&storage, st);
  if (stnode != NULL) {
    metadata_t* file = (metadata_t*) data;
    while (end > (void*) file) {
      trie_insert(&files, file, stnode);
      file++;
    }
    logns(logfile, COMPLETION, "Storage server %s:%d joined the network\n", ip, port);
  }
  buffer.type = JOIN + 1;
  send_tx(sock, &buffer, sizeof(buffer), 0);

ret_join:
  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_read(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received read request from %s:%d, for %s\n", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file == 0) {
    msg.type = NOTFOUND;
    logns(logfile, FAILURE, "Returning read request from %s:%d, for unknown file %s\n", ip, port, msg.data);
  }
  else {
    pthread_mutex_lock(&(file->lock));
    if (file->wr > 0) {
      msg.type = XLOCK;
      logns(logfile, FAILURE, "Returning read request from %s:%d, due to %s being locked\n", ip, port, msg.data);
    }
    else if (file->loc && file->loc->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->loc->st.ip);
      sprintf(msg.data + 32, "%d", file->loc->st.clport);
      logns(logfile, COMPLETION, "Redirecting read request from %s:%d, to %s:%d\n", ip, port, file->loc->st.ip, file->loc->st.clport);
    }
    else if (file->bkp1 && file->bkp1->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp1->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp1->st.clport); 
      logns(logfile, COMPLETION, "Redirecting read request from %s:%d, to %s:%d\n", ip, port, file->bkp1->st.ip, file->bkp1->st.clport);
    }
    else if (file->bkp2 && file->bkp2->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp2->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp2->st.clport); 
      logns(logfile, COMPLETION, "Redirecting read request from %s:%d, to %s:%d\n", ip, port, file->bkp2->st.ip, file->bkp2->st.clport);
    }
    else {
      msg.type = UNAVAILABLE;
      logns(logfile, FAILURE, "Returning read request from %s:%d, due to %s being unavailable\n", ip, port, msg.data);
    }
    pthread_mutex_unlock(&(file->lock));
  }

  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_read_completion(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received read acknowledgment from %s:%d, for %s\n", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file != 0) {
    pthread_mutex_lock(&(file->lock));
    file->rd--;
    pthread_mutex_unlock(&(file->lock));
  }

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_write(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received write request from %s:%d, for %s\n", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file == 0) {
    msg.type = NOTFOUND;
    logns(logfile, FAILURE, "Returning write request from %s:%d, for unknown file %s\n", ip, port, msg.data);
  }
  else {
    pthread_mutex_lock(&(file->lock));
    if (file->rd > 0) {
      msg.type = BEING_READ;
      logns(logfile, FAILURE, "Returning write request from %s:%d, due to %s being read\n", ip, port, msg.data);
    }
    else if (file->wr != 0) {
      msg.type = XLOCK;
      logns(logfile, FAILURE, "Returning write request from %s:%d, due to %s being locked\n", ip, port, msg.data);
    }
    else if (file->loc && file->loc->down == 0) {
      (file->wr)++;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->loc->st.ip);
      sprintf(msg.data + 32, "%d", file->loc->st.clport); 
      logns(logfile, COMPLETION, "Redirecting write request from %s:%d, to %s:%d\n", ip, port, file->loc->st.ip, file->loc->st.clport);
    }
    else if (file->bkp1 && file->bkp1->down == 0) {
      (file->wr)++;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp1->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp1->st.clport); 
      logns(logfile, COMPLETION, "Redirecting write request from %s:%d, to %s:%d\n", ip, port, file->bkp1->st.ip, file->bkp1->st.clport);
    }
    else if (file->bkp2 && file->bkp2->down == 0) {
      (file->wr)++;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp2->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp2->st.clport); 
      logns(logfile, COMPLETION, "Redirecting write request from %s:%d, to %s:%d\n", ip, port, file->bkp2->st.ip, file->bkp2->st.clport);
    }
    else {
      msg.type = UNAVAILABLE;
      logns(logfile, FAILURE, "Returning write request from %s:%d, due to %s being unavailable\n", ip, port, msg.data);
    }
    pthread_mutex_unlock(&(file->lock));
  }

  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_write_completion(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received write acknowledgment from %s:%d, for %s\n", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file != 0) {
    pthread_mutex_lock(&(file->lock));
    file->wr--;
    pthread_mutex_unlock(&(file->lock));
  }

  int bytes;
  metadata_t* info;

  msg.type = INFO;
  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch(msg.type)
  {
    case INFO + 1:
      bytes = atoi(msg.data);
      logns(logfile, COMPLETION, "Receiving %d bytes of metadata from %s:%d", bytes, ip, port);
      break;
    default:
      goto ret_write_ack;
  }

  info = (metadata_t*) malloc(bytes);
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logns(logfile, COMPLETION, "Received %d bytes of metadata from %s:%d\n", bytes, ip, port);
        trie_update(&files, info);
        goto ret_write_ack;

      case INFO + 1:
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
        goto ret_write_ack;
    }
  }

ret_write_ack:
  close_tx(sock);
  free(req);
  return NULL;
}

void* handle_copy(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(logfile, EVENT, "Received copy request from %s:%d, for %s\n", ip, port, msg.data);

  close_tx(sock);
  free(req);
  return NULL;
}

void request_delete(fnode_t* node)
{
  if (node->loc) {
    request_delete_worker(node, node->loc);
    node->loc = 0;
  }

  if (node->bkp1) {
    request_delete_worker(node, node->bkp1);
    node->bkp1 = 0;
  }

  if (node->bkp2) {
    request_delete_worker(node, node->bkp2);
    node->bkp2 = 0;
  }
}

void request_delete_worker(fnode_t* node, snode_t* snode)
{
  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(snode->st.nsport);
  addr.sin_addr.s_addr = inet_addr_tx(snode->st.ip);

  connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));

  message_t msg;
  msg.type = DELETE;
  bzero(msg.data, BUFSIZE);
  strcpy(msg.data, node->file.path);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case DELETE + 1:
      logns(logfile, COMPLETION, "Received delete acknowledgement, from %s:%d, for %s\n", snode->st.ip, snode->st.nsport, node->file.path); break;
    default:
      logns(logfile, FAILURE, "Failed to receive delete acknowledgement, from %s:%d, for %s\n", snode->st.ip, snode->st.nsport, node->file.path);
  }

  close_tx(sock);
}

void request_replicate(fnode_t* node)
{

}
