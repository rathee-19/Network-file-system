#include "defs.h"

extern logfile_t* logfile;

void* nslisten(void* arg)
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
    req->newsock = accept_tx(sock, (struct sockaddr*) &(req->addr), &(req->addrlen));
    
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
    pthread_create_tx(&worker, NULL, thread_assignment_ns, newreq);
  }

  return NULL;
}

void* thread_assignment_ns(void* arg)
{
  request_t* req = arg;
  recv_tpx(req, req->sock, &(req->msg), sizeof(req->msg), 0);

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
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received copy file request from naming server, for %s", msg.data);

  char curpath[PATH_MAX];
  char newpath[PATH_MAX];

  sprintf(curpath, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  sprintf(newpath, "%s", msg.data);

  struct stat st;
  if (stat(curpath, &st) < 0) {
    msg.type = UNAVAILABLE;
    goto ret_copy_internal;
  }

  if (S_ISDIR(st.st_mode)) {
    if ((mkdir(newpath, 0777) < 0) || (stat(newpath, &st) < 0)) {
      msg.type = UNAVAILABLE;
      goto ret_copy_internal;
    }
    msg.type = COPY_INTERNAL + 1;
  }
  else {
    int src_fd, dst_fd, err, n;
    char buffer[4096];

    src_fd = open(curpath, O_RDONLY);
    if (src_fd < 0) {
      msg.type = UNAVAILABLE;
      goto ret_copy_internal;
    }

    dst_fd = open(newpath, O_CREAT | O_WRONLY | S_IRWXU | S_IRWXG | S_IRWXO);
    if (dst_fd < 0) {
      msg.type = UNAVAILABLE;
      goto ret_copy_internal;
    }

    while (1) {
      err = read(src_fd, buffer, 4096);
      if (err == -1) {
        msg.type = UNAVAILABLE;
        close(src_fd);
        close(dst_fd);
        goto ret_copy_internal;
      }

      n = err;
      if (n == 0) {
        logst(COMPLETION, "Completed copy file request, for %s", curpath);
        msg.type = COPY_INTERNAL + 1;
        close(src_fd);
        close(dst_fd);
        goto ret_copy_internal;
      }

      err = write(dst_fd, buffer, n);
      if (err == -1) {
        msg.type = UNAVAILABLE;
        close(src_fd);
        close(dst_fd);
        goto ret_copy_internal;
      }
    }
  }

ret_copy_internal:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}

void* handle_create_dir(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received create directory request from naming server, for %s", msg.data);

  struct stat st;
  if (stat(msg.data, &st) < 0) {
    if ((mkdir(msg.data, 0777) < 0) || (stat(msg.data, &st) < 0)) {
      msg.type = UNAVAILABLE;
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      goto ret_create_dir;
    }
  }

  logst(COMPLETION, "Created directory %s", msg.data);

  metadata_t info;
  strcpy(info.path, msg.data);
  info.mode = st.st_mode;
  info.size = st.st_size;
  info.ctime = st.st_ctime;
  info.mtime = st.st_mtime;

  msg.type = CREATE_DIR + 1;
  int bytes = sizeof(metadata_t);
  sprintf(msg.data, "%ld", sizeof(metadata_t));
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  logst(PROGRESS, "Sending %d bytes of metadata to naming server", bytes);

  int sent = 0;
  void* ptr = &info;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, ptr + sent, BUFSIZE);
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, ptr + sent, bytes - sent);
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent %d bytes of metadata to naming server", bytes);

ret_create_dir:
  reqfree(req);
  return NULL;
}

void* handle_create_file(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received create file request from naming server, for %s", msg.data);
  
  FILE* file = fopen(msg.data, "w");
  if (file == NULL) {
    msg.type = UNAVAILABLE;
    send_tpx(req, sock, &msg, sizeof(msg), 0);
    goto ret_create_file;
  }
  
  struct stat st;
  if (stat(msg.data, &st) < 0) {
    msg.type = UNAVAILABLE;
    fclose(file);
    send_tpx(req, sock, &msg, sizeof(msg), 0);
    goto ret_create_file;
  }

  fclose(file);
  logst(COMPLETION, "Created file %s", msg.data);

  metadata_t info;
  strcpy(info.path, msg.data);
  info.mode = st.st_mode;
  info.size = st.st_size;
  info.ctime = st.st_ctime;
  info.mtime = st.st_mtime;

  msg.type = CREATE_FILE + 1;
  int bytes = sizeof(metadata_t);
  sprintf(msg.data, "%ld", sizeof(metadata_t));
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  logst(PROGRESS, "Sending %d bytes to naming server", bytes);

  int sent = 0;
  void* ptr = &info;
  while (sent < bytes) {
    if (bytes - sent >= BUFSIZE) {
      memcpy(msg.data, ptr + sent, BUFSIZE);
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      sent += BUFSIZE;
    }
    else {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, ptr + sent, bytes - sent);
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      sent = bytes;
    }
  }

  msg.type = STOP;
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent %d bytes of metadata to naming server", bytes);

ret_create_file:
  reqfree(req);
  return NULL;
}

void* handle_delete(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  if (remove(msg.data) < 0) {
    char path[PATH_MAX];
    add_prefix(path, msg.data, ".backup/");
    remove(path);
  }
  msg.type = DELETE + 1;
  send_tpx(req, sock, &msg, sizeof(msg), 0);

  reqfree(req);
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
  logst(EVENT, "Received ping from %s:%d", ip, port);

  msg.type = PING + 1;
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  logst(COMPLETION, "Sent ping acknowledgement to %s:%d", ip, port);

  reqfree(req);
  return NULL;
}
