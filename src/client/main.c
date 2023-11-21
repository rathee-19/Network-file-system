#include "defs.h"

int sock;
logfile_t* logfile;

int main(int argc, char *argv[])
{
#ifdef LOG
  if (argc < 2) {
    fprintf(stderr, "usage: %s <logfile>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  strcpy(logfile->path, argv[1]);
  pthread_mutex_init(&(logfile->lock), NULL);

#else
  logfile = NULL;

#endif

  while (1)
  {
    sock = socket_tx(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NSPORT);
    addr.sin_addr.s_addr = inet_addr_tx(NSIP);

    connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));

    printf("\n==> [R]ead [W]rite [P]hoto-Copy [C]reate [D]elete [I]nfo [L]ist [Q]uit\n");
    printf("==> Operation to request: ");

    char op;
    scanf(" %c", &op);
    op = toupper(op);
    
    switch (op)
    {
      case 'R':
        request_read(); break;
      case 'W':
        request_write(); break;
      case 'P':
        request_copy(); break;
      case 'C':
        request_create(); break;
      case 'D':
        request_delete(); break;
      case 'I':
        request_info(); break;
      case 'L':
        request_list(); break;
      case 'Q':
        request_invalid(); goto ret_main;
      default:
        request_invalid(); fprintf(stderr, "Invalid request\n");
    }

    close_tx(sock);
  }

ret_main:
  printf("\n");
  close_tx(sock);
  pthread_mutex_destroy(&(logfile->lock));
  free(logfile);
  return 0;
}

void request_read(void)
{
  message_t msg;
  msg.type = READ;
  bzero(msg.data, BUFSIZE);

  char path[PATH_MAX];
  printf("==> Path: ");
  scanf(" %[^\n]s", path);
  strcpy(msg.data, path);

  char localpath[PATH_MAX];
  printf("==> Local Path: ");
  scanf(" %[^\n]s", localpath);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  char ip[INET_ADDRSTRLEN];
  int port;

  switch (msg.type)
  {
    case READ + 1:
      strncpy(ip, msg.data, INET_ADDRSTRLEN);
      port = atoi(msg.data + 32);
      logc(logfile, PROGRESS, "Sending request to storage server %s:%d\n", ip, port);
      goto connect_read;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); return;
    case UNAVAILABLE:
      logc(logfile, FAILURE, "%s is unavailable currently\n", path); return;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for read\n"); return;
    default:
      invalid_response(msg.type); return;
  }

connect_read:
  int ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tx(ip);

  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = READ;
  strcpy(msg.data, path);
  send_tx(ss_sock, &msg, sizeof(msg), 0);
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  FILE* file;
  
  switch(msg.type)
  {
    case READ + 1:
      bytes = atoi(msg.data);
      logc(logfile, PROGRESS, "Receiving %d bytes from storage server %s:%d\n", bytes, ip, port);
      goto recv_read;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for read\n"); return;
    default:
      invalid_response(msg.type); return;
  }

recv_read:
  file = fopen_tx(localpath, "w+");
  int left = bytes;

  while (1)
  {
    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        goto ret_read;

      case READ + 1:
        if (left > BUFSIZE) {
          fwrite_tx(msg.data, sizeof(char), BUFSIZE, file);
          left -= BUFSIZE;
          continue;
        }
        else if (left > 0) {
          fwrite_tx(msg.data, sizeof(char), left, file);
          left = 0;
          continue;
        }

      default:
        invalid_response(msg.type); return;
    }
  }

ret_read:
  logc(logfile, COMPLETION, "Received %d bytes from storage server %s:%d\n", bytes, ip, port);
  fclose_tx(file);
  close_tx(ss_sock);
}

void request_write(void)
{
  message_t msg;
  msg.type = WRITE;
  bzero(msg.data, BUFSIZE);

  char path[PATH_MAX];
  printf("==> Path: ");
  scanf(" %[^\n]s", path);
  strcpy(msg.data, path);

  char localpath[PATH_MAX];
  printf("==> Local Path: ");
  scanf(" %[^\n]s", localpath);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  char ip[INET_ADDRSTRLEN];
  int port;

  switch (msg.type)
  {
    case WRITE + 1:
      strncpy(ip, msg.data, INET_ADDRSTRLEN);
      port = atoi(msg.data + 32);
      logc(logfile, PROGRESS, "Sending request to storage server %s:%d\n", ip, port);
      goto connect_write;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); return;
    case UNAVAILABLE:
      logc(logfile, FAILURE, "%s is unavailable currently\n", path); return;
    case BEING_READ:
      logc(logfile, FAILURE, "%s is being read currently\n", path); return;
    case RDONLY:
      logc(logfile, FAILURE, "%s has been marked read-only\n", path); return;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for write\n"); return;
    default:
      invalid_response(msg.type); return;
  }

connect_write:
  int ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr_tx(ip);

  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = WRITE;
  strcpy(msg.data, path);
  send_tx(ss_sock, &msg, sizeof(msg), 0);
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  FILE* file;
  struct stat st;
  
  switch(msg.type)
  {
    case WRITE + 1:
      goto send_write;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); return;
    case RDONLY:
      logc(logfile, FAILURE, "%s has been marked read-only\n", path); return;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for write\n"); return;
    default:
      invalid_response(msg.type); return;
  }

send_write:
  file = fopen_tx(localpath, "r");
  stat(localpath, &st);
  bytes = st.st_size;
  logc(logfile, PROGRESS, "Sending %d bytes to storage server %s:%d\n", bytes, ip, port);

  msg.type = WRITE;
  sprintf(msg.data, "%d", bytes);
  send_tx(ss_sock, &msg, sizeof(msg), 0);

  int sent = 0;
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

  msg.type = STOP;
  send_tx(ss_sock, &msg, sizeof(msg), 0);

  logc(logfile, COMPLETION, "Sent %d bytes to storage server %s:%d\n", bytes, ip, port);
  fclose_tx(file);
  close_tx(ss_sock);
}

void request_create(void)
{
  message_t msg;
  bzero(msg.data, BUFSIZE);

  char ch;
  printf("==> [F]ile [D]irectory: ");
  scanf(" %c", &ch);
  ch = toupper(ch);

  switch (ch)
  {
    case 'D':
      msg.type = CREATE_DIR; break;
    case 'F':
      msg.type = CREATE_FILE; break;
    default:
      fprintf(stderr, "Invalid request\n"); return;
  }

  printf("==> Path: ");
  scanf(" %[^\n]s", msg.data);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case CREATE_DIR + 1:
    case CREATE_FILE + 1:
      logc(logfile, COMPLETION, "%s was created\n", msg.data); break;
    case EXISTS:
      logc(logfile, FAILURE, "%s already exists\n", msg.data); break;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for create\n"); break;
    default:
      invalid_response(msg.type);
  }
}

void request_delete(void)
{
  message_t msg;
  msg.type = DELETE;
  bzero(msg.data, BUFSIZE);

  char path[PATH_MAX];
  printf("==> Path: ");
  scanf(" %[^\n]s", path);
  strcpy(msg.data, path);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case DELETE + 1:
      logc(logfile, COMPLETION, "%s was deleted\n", path); break;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); break;
    case UNAVAILABLE:
      logc(logfile, FAILURE, "%s is unavailable currently\n", path); return;
    case BEING_READ:
      logc(logfile, FAILURE, "%s is being read currently\n", path); return;
    case RDONLY:
      logc(logfile, FAILURE, "%s has been marked read-only\n", path); break;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for delete\n"); break;
    default:
      invalid_response(msg.type);
  }
}

void request_info(void)
{
  message_t msg;
  msg.type = INFO;
  bzero(msg.data, BUFSIZE);

  char path[PATH_MAX];
  printf("==> Path: ");
  scanf(" %[^\n]s", path);
  strcpy(msg.data, path);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  int bytes;
  metadata_t* info;

  switch (msg.type)
  {
    case INFO + 1:
      bytes = atoi(msg.data);
      logc(logfile, PROGRESS, "Receiving %d bytes from naming server\n", bytes);
      goto recv_info;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", path); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for info\n"); return;
    default:
      invalid_response(msg.type); return;
  }

recv_info:
  info = (metadata_t*) malloc(bytes);
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logc(logfile, COMPLETION, "Received %d bytes from naming server\n", bytes);
        goto ret_info;

      case INFO + 1:
        if (end >= ptr + BUFSIZE) {
          memcpy(ptr, msg.data, BUFSIZE);
          ptr += BUFSIZE;
          continue;
        }
        else if (end > ptr) {
          memcpy(ptr, msg.data, (void*) end - ptr);
          ptr = end;
          continue;
        }

      default:
        invalid_response(msg.type); return;
    }
  }

ret_info:
  char perms[11], mtime_str[80];
  get_permissions(perms, info->mode);
  strftime(mtime_str, sizeof(mtime_str), "%b %d %H:%M", localtime(&(info->mtime)));
  printf("%s %zu %s\n", perms, info->size, mtime_str);

  free(info);
}

void request_list(void)
{
  message_t msg;
  msg.type = LIST;

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  int bytes;
  metadata_t** info;

  switch (msg.type)
  {
    case LIST + 1:
      bytes = atoi(msg.data);
      logc(logfile, PROGRESS, "Receiving %d bytes from naming server\n", bytes);
      goto recv_list;
    default:
      invalid_response(msg.type); return;
  }

recv_list:
  info = (metadata_t**) malloc(bytes);
  void* ptr = info;
  void* end = ptr + bytes;

  while (1)
  {
    recv_tx(sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logc(logfile, COMPLETION, "Received %d bytes from naming server\n", bytes);
        goto ret_list;

      case LIST + 1:
        if (end >= ptr + BUFSIZE) {
          memcpy(ptr, msg.data, BUFSIZE);
          ptr += BUFSIZE;
          continue;
        }
        else if (end > ptr) {
          memcpy(ptr, msg.data, (void*) end - ptr);
          ptr = end;
          continue;
        }

      default:
        invalid_response(msg.type); return;
    }
  }

ret_list:
  metadata_t* file = (metadata_t*) info;
  while (end > (void*) file) {
    if (S_ISDIR(file->mode))
      printf(BLUE "%s" RESET "\n", file->path);
    else
      printf("%s\n", file->path);
    file++;
  }
  
  free(info);
}

void request_copy(void)
{
  message_t msg;
  msg.type = COPY;
  bzero(msg.data, BUFSIZE);

  char curpath[PATH_MAX];
  printf("==> Current Path: ");
  scanf(" %[^\n]s", curpath);

  char newpath[PATH_MAX];
  printf("==> New Path: ");
  scanf(" %[^\n]s", newpath);

  strcpy(msg.data, curpath);
  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case COPY + 1:
      goto send_copy;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", curpath); return;
    case UNAVAILABLE:
      logc(logfile, FAILURE, "%s is unavailable currently\n", curpath); return;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", curpath); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for copy\n"); return;
    default:
      invalid_response(msg.type); return;
  }

send_copy:
  strcpy(msg.data, newpath);
  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case COPY + 1:
      goto send_copy;
      logc(logfile, COMPLETION, "%s was copied to %s\n", curpath, newpath); break;
    case NOTFOUND:
      logc(logfile, FAILURE, "%s was not found\n", curpath); return;
    case UNAVAILABLE:
      logc(logfile, FAILURE, "%s is unavailable currently\n", curpath); return;
    case XLOCK:
      logc(logfile, FAILURE, "%s is being written to by a client\n", curpath); return;
    case PERM:
      logc(logfile, FAILURE, "Missing permissions for copy\n"); return;
    default:
      invalid_response(msg.type); return;
  }
}

void request_invalid(void)
{
  message_t msg;
  msg.type = INVALID;

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);
}

void invalid_response(int resp)
{
  logc(logfile, FAILURE, "Received an invalid response %d from the server\n", resp);
}
