#include "defs.h"

extern logfile_t* logfile;

void* cllisten(void* arg)
{
  int port = *((int*) arg);
  request_t* req = reqalloc();

  int sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->sock = sock;

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, IP);

  bind_t(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tpx(req, sock, 128);

  while (1)
  {
    req->allocptr = NULL;
    req->newsock = accept_tpx(req, sock, (struct sockaddr*) &(req->addr), &(req->addrlen));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    logst(EVENT, "Accepted connection from %s:%d", ip, port);

    request_t* newreq = reqalloc();
    req->allocptr = (void*) newreq;
    newreq->sock = req->newsock;
    newreq->addr = req->addr;
    newreq->addrlen = req->addrlen;
    
    pthread_t worker;
    pthread_create_tx(&worker, NULL, thread_assignment_cl, newreq);
  }

  return NULL;
}

void* thread_assignment_cl(void* arg)
{
  request_t* req = arg;
  recv_tpx(req, req->sock, &(req->msg), sizeof(req->msg), 0);

  switch (req->msg.type)
  {
    case READ:
      handle_read(req); break;
    case WRITE:
      handle_write(req); break;
    default:
      handle_invalid(req);
  }

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
  logst(EVENT, "Received read request from %s:%d, for %s", ip, port, msg.data);

  char orgpath[BUFSIZE];
  strcpy(orgpath, msg.data);

  char path[BUFSIZE];
  strcpy(path, msg.data);
  if (access(path, F_OK) != 0)
    add_prefix(path, msg.data, ".backup/");

  FILE *file = fopen_tpx(req, path, "r");
  struct stat st;
  if (stat(path, &st) < 0) {
    msg.type = PERM;
    send_tpx(req, sock, &msg, sizeof(msg), 0);
    logst(FAILURE, "Returning read request from %s:%d, due to lack of permissions", ip, port);
  }
  else {
    int bytes = st.st_size;
    msg.type = READ + 1;
    sprintf(msg.data, "%d", bytes);
    send_tpx(req, sock, &msg, sizeof(msg), 0);
    logst(PROGRESS, "Sending %d bytes to %s:%d", bytes, ip, port);

    int sent = 0;
    while (sent < bytes) {
      if (bytes - sent >= BUFSIZE) {
        fread_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
        send_tpx(req, sock, &msg, sizeof(msg), 0);
        sent += BUFSIZE;
      }
      else {
        bzero(msg.data, BUFSIZE);
        fread_tpx(req, msg.data, sizeof(char), bytes - sent, file);
        send_tpx(req, sock, &msg, sizeof(msg), 0);
        sent = bytes;
      }
    }

    msg.type = STOP;
    send_tpx(req, sock, &msg, sizeof(msg), 0);
  }
  fclose(file);
  
  int ns_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ns_sock;

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tpx(req, NSIP);

  connect_t(ns_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = READ + 1;
  strcpy(msg.data, orgpath);
  send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent read confirmation, for %s, to naming server", path);
  
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
  logst(EVENT, "Received write request from %s:%d, for %s", ip, port, msg.data);

  char orgpath[BUFSIZE];
  strcpy(orgpath, msg.data);

  char path[BUFSIZE];
  strcpy(path, msg.data);
  if (access(path, F_OK) != 0)
    add_prefix(path, msg.data, ".backup/");
    
  FILE *file = fopen_tpx(req, path, "w+");
  
  msg.type = WRITE + 1;
  send_tpx(req, sock, &msg, sizeof(msg), 0);

  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  int bytes = atoi(msg.data);
  logst(PROGRESS, "Receiving %d bytes from %s:%d", bytes, ip, port);

  while (1)
  {
    recv_tpx(req, sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        fclose(file);
        goto finish_write;

      case WRITE:
        if (bytes > BUFSIZE) {
          fwrite_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
          bytes -= BUFSIZE;
          continue;
        }
        else if (bytes > 0) {
          fwrite_tpx(req, msg.data, sizeof(char), bytes, file);
          bytes = 0;
          continue;
        }

      default:
        goto finish_write;
    }
  }

finish_write:
  int ns_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ns_sock;

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tpx(req, NSIP);

  connect_t(ns_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = WRITE + 1;
  strcpy(msg.data, orgpath);
  send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent write confirmation, for %s, to the naming server", path);

  recv_tpx(req, ns_sock, &msg, sizeof(msg), 0);
  
  struct stat st;
  if (stat(msg.data, &st) < 0) {
    msg.type = UNAVAILABLE;
    send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
    goto ret_write;
  }

  metadata_t info;
  strcpy(info.path, msg.data);
  info.mode = st.st_mode;
  info.size = st.st_size;
  info.ctime = st.st_ctime;
  info.mtime = st.st_mtime;

  msg.type = INFO + 1;
  bytes = sizeof(metadata_t);
  sprintf(msg.data, "%ld", sizeof(metadata_t));
  send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
  logst(PROGRESS, "Sending %d bytes of metadata to naming server", bytes);

  int sent = 0;
  void* ptr = &info;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, ptr + sent, BUFSIZE);
      send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, ptr + sent, bytes - sent);
      send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tpx(req, ns_sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent %d bytes of metadata to naming server", bytes);

ret_write:
  reqfree(req);
  return NULL;
}

void* handle_invalid(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(EVENT, "Received invalid request from %s:%d", ip, port);

  msg.type = INVALID;
  send_tpx(req, sock, &msg, sizeof(msg), 0);

  reqfree(req);
  return NULL;
}
