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
      case BACKUP:
        pthread_create_tx(&worker, NULL, handle_backup_recv, req); break;
      case COPY_ACROSS:
        pthread_create_tx(&worker, NULL, handle_copy_recv, req); break;
      case UPDATE:
        pthread_create_tx(&worker, NULL, handle_update_recv, req); break;
      default:
        pthread_create_tx(&worker, NULL, handle_invalid, req);
    }
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
  return NULL;
}

void* handle_copy_send(void* arg)
{
  return NULL;
}

void* handle_update_recv(void* arg)
{
  return NULL;
}
