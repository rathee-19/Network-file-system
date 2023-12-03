#include "defs.h"

extern list_t storage;
extern trie_t files;
extern queue_t qdel;
extern queue_t qrep;
extern cache_t cache;
extern logfile_t* logfile;

void* stping(void* arg)
{
  request_t* req = reqalloc();

  while (1)
  {
    snode_t* node = storage.head->next;
    while (node != storage.head)
    {
      int sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
      req->sock = sock;

      struct sockaddr_in addr;
      memset(&addr, '\0', sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(node->st.nsport);
      addr.sin_addr.s_addr = inet_addr_tpx(req, node->st.ip);

      if (connect_tb(sock, (struct sockaddr*) &addr, sizeof(addr), PING_TIMEOUT)) {
        (node->down)++;
        if (node->down == PING_TOLERANCE)
          mark_rdonly(&files, node);
        goto ping_next;
      }

      message_t msg;
      msg.type = PING;
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      recv_tpx(req, sock, &msg, sizeof(msg), 0);

      switch (msg.type)
      {
        case PING + 1:
          if (node->down < PING_TOLERANCE)
            unmark_rdonly(&files, node);
          node->down = 0;
          logns(STATUS, "Received ping acknowledgment from storage server %s:%d", node->st.ip, node->st.nsport);
          break;

        default:
          logns(STATUS, "Failed to receive a ping acknowledgment from storage server %s:%d", node->st.ip, node->st.nsport);
      }

ping_next:
      node = node->next;
      close_tpx(req, sock);
    }

    sleep(PING_SLEEP);
  }

  free(req);
  return NULL;
}

void* filecrawl(void* arg)
{
  while (1)
  {
    pthread_t worker;
    fnode_t* file = NULL;
    if ((file = queue_pop(&qdel)) != NULL)
      pthread_create_tx(&worker, NULL, request_delete, file);
    else if ((storage.num > 2) && ((file = queue_pop(&qrep)) != NULL))
      pthread_create_tx(&worker, NULL, request_replicate, file);
    else
      sleep(CRAWL_SLEEP);
    usleep(CRAWL_COOLDOWN);
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
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received join request from %s:%d", ip, port);

  storage_t st;
  strcpy(st.ip, buffer.data);
  st.nsport = atoi(buffer.data + 32);
  st.clport = atoi(buffer.data + 64);
  st.stport = atoi(buffer.data + 96);
  int bytes = atoi(buffer.data + 128);

  metadata_t** data = (metadata_t**) malloc(bytes);
  req->allocptr = (void*) data;
  void* ptr = data;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tpx(req, sock, &buffer, sizeof(buffer), 0);
    
    switch (buffer.type)
    {
      case STOP:
        logns(PROGRESS, "Received %d bytes from %s:%d", bytes, ip, port);
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
        logns(EVENT, "Received invalid request from %s/%d", ip, port);
        buffer.type = INVALID;
        send_tpx(req, sock, &buffer, sizeof(buffer), 0);
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
    logns(COMPLETION, "Storage server %s:%d joined the network", ip, port);
  }
  buffer.type = JOIN + 1;
  send_tpx(req, sock, &buffer, sizeof(buffer), 0);

ret_join:
  reqfree(req);
  return NULL;
}

void* handle_read(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received read request from %s:%d, for %s", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file == 0) {
    msg.type = NOTFOUND;
    logns(FAILURE, "Returning read request from %s:%d, for unknown file %s", ip, port, msg.data);
  }
  else if (S_ISDIR(file->file.mode)) {
    msg.type = INVALID;
    logns(FAILURE, "Returning read request from %s:%d, for directory %s", ip, port, msg.data);
  }
  else {
    pthread_mutex_lock_tx(&(file->lock));
    if (file->wr > 0) {
      msg.type = XLOCK;
      logns(FAILURE, "Returning read request from %s:%d, due to %s being locked", ip, port, msg.data);
    }
    else if (file->loc && file->loc->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->loc->st.ip);
      sprintf(msg.data + 32, "%d", file->loc->st.clport);
      logns(COMPLETION, "Redirecting read request from %s:%d, to %s:%d", ip, port, file->loc->st.ip, file->loc->st.clport);
    }
    else if (file->bkp1 && file->bkp1->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp1->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp1->st.clport); 
      logns(COMPLETION, "Redirecting read request from %s:%d, to %s:%d", ip, port, file->bkp1->st.ip, file->bkp1->st.clport);
    }
    else if (file->bkp2 && file->bkp2->down == 0) {
      (file->rd)++;
      msg.type = READ + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp2->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp2->st.clport); 
      logns(COMPLETION, "Redirecting read request from %s:%d, to %s:%d", ip, port, file->bkp2->st.ip, file->bkp2->st.clport);
    }
    else {
      msg.type = UNAVAILABLE;
      logns(FAILURE, "Returning read request from %s:%d, due to %s being unavailable", ip, port, msg.data);
    }
    pthread_mutex_unlock_tx(&(file->lock));
  }

  send_tpx(req, sock, &msg, sizeof(msg), 0);

  reqfree(req);
  return NULL;
}

void* handle_read_completion(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received read acknowledgment from %s:%d, for %s", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file != 0) {
    pthread_mutex_lock_tx(&(file->lock));
    file->rd--;
    pthread_mutex_unlock_tx(&(file->lock));
  }

  reqfree(req);
  return NULL;
}

void* handle_write(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received write request from %s:%d, for %s", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  if (file == 0) {
    msg.type = NOTFOUND;
    logns(FAILURE, "Returning write request from %s:%d, for unknown file %s", ip, port, msg.data);
  }
  else if (S_ISDIR(file->file.mode)) {
    msg.type = INVALID;
    logns(FAILURE, "Returning read request from %s:%d, for directory %s", ip, port, msg.data);
  }
  else {
    pthread_mutex_lock_tx(&(file->lock));
    if (file->rd > 0) {
      msg.type = BEING_READ;
      logns(FAILURE, "Returning write request from %s:%d, due to %s being read", ip, port, msg.data);
    }
    else if (file->wr != 0) {
      msg.type = XLOCK;
      logns(FAILURE, "Returning write request from %s:%d, due to %s being locked", ip, port, msg.data);
    }
    else if (file->loc && file->loc->down == 0) {
      (file->wr)++;
      file->writer = file->loc;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->loc->st.ip);
      sprintf(msg.data + 32, "%d", file->loc->st.clport); 
      logns(COMPLETION, "Redirecting write request from %s:%d, to %s:%d", ip, port, file->loc->st.ip, file->loc->st.clport);
    }
    else if (file->bkp1 && file->bkp1->down == 0) {
      (file->wr)++;
      file->writer = file->bkp1;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp1->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp1->st.clport); 
      logns(COMPLETION, "Redirecting write request from %s:%d, to %s:%d", ip, port, file->bkp1->st.ip, file->bkp1->st.clport);
    }
    else if (file->bkp2 && file->bkp2->down == 0) {
      (file->wr)++;
      file->writer = file->bkp2;
      msg.type = WRITE + 1;
      bzero(msg.data, BUFSIZE);
      sprintf(msg.data, "%s", file->bkp2->st.ip);
      sprintf(msg.data + 32, "%d", file->bkp2->st.clport); 
      logns(COMPLETION, "Redirecting write request from %s:%d, to %s:%d", ip, port, file->bkp2->st.ip, file->bkp2->st.clport);
    }
    else {
      msg.type = UNAVAILABLE;
      logns(FAILURE, "Returning write request from %s:%d, due to %s being unavailable", ip, port, msg.data);
    }
    pthread_mutex_unlock_tx(&(file->lock));
  }

  send_tpx(req, sock, &msg, sizeof(msg), 0);

  reqfree(req);
  return NULL;
}

void* handle_write_completion(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received write acknowledgment from %s:%d, for %s", ip, port, msg.data);

  fnode_t* file = cache_search(&cache, msg.data);
  if (file == NULL) {
    file = trie_search(&files, msg.data);
    if (file != NULL)
      cache_insert(&cache, file);
  }

  int bytes;
  metadata_t* info;

  msg.type = INFO;
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);

  switch(msg.type)
  {
    case INFO + 1:
      bytes = atoi(msg.data);
      logns(COMPLETION, "Receiving %d bytes of metadata from %s:%d", bytes, ip, port);
      break;
    default:
      goto ret_write_ack;
  }

  info = (metadata_t*) malloc(bytes);
  req->allocptr = info;
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tpx(req, sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logns(COMPLETION, "Received %d bytes of metadata from %s:%d", bytes, ip, port);
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
  if (file != 0) {
    if (file->loc && file->loc != file->writer)
      request_update(req, file->loc, file->writer);
    if (file->bkp1 && file->bkp1 != file->writer)
      request_update(req, file->bkp1, file->writer);
    if (file->bkp2 && file->bkp2 != file->writer)
      request_update(req, file->bkp2, file->writer);

    pthread_mutex_lock_tx(&(file->lock));
    file->wr--;
    pthread_mutex_unlock_tx(&(file->lock));
  }

  reqfree(req);
  return NULL;
}

void* handle_copy(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logns(EVENT, "Received copy request from %s:%d, for %s", ip, port, msg.data);

  char curpath[PATH_MAX];
  char newpath[PATH_MAX];

  sprintf(curpath, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  sprintf(newpath, "%s", msg.data);

  fnode_t* curnode = cache_search(&cache, curpath);
  if (curnode == NULL) {
    curnode = trie_search(&files, curpath);
    if (curnode != NULL)
      cache_insert(&cache, curnode);
  }

  if (curnode == NULL) {
    msg.type = NOTFOUND;
    logns(FAILURE, "Returning copy request from %s:%d, for unknown file %s", ip, port, curpath);
    goto ret_copy;
  }

  char parent[PATH_MAX];
  get_parent_dir(parent, newpath);

  fnode_t* newnode = cache_search(&cache, parent);
  if (newnode == NULL) {
    newnode = trie_search(&files, parent);
    if (newnode != NULL)
      cache_insert(&cache, newnode);
  }

  if (newnode == NULL) {
    msg.type = NOTFOUND;
    logns(FAILURE, "Returning copy request from %s:%d, to unknown parent directory %s", ip, port, parent);
    goto ret_copy;
  }

  snode_t* sender = available_server(curnode);
  snode_t* receiver = available_server(newnode);

  if (sender == NULL || receiver == NULL) {
    msg.type = UNAVAILABLE;
    goto ret_copy;
  }

  char ss_ip[INET_ADDRSTRLEN];
  int ss_port;

  sprintf(ss_ip, "%s", sender->st.ip);
  ss_port = sender->st.nsport;
  logns(PROGRESS, "Redirecting create file request from %s:%d, to %s:%d", ip, port, ss_ip, ss_port);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ss_port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ss_ip);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));
  
  if (stequal(sender->st, receiver->st)) {
    msg.type = COPY_INTERNAL;
    sprintf(msg.data, "%s", curpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%s", newpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    logns(PROGRESS, "Forwared copy request to %s:%d, for %s", ss_ip, ss_port, curpath);

    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    switch(msg.type)
    {
      case COPY_INTERNAL + 1:
        logns(COMPLETION, "Received copy acknowledgment from %s:%d, for %s", ss_ip, ss_port, curpath);
        msg.type = COPY + 1;
        break;
      
      default:
    }
  }
  else {
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", curpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%s", newpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%s", receiver->st.ip);
    sprintf(msg.data + 32, "%d", receiver->st.stport);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    logns(PROGRESS, "Forwarded copy request to %s:%d, for %s", ss_ip, ss_port, curpath);

    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    switch(msg.type)
    {
      case COPY_ACROSS + 1:
        logns(COMPLETION, "Received copy acknowledgment from %s:%d, for %s", ss_ip, ss_port, curpath);
        msg.type = COPY + 1;
        break;
      
      default:
    }
  }
  close_tpx(req, ss_sock);

ret_copy:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}

void* request_delete(void* arg)
{
  fnode_t* node = (fnode_t*) arg;

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

  return NULL;
}

void request_delete_worker(fnode_t* node, snode_t* snode)
{
  request_t* req = reqalloc();
  int sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->sock = sock;

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(snode->st.nsport);
  addr.sin_addr.s_addr = inet_addr_tpx(req, snode->st.ip);

  connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));

  message_t msg;
  msg.type = DELETE;
  bzero(msg.data, BUFSIZE);
  strcpy(msg.data, node->file.path);

  send_tpx(req, sock, &msg, sizeof(msg), 0);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case DELETE + 1:
      logns(COMPLETION, "Received delete acknowledgement, from %s:%d, for %s", snode->st.ip, snode->st.nsport, node->file.path); break;
    default:
      logns(FAILURE, "Failed to receive delete acknowledgement, from %s:%d, for %s", snode->st.ip, snode->st.nsport, node->file.path);
  }

  reqfree(req);
}

void* request_replicate(void* arg)
{
  fnode_t* node = (fnode_t*) arg;
  request_t* req = reqalloc();

  int succ;
  int fail;
  message_t msg;
  snode_t* snode;

  char ip[INET_ADDRSTRLEN];
  sprintf(ip, "%s", node->loc->st.ip);
  int port = node->loc->st.nsport;

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ip);

  for (succ = 0, fail = 0; succ < 2 && fail < REPLICATE_TOLERANCE; succ++)
  {
    snode = list_random(&storage);
    while (snode == NULL || snode == node->loc || (succ && (snode == node->bkp1)))
      snode = list_random(&storage);

    req->sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
    connect_t(req->sock, (struct sockaddr*) &addr, sizeof(addr));

    logns(PROGRESS, "Sending replicate request to %s:%d, for %s", ip, port, node->file.path);
    msg.type = BACKUP;
    strcpy(msg.data, node->file.path);
    send_tpx(req, req->sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%s", snode->st.ip);
    sprintf(msg.data + 32, "%d", snode->st.stport);
    send_tpx(req, req->sock, &msg, sizeof(msg), 0);
    logns(PROGRESS, "Sent replicate request to %s:%d, for %s", ip, port, node->file.path);

    recv_tpx(req, req->sock, &msg, sizeof(msg), 0);

    switch (msg.type)
    {
      case BACKUP + 1:
        logns(COMPLETION, "Received replication acknowledgment from %s:%d, for %s", ip, port, node->file.path);
        if (succ == 0)
          node->bkp1 = snode;
        else
          node->bkp2 = snode;
        break;

      default:
        logns(FAILURE, "Failed to receive replication acknowledgment from %s:%d, for %s", ip, port, node->file.path);
        fail++;
        succ--;
    }

    close_tpx(req, req->sock);
  }

  switch (succ)
  {
    case 0:
      logns(FAILURE, "Failed to replicate the file %s completely", node->file.path); break;
    case 1:
      logns(FAILURE, "Failed to replicate the file %s partially", node->file.path); break;
    default:
      logns(COMPLETION, "Finished replicating %s", node->file.path);
  }

  reqfree(req);
  return NULL;
}

void request_update(request_t* req, snode_t* dest, snode_t* src)
{
  message_t msg = req->msg;
  char path[PATH_MAX];
  char src_ip[INET_ADDRSTRLEN];
  char dest_ip[INET_ADDRSTRLEN];
  int src_port = src->st.nsport;
  int dest_port = dest->st.stport;

  strcpy(path, msg.data);
  sprintf(src_ip, "%s", src->st.ip);
  sprintf(dest_ip, "%s", dest->st.ip);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(src_port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, src_ip);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));
  
  msg.type = UPDATE;
  send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
  
  sprintf(msg.data, "%s", dest_ip);
  sprintf(msg.data + 32, "%d", dest_port);
  send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
  logns(PROGRESS, "Sent update request to %s:%d, for %s, at %s:%d", src_ip, src_port, path, dest_ip, dest_port);
  
  recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case UPDATE + 1:
      logns(COMPLETION, "Received update acknowledgment from %s:%d, for %s, at %s:%d", src_ip, src_port, path, dest_ip, dest_port);
      break;

    default:
      logns(FAILURE, "Failed to receive update acknowledgment from %s:%d, for %s, at %s:%d", src_ip, src_port, path, dest_ip, dest_port);
  }
}
