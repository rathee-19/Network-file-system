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
    pthread_create_tx(&worker, NULL, thread_assignment_st, newreq);
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
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;
    
  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(EVENT, "Received backup file request from %s:%d, for %s", ip, port, msg.data);

  int bytes;
  struct stat st;
  char path[PATH_MAX];
  char newpath[PATH_MAX];
  char parent[PATH_MAX];

  sprintf(path, "%s", msg.data);
  add_prefix(newpath, path, ".backup/");
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  bytes = atoi(msg.data);

  if (bytes == -1) {
    if (stat(newpath, &st) < 0) {
      if (create_dir(newpath) < 0) {
        logst(FAILURE, "Returning backup request from %s:%d, due to failure in creating %s", ip, port, newpath);
        msg.type = UNAVAILABLE;
        send_tpx(req, sock, &msg, sizeof(msg), 0);
        goto ret_backup_recv;
      }
    }
    logst(COMPLETION, "Created directory %s", newpath);
    msg.type = BACKUP + 1;
    send_tpx(req, sock, &msg, sizeof(msg), 0);
  }
  else {
    get_parent_dir(parent, newpath);
    if (stat(parent, &st) < 0) {
      if (create_dir(parent) < 0) {
        logst(FAILURE, "Returning backup request from %s:%d, due to failure in creating %s", ip, port, parent);
        msg.type = UNAVAILABLE;
        send_tpx(req, sock, &msg, sizeof(msg), 0);
        goto ret_backup_recv;
      }
    }
    FILE *file = fopen_tpx(req, newpath, "w+");
    if (file == NULL) {
      logst(FAILURE, "Returning backup request from %s:%d, due to failure in writing %s", ip, port, newpath);
      msg.type = UNAVAILABLE;
      send_tpx(req, sock, &msg, sizeof(msg), 0);
      goto ret_backup_recv;
    }

    msg.type = BACKUP + 1;
    send_tpx(req, sock, &msg, sizeof(msg), 0);

    int left = bytes;
    logst(PROGRESS, "Receiving %d bytes from %s:%d, for %s", bytes, ip, port, path);

    while (1)
    {
      recv_tpx(req, sock, &msg, sizeof(msg), 0);
      switch (msg.type)
      {
        case STOP:
          fclose(file);
          logst(COMPLETION, "Received %d bytes from %s:%d, for %s", bytes, ip, port, path);
          goto ret_backup_recv;

        case BACKUP:
          if (left > BUFSIZE) {
            fwrite_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
            left -= BUFSIZE;
            continue;
          }
          else if (left > 0) {
            fwrite_tpx(req, msg.data, sizeof(char), left, file);
            left = 0;
            continue;
          }

        default:
      }
    }
  }

ret_backup_recv:
  reqfree(req);
  return NULL;
}

void* handle_backup_send(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received replicate file request from naming server, for %s", msg.data);

  char path[PATH_MAX];
  char ip[INET_ADDRSTRLEN];
  int port;

  sprintf(path, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  strcpy(ip, msg.data);
  port = atoi(msg.data + 32);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ip);

  struct stat st;
  if (stat(path, &st) < 0) {
    logst(FAILURE, "Returning replicate request for %s, due to access failure", path);
    msg.type = UNAVAILABLE;
    goto ret_backup_send;
  }

  logst(PROGRESS, "Connecting to %s:%d, for replication of %s", ip, port, path);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  if (S_ISDIR(st.st_mode)) {
    int bytes = -1;
    msg.type = BACKUP;
    sprintf(msg.data, "%s", path);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", bytes);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);

    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    logst(COMPLETION, "Sent replicate directory request to %s:%d, for %s", ip, port, path);

    msg.type = BACKUP + 1;
  }
  else {
    int bytes = st.st_size;
    msg.type = BACKUP;
    sprintf(msg.data, "%s", path);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", bytes);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);

    recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    if (msg.type != BACKUP + 1) {
      logst(FAILURE, "Replication request rejected by %s:%d, for %s", ip, port, path);
      goto ret_backup_send;
    }

    logst(PROGRESS, "Sending %d bytes to %s:%d, for %s", bytes, ip, port, path);

    int sent = 0;
    msg.type = BACKUP;
    FILE *file = fopen_tpx(req, path, "r");

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
    logst(COMPLETION, "Sent %d bytes to %s:%d, for %s", bytes, ip, port, path);

    msg.type = BACKUP + 1;
  }

ret_backup_send:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
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
        logst(FAILURE, "Returning backup request from %s:%d, due to failure in creating %s", ip, port, path);
        msg.type = UNAVAILABLE;
        goto ret_copy_recv;
      }
    }
    logst(COMPLETION, "Created directory %s", path);
  }
  else {
    FILE *file = fopen_tpx(req, path, "w+");
    if (file == NULL) {
      logst(FAILURE, "Returning copy request from %s:%d, due to failure in writing %s", ip, port, path);
      msg.type = UNAVAILABLE;
      goto ret_copy_recv;
    }
    
    msg.type = COPY_ACROSS + 1;
    send_tpx(req, sock, &msg, sizeof(msg), 0);

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
            fwrite_tpx(req, msg.data, sizeof(char), left, file);
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

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ip);

  struct stat st;
  if (stat(curpath, &st) < 0) {
    logst(FAILURE, "Returning copy request for %s, due to access failure", curpath);
    msg.type = UNAVAILABLE;
    goto ret_copy_send;
  }

  logst(PROGRESS, "Connecting to %s:%d, for copy of %s", ip, port, msg.data);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  if (S_ISDIR(st.st_mode)) {
    int bytes = -1;
    msg.type = COPY_ACROSS;
    sprintf(msg.data, "%s", newpath);
    send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
    sprintf(msg.data, "%d", bytes);
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
      goto ret_copy_send;
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

ret_copy_send:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}

void* handle_update_recv(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;
    
  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop_tpx(req, AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);
  logst(EVENT, "Received update file request from %s:%d, for %s", ip, port, msg.data);

  int bytes;
  char path[PATH_MAX];

  if (access(msg.data, F_OK) != 0)
    add_prefix(path, msg.data, ".backup/");
  else
    strcpy(path, msg.data);
  
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  bytes = atoi(msg.data);

  FILE *file = fopen_tpx(req, path, "w+");
  if (file == NULL) {
    logst(FAILURE, "Returning update request from %s:%d, due to failure in writing %s", ip, port, path);
    msg.type = UNAVAILABLE;
    send_tpx(req, sock, &msg, sizeof(msg), 0);
    goto ret_update_recv;
  }

  msg.type = UPDATE + 1;
  send_tpx(req, sock, &msg, sizeof(msg), 0);

  int left = bytes;
  logst(PROGRESS, "Receiving %d bytes from %s:%d, for %s", bytes, ip, port, path);

  while (1)
  {
    recv_tpx(req, sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        fclose(file);
        logst(COMPLETION, "Received %d bytes from %s:%d, for %s", bytes, ip, port, path);
        goto ret_update_recv;

      case UPDATE:
        if (left > BUFSIZE) {
          fwrite_tpx(req, msg.data, sizeof(char), BUFSIZE, file);
          left -= BUFSIZE;
          continue;
        }
        else if (left > 0) {
          fwrite_tpx(req, msg.data, sizeof(char), left, file);
          left = 0;
          continue;
        }

      default:
    }
  }

ret_update_recv:
  reqfree(req);
  return NULL;
}

void* handle_update_send(void* arg)
{
  request_t* req = arg;
  message_t msg = req->msg;
  int sock = req->sock;

  logst(EVENT, "Received update file request from naming server, for %s", msg.data);

  char path[PATH_MAX];
  char localpath[PATH_MAX];
  char ip[INET_ADDRSTRLEN];
  int port;
  struct stat st;

  sprintf(path, "%s", msg.data);
  recv_tpx(req, sock, &msg, sizeof(msg), 0);
  strcpy(ip, msg.data);
  port = atoi(msg.data + 32);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tpx(req, ip);

  if (access(path, F_OK) != 0)
    add_prefix(localpath, path, ".backup/");
  else
    strcpy(localpath, path);

  if (stat(localpath, &st) < 0) {
    msg.type = UNAVAILABLE;
    goto ret_update_send;
  }

  logst(PROGRESS, "Connecting to %s:%d, for sending update of %s", ip, port, path);

  int ss_sock = socket_tpx(req, AF_INET, SOCK_STREAM, 0);
  req->newsock = ss_sock;
  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  int bytes = st.st_size;
  msg.type = UPDATE;
  sprintf(msg.data, "%s", path);
  send_tpx(req, ss_sock, &msg, sizeof(msg), 0);
  sprintf(msg.data, "%d", bytes);
  send_tpx(req, ss_sock, &msg, sizeof(msg), 0);

  recv_tpx(req, ss_sock, &msg, sizeof(msg), 0);
  if (msg.type != UPDATE + 1) {
    logst(FAILURE, "Update request rejected by %s:%d, for %s", ip, port, path);
    goto ret_update_send;
  }

  logst(PROGRESS, "Sending %d bytes to %s:%d, for %s", bytes, ip, port, path);

  int sent = 0;
  msg.type = UPDATE;
  FILE *file = fopen_tpx(req, localpath, "r");

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
  logst(COMPLETION, "Sent %d bytes to %s:%d, for %s", bytes, ip, port, path);

  msg.type = UPDATE + 1;

ret_update_send:
  send_tpx(req, sock, &msg, sizeof(msg), 0);
  reqfree(req);
  return NULL;
}
