#include "defs.h"

extern logfile_t* logfile;

void* stlisten(void* arg)
{
  int port = *((int*) arg);

  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tx(IP);

  bind_tx(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 50);

  while (1)
  {
    pthread_t worker;
    request_t* req = (request_t*) calloc(1, sizeof(request_t));
    req->addr_size = sizeof(req->addr);

    req->sock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addr_size));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    logst(logfile, EVENT, "Accepted connection from %s:%d\n", ip, port);

    pthread_create_tx(&worker, NULL, thread_assignment_st, req);
  }

  return NULL;
}

void* thread_assignment_st(void* arg)
{
  request_t* req = arg;
  recv_tx(req->sock, &(req->msg), sizeof(req->msg), 0);

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
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(logfile, EVENT, "Received copy file request from %s:%d, for %s\n", ip, port, msg.data);

  char path[PATH_MAX];
  sprintf(path, "%s", msg.data);
  
  int bytes;
  recv_tx(sock, &msg, sizeof(msg), 0);
  bytes = atoi(msg.data);

  if (bytes == -1) {
    struct stat st;
    if (stat(path, &st) < 0) {
      if ((mkdir(path, 0777) < 0) || (stat(path, &st) < 0)) {
        msg.type = UNAVAILABLE;
        goto ret_copy_recv;
      }
    }
    logst(logfile, COMPLETION, "Created directory %s\n", path);
  }
  else {
    FILE *file = fopen_tx(path, "w+");
    if (file == NULL) {
      msg.type = UNAVAILABLE;
      goto ret_copy_recv;
    }
    else {
      msg.type = COPY_ACROSS + 1;
      send_tx(sock, &msg, sizeof(msg), 0);
    }

    int left = bytes;
    logst(logfile, PROGRESS, "Receiving %d bytes from %s:%d\n", bytes, ip, port);

    while (1)
    {
      recv_tx(sock, &msg, sizeof(msg), 0);
      switch (msg.type)
      {
        case STOP:
          msg.type = COPY_ACROSS + 1;
          fclose(file);
          logst(logfile, COMPLETION, "Received %d bytes from %s:%d\n", bytes, ip, port);
          goto ret_copy_recv;

        case COPY_ACROSS:
          if (left > BUFSIZE) {
            fwrite_tx(msg.data, sizeof(char), BUFSIZE, file);
            left -= BUFSIZE;
            continue;
          }
          else if (left > 0) {
            fwrite_tx(msg.data, sizeof(char), bytes, file);
            left = 0;
            continue;
          }

        default:
      }
    }
  }

ret_copy_recv:
  send_tx(sock, &msg, sizeof(msg), 0);
  close(sock);
  free(req);
  return NULL;
}

void* handle_copy_send(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(logfile, EVENT, "Received copy file request from naming server, for %s\n", msg.data);

  char curpath[PATH_MAX];
  char newpath[PATH_MAX];
  char ip[INET_ADDRSTRLEN];
  int port;

  sprintf(curpath, "%s", msg.data);
  recv_tx(sock, &msg, sizeof(msg), 0);
  sprintf(newpath, "%s", msg.data);
  recv_tx(sock, &msg, sizeof(msg), 0);
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
  addr.sin_addr.s_addr = inet_addr_tx(ip);

  logst(logfile, PROGRESS, "Connecting to %s:%d, for copy of %s\n", ip, port, msg.data);

  int ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  if (S_ISDIR(st.st_mode)) {
    if ((mkdir(newpath, 0777) < 0) || (stat(newpath, &st) < 0)) {
      msg.type = UNAVAILABLE;
      goto ret_copy_internal;
    }
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", newpath);
    send_tx(ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", -1);
    send_tx(ss_sock, &msg, sizeof(msg), 0);
    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    logst(logfile, COMPLETION, "Sent copy directory request to %s:%d, for %s\n", ip, port, curpath);
  }
  else {
    int bytes = st.st_size;
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", newpath);
    send_tx(ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", bytes);
    send_tx(ss_sock, &msg, sizeof(msg), 0);

    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    if (msg.type != COPY_ACROSS + 1) {
      logst(logfile, FAILURE, "Copy directory request rejected by %s:%d, for %s\n", ip, port, curpath);
      close(ss_sock);
      goto ret_copy_internal;
    }

    logst(logfile, PROGRESS, "Sending %d bytes to %s:%d, for %s\n", bytes, ip, port, curpath);

    int sent = 0;
    msg.type = COPY_ACROSS;
    FILE *file = fopen_tx(curpath, "r");

    while (sent < bytes) {
      if (bytes - sent >= BUFSIZE) {
        fread_tx(msg.data, sizeof(char), BUFSIZE, file);
        send_tx(ss_sock, &msg, sizeof(msg), 0);
        sent += BUFSIZE;
      }
      else {
        bzero(msg.data, BUFSIZE);
        fread_tx(msg.data, sizeof(char), bytes - sent, file);
        send_tx(ss_sock, &msg, sizeof(msg), 0);
        sent = bytes;
      }
    }

    fclose_tx(file);

    msg.type = STOP;
    send_tx(ss_sock, &msg, sizeof(msg), 0);
    logst(logfile, COMPLETION, "Sent %d bytes to %s:%d, for %s\n", bytes, ip, port, curpath);

    msg.type = COPY_ACROSS + 1;
  }

  close(ss_sock);

ret_copy_internal:
  send_tx(sock, &msg, sizeof(msg), 0);
  close(sock);
  free(req);
  return NULL;
}

void* handle_update_recv(void* arg)
{
  return NULL;
}
