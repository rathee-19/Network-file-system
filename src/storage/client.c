#include "defs.h"

extern logfile_t* logfile;

void* cllisten(void* arg)
{
  int port = *((int*) arg);

  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tx(IP);

  bind_tx(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 5);

  while (1)
  {
    pthread_t worker;
    request_t* req = (request_t*) calloc(1, sizeof(request_t));
    req->addr_size = sizeof(req->addr);

    req->sock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addr_size));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    fprintf_t(stdout, "Accepted connection from %s:%d\n", ip, port);

    recv_tx(req->sock, &(req->msg), sizeof(req->msg), 0);
    switch (req->msg.type)
    {
      case READ:
        pthread_create_tx(&worker, NULL, handle_read, req); break;
      case WRITE:
        pthread_create_tx(&worker, NULL, handle_write, req); break;
      default:
        pthread_create_tx(&worker, NULL, handle_invalid, req);
    }
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
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  fprintf_t(stdout, "Received read request from %s:%d, for %s\n", ip, port, msg.data);

  char orgpath[BUFSIZE];
  strcpy(orgpath, msg.data);

  char path[BUFSIZE];
  strcpy(path, msg.data);
  if (access(path, F_OK) != 0)
    add_prefix(path, ".backup/", msg.data);

  FILE *file = fopen_tx(path, "r");
  struct stat st;
  if (stat(path, &st) < 0) {
    msg.type = PERM;
    send_tx(sock, &msg, sizeof(msg), 0);
    fprintf_t(stderr, "Returning read request from %s:%d, due to lack of permissions\n", ip, port);
  }
  else {
    int bytes = st.st_size;
    msg.type = READ + 1;
    sprintf(msg.data, "%d", bytes);
    send_tx(sock, &msg, sizeof(msg), 0);
    fprintf_t(stdout, "Sending %d bytes to %s:%d\n", bytes, ip, port);

    int sent = 0;
    while (sent < bytes) {
      if (bytes - sent >= BUFSIZE) {
        fread_tx(msg.data, sizeof(char), BUFSIZE, file);
        send_tx(sock, &msg, sizeof(msg), 0);
        sent += BUFSIZE;
      }
      else {
        bzero(msg.data, BUFSIZE);
        fread_tx(msg.data, sizeof(char), bytes - sent, file);
        send_tx(sock, &msg, sizeof(msg), 0);
        sent = bytes;
      }
    }

    msg.type = STOP;
    send_tx(sock, &msg, sizeof(msg), 0);
  }
  
  int ns_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  connect_t(ns_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = READ + 1;
  strcpy(msg.data, orgpath);
  send_tx(ns_sock, &msg, sizeof(msg), 0);
  fprintf_t(stdout, "Sent read confirmation, for %s, to the naming server\n", path);
  
  close(ns_sock);
  fclose(file);

  close(sock);
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
  fprintf_t(stdout, "Received write request from %s:%d, for %s\n", ip, port, msg.data);

  char orgpath[BUFSIZE];
  strcpy(orgpath, msg.data);

  char path[BUFSIZE];
  strcpy(path, msg.data);
  if (access(path, F_OK) != 0)
    add_prefix(path, ".backup/", msg.data);
    
  FILE *file = fopen_tx(path, "w+");
  
  msg.type = WRITE + 1;
  send_tx(sock, &msg, sizeof(msg), 0);

  recv_tx(sock, &msg, sizeof(msg), 0);
  int bytes = atoi(msg.data);
  fprintf_t(stdout, "Receiving %d bytes from %s:%d\n", bytes, ip, port);

  while (1)
  {
    recv_tx(sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        goto finish_write;

      case WRITE:
        if (bytes > BUFSIZE) {
          fwrite_tx(msg.data, sizeof(char), BUFSIZE, file);
          bytes -= BUFSIZE;
          continue;
        }
        else if (bytes > 0) {
          fwrite_tx(msg.data, sizeof(char), bytes, file);
          bytes = 0;
          continue;
        }

      default:
        goto finish_write;
    }
  }

finish_write:
  int ns_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  connect_t(ns_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = WRITE + 1;
  strcpy(msg.data, orgpath);
  send_tx(ns_sock, &msg, sizeof(msg), 0);
  fprintf_t(stdout, "Sent write confirmation, for %s, to the naming server\n", path);

  close(ns_sock);
  fclose(file);
  
  close(sock);
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
  logst(logfile, EVENT, "Received invalid request from %s:%d\n", ip, port);

  msg.type = INVALID;
  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  free(req);
  return NULL;
}
