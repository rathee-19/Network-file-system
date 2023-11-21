#include "defs.h"

extern logfile_t* logfile;

void nsnotify(int nsport, int clport, int stport, char* paths, int n)
{
  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NSPORT);
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));
  logst(logfile, STATUS, "Connected to the naming server\n");

  int bytes = 0;
  void* info = (void*) dirinfo(paths, n, &bytes);

  message_t msg;
  msg.type = JOIN;
  bzero(msg.data, BUFSIZE);
  sprintf(msg.data, "%s", IP);
  sprintf(msg.data + 32, "%d", nsport);
  sprintf(msg.data + 64, "%d", clport);
  sprintf(msg.data + 96, "%d", stport);
  sprintf(msg.data + 128, "%d", bytes);

  send_tx(sock, &msg, sizeof(msg), 0);

  int sent = 0;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, info + sent, BUFSIZE);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, info + sent, bytes - sent);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, PROGRESS, "Sent %d bytes to the naming server\n", bytes);

  recv_tx(sock, &msg, sizeof(msg), 0);
  if (msg.type != JOIN + 1) {
    logst(logfile, FAILURE, "Failed to register with the naming server\n");
    exit(1);
  }
  logst(logfile, COMPLETION, "Registered with the naming server\n");

  close_tx(sock);

  pthread_t server, client, storage;
  pthread_create_tx(&server, NULL, nslisten, &nsport);
  pthread_create_tx(&client, NULL, cllisten, &clport);
  pthread_create_tx(&storage, NULL, stlisten, &stport);

  logst(logfile, STATUS, "Started the listener threads\n");
  
  pthread_join(server, NULL);
  pthread_join(client, NULL);
  pthread_join(storage, NULL);
}

void* nslisten(void* arg)
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

    pthread_create_tx(&worker, NULL, thread_assignment_ns, req);
  }

  return NULL;
}

void* thread_assignment_ns(void* arg)
{
  request_t* req = arg;
  recv_tx(req->sock, &(req->msg), sizeof(req->msg), 0);

  switch (req->msg.type)
  {
    case BACKUP:
      handle_backup_send(req); break;
    case COPY_ACROSS:
      handle_copy_send(req); break;
    case COPY_INTERNAL:
      handle_copy_internal(req); break;
    case CREATE_DIR:
      handle_create_dir(req); break;
    case CREATE_FILE:
      handle_create_file(req); break;
    case DELETE:
      handle_delete(req); break;
    case PING:
      handle_ping(req); break;
    case UPDATE:
      handle_update_send(req); break;
    default:
      handle_invalid(req);
  }

  return NULL;
}

void* handle_copy_internal(void* arg)
{
  return NULL;
}

void* handle_create_dir(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(logfile, EVENT, "Received create directory request from naming server, for %s\n", msg.data);

  struct stat st;
  if (stat(msg.data, &st) < 0) {
    if ((mkdir(msg.data, 0777) < 0) || (stat(msg.data, &st) < 0)) {
      msg.type = UNAVAILABLE;
      send_tx(sock, &msg, sizeof(msg), 0);
      goto ret_create_dir;
    }
  }

  logst(logfile, COMPLETION, "Created directory %s\n", msg.data);

  metadata_t info;
  strcpy(info.path, msg.data);
  info.mode = st.st_mode;
  info.size = st.st_size;
  info.ctime = st.st_ctime;
  info.mtime = st.st_mtime;

  msg.type = CREATE_DIR + 1;
  int bytes = sizeof(metadata_t);
  sprintf(msg.data, "%ld", sizeof(metadata_t));
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, PROGRESS, "Sending %d bytes of metadata to naming server\n", bytes);

  int sent = 0;
  void* ptr = &info;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, ptr + sent, BUFSIZE);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, ptr + sent, bytes - sent);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, COMPLETION, "Sent %d bytes of metadata to naming server\n", bytes);

ret_create_dir:
  close_tx(sock);
  return NULL;
}

void* handle_create_file(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(logfile, EVENT, "Received create file request from naming server, for %s\n", msg.data);
  
  FILE* file = fopen(msg.data, "w");
  if (file == NULL) {
    msg.type = UNAVAILABLE;
    send_tx(sock, &msg, sizeof(msg), 0);
    goto ret_create_file;
  }
  
  struct stat st;
  if (stat(msg.data, &st) < 0) {
    msg.type = UNAVAILABLE;
    send_tx(sock, &msg, sizeof(msg), 0);
    goto ret_create_file;
  }

  fclose(file);
  logst(logfile, COMPLETION, "Created file %s\n", msg.data);

  metadata_t info;
  strcpy(info.path, msg.data);
  info.mode = st.st_mode;
  info.size = st.st_size;
  info.ctime = st.st_ctime;
  info.mtime = st.st_mtime;

  msg.type = CREATE_FILE + 1;
  int bytes = sizeof(metadata_t);
  sprintf(msg.data, "%ld", sizeof(metadata_t));
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, PROGRESS, "Sending %d bytes to naming server\n", bytes);

  int sent = 0;
  void* ptr = &info;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, ptr + sent, BUFSIZE);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, ptr + sent, bytes - sent);
      send_tx(sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, COMPLETION, "Sent %d bytes of metadata to naming server\n", bytes);

ret_create_file:
  close_tx(sock);
  return NULL;
}

void* handle_delete(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  // if (del)
    msg.type = DELETE + 1;
  // else
  //  msg.type = NOTFOUND, or
  //  msg.type = PERM
  send_tx(sock, &msg, sizeof(msg), 0);

  close_tx(sock);
  return NULL;
}

void* handle_ping(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;
    
  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(logfile, EVENT, "Received ping from %s:%d\n", ip, port);

  msg.type = PING + 1;
  send_tx(sock, &msg, sizeof(msg), 0);
  logst(logfile, COMPLETION, "Sent ping acknowledgement to %s:%d\n", ip, port);

  close_tx(sock);
  return NULL;
}

void* handle_update_send(void* arg)
{
  return NULL;
}
