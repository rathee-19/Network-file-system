/**
 * store storage server data.
 * search database (tries, hashmaps)
 * LRU caching for search
 * single writer to a file (timeout ??)
 * storage server failure detection (allow only read)
 * asynchronous duplication
 * redundancy, replication and recovery ??
*/

#include "defs.h"

list_t storage;

int main(void)
{
  init_list(&storage);

  pthread_t ping;
  pthread_create_tx(&ping, NULL, stping, NULL);

  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = NSPORT;
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  bind_tx(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 5);                                                       // TODO: IMP: how much backlog can we tolerate??

  while (1)
  {
    pthread_t worker;
    request_t* req = (request_t*) calloc(1, sizeof(request_t));
    req->addr_size = sizeof(req->addr);

    req->sock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addr_size));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    fprintf_t(stdout, "Accepted connection from %s/%d\n", ip, port);

    recv_tx(req->sock, &(req->msg), sizeof(req->msg), 0);
    switch(req->msg.type)
    {
      case CREATE_DIR:
        pthread_create_tx(&worker, NULL, handle_createdir, req); break;
      case CREATE_FILE:
        pthread_create_tx(&worker, NULL, handle_createfile, req); break;
      case DELETE:
        pthread_create_tx(&worker, NULL, handle_delete, req); break;
      case INFO:
        pthread_create_tx(&worker, NULL, handle_info, req); break;
      case LIST:
        pthread_create_tx(&worker, NULL, handle_list, req); break;
      case READ:
        pthread_create_tx(&worker, NULL, handle_read, req); break;
      case STORAGE_JOIN:
        pthread_create_tx(&worker, NULL, handle_join, req); break;
      case WRITE:
        pthread_create_tx(&worker, NULL, handle_write, req); break;
      default:
        pthread_create_tx(&worker, NULL, handle_invalid, req);
    }
  }

  close_tx(sock);
  
  return 0;
}
