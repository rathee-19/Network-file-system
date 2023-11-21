#include "defs.h"

list_t storage;
trie_t files;
cache_t cache;
logfile_t* logfile;

int main(void)
{
  list_init(&storage);
  trie_init(&files);
  cache_init(&cache);

#ifdef LOG
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  sprintf(logfile->path, "serverlog");
  pthread_mutex_init(&(logfile->lock), NULL);
#else
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  logfile->path[0] = 0;
  pthread_mutex_init(&(logfile->lock), NULL);
#endif

  pthread_t ping;
  pthread_create_tx(&ping, NULL, stping, NULL);

  pthread_t crawl;
  pthread_create_tx(&crawl, NULL, filecrawl, NULL);

  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  bind_tx(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 50);                                          // TODO: IMP: how much backlog can we tolerate??

  while (1)
  {
    pthread_t worker;
    request_t* req = (request_t*) calloc(1, sizeof(request_t));
    req->addr_size = sizeof(req->addr);

    req->sock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addr_size));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    logns(logfile, EVENT, "Accepted connection from %s:%d\n", ip, port);

    pthread_create_tx(&worker, NULL, thread_assignment, req);
  }

  close_tx(sock);
  return 0;
}

void* thread_assignment(void* arg)
{
  request_t* req = arg;
  recv_tx(req->sock, &(req->msg), sizeof(req->msg), 0);

  switch(req->msg.type)
  {
    case COPY:
      handle_copy(req); break;
    case CREATE_DIR:
      handle_create_dir(req); break;
    case CREATE_FILE:
      handle_create_file(req); break;
    case DELETE:
      handle_delete(req); break;
    case INFO:
      handle_info(req); break;
    case JOIN:
      handle_join(req); break;
    case LIST:
      handle_list(req); break;
    case READ:
      handle_read(req); break;
    case READ + 1:
      handle_read_completion(req); break;
    case WRITE:
      handle_write(req); break;
    case WRITE + 1:
      handle_write_completion(req); break;
    default:
      handle_invalid(req);
  }

  return NULL;
}
