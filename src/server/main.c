#include "defs.h"

list_t storage;
trie_t files;
queue_t qdel;
queue_t qrep;
cache_t cache;
logfile_t* logfile;

int main(int argc, char* argv[])
{
#ifdef LOG
  if (argc < 2) {
    fprintf(stderr, "usage: %s <logfile>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  strcpy(logfile->path, argv[1]);
  pthread_mutex_init_tx(&(logfile->lock), NULL);

#else
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  logfile->path[0] = 0;
  pthread_mutex_init_tx(&(logfile->lock), NULL);

#endif
  
  signal_tx(SIGPIPE, SIG_IGN);

  list_init(&storage);
  trie_init(&files);
  queue_init(&qdel);
  queue_init(&qrep);
  cache_init(&cache);

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

  bind_t(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 64);

  while (1)
  {
    pthread_t worker;
    request_t* req = reqalloc();
    req->sock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addrlen));
    
    char ip[INET_ADDRSTRLEN];
    int port = ntohs(req->addr.sin_port);
    inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
    logns(EVENT, "Accepted connection from %s:%d", ip, port);

    pthread_create_tx(&worker, NULL, thread_assignment, req);
  }

  close_tx(sock);
  return 0;
}

void* thread_assignment(void* arg)
{
  request_t* req = arg;
  recv_tpx(req, req->sock, &(req->msg), sizeof(req->msg), 0);

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
