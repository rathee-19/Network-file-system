#include "defs.h"

extern logfile_t* logfile;

void* stlisten(void* arg)
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
  listen_tpx(req, sock, 50);

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

void* thread_assignment_st(void* arg)
{
  request_t* req = arg;
  recv_tpx(req, req->sock, &(req->msg), sizeof(req->msg), 0);

  switch (req->msg.type)
  {
    case BACKUP:
      handle_backup_recv(req); break;
    case COPY_ACROSS:
      handle_copy_recv(req); break;
    case UPDATE:
      handle_update_recv(req); break;
    default:
      handle_invalid(req);
  }

  return NULL;
}

void* handle_backup_recv(void* arg)
{
  return NULL;
}

void* handle_backup_send(void* arg)
{
  return NULL;
}

void* handle_copy_recv(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;
    
  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(EVENT, "Received copy file request from %s:%d, for %s", ip, port, msg.data);

  char path[PATH_MAX];
  sprintf(path, "%s", msg.data);
  
  int bytes;
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  bytes = atoi(msg.data);

  if (bytes == -1) {
    struct stat st;
    if (stat(path, &st) < 0) {
      if ((mkdir(path, 0777) < 0) || (stat(path, &st) < 0)) {
        msg.type = UNAVAILABLE;
        goto ret_copy_recv;
      }
    }
    logst(COMPLETION, "Created directory %s", path);
  }
  else {
    FILE *file = fopen_tpx(req, path, "w+");
    if (file == NULL) {
      msg.type = UNAVAILABLE;
      goto ret_copy_recv;
    }
    else {
      msg.type = COPY_ACROSS + 1;
      send_tpx(req, sock, &msg, sizeof(msg), 0);
    }

    int left = bytes;
    logst(PROGRESS, "Receiving %d bytes from %s:%d", bytes, ip, port);

    while (1)
    {
      recv_tpx(req, sock, &msg, sizeof(msg), 0);
      switch (msg.type)
      {
        case STOP:
          msg.type = COPY_ACROSS + 1;
          fclose(file);
          logst(COMPLETION, "Received %d bytes from %s:%d", bytes, ip, port);
          goto ret_copy_recv;

        case COPY_ACROSS:
          if (left > BUFSIZE) {
            fwrite_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
            left -= BUFSIZE;
            continue;
          }
          else if (left > 0) {
            fwrite_tpx(req, msg.data, sizeof(char), bytes, file);
            left = 0;
            continue;
          }

        default:
      }
    }
  }

ret_copy_recv:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}

void* handle_copy_send(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received copy file request from naming server, for %s", msg.data);

  char curpath[PATH_MAX];
  char newpath[PATH_MAX];
  char ip[INET_ADDRSTRLEN];
  int port;

  sprintf(curpath, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  sprintf(newpath, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  strcpy(ip, msg.data);
  port = atoi(msg.data + 32);

  struct stat st;
  if (stat(curpath, &st) < 0) {
    msg.type = UNAVAILABLE;
    goto ret_copy_internal;
  }

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ip);

  logst(PROGRESS, "Connecting to %s:%d, for copy of %s", ip, port, msg.data);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  if (S_ISDIR(st.st_mode)) {
    if ((mkdir(newpath, 0777) < 0) || (stat(newpath, &st) < 0)) {
      msg.type = UNAVAILABLE;
      goto ret_copy_internal;
    }
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", newpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", -1);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    logst(COMPLETION, "Sent copy directory request to %s:%d, for %s", ip, port, curpath);
  }
  else {
    int bytes = st.st_size;
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", newpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", bytes);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);

    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    if (msg.type != COPY_ACROSS + 1) {
      logst(FAILURE, "Copy directory request rejected by %s:%d, for %s", ip, port, curpath);
      goto ret_copy_internal;
    }

    logst(PROGRESS, "Sending %d bytes to %s:%d, for %s", bytes, ip, port, curpath);

    int sent = 0;
    msg.type = COPY_ACROSS;
    FILE *file = fopen_tpx(req, curpath, "r");

    while (sent < bytes) {
      if (bytes - sent >= BUFSIZE) {
        fread_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
        send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
        sent += BUFSIZE;
      }
      else {
        bzero(msg.data, BUFSIZE);
        fread_tpx(req, msg.data, sizeof(char), bytes - sent, file);
        send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
        sent = bytes;
      }
    }

    fclose_tpx(req, file);

    msg.type = STOP;
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    logst(COMPLETION, "Sent %d bytes to %s:%d, for %s", bytes, ip, port, curpath);

    msg.type = COPY_ACROSS + 1;
  }

ret_copy_internal:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}

void* handle_update_recv(void* arg)
{
  return NULL;
}
