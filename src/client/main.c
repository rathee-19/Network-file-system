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
  pthread_mutex_init_tx(&(logfile->lock), NULL);

#else
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  logfile->path[0] = 0;
  pthread_mutex_init_tx(&(logfile->lock), NULL);

#endif

  signal_tx(SIGPIPE, SIG_IGN);

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
      logc(PROGRESS, "Sending request to storage server %s:%d", ip, port);
      goto connect_read;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path); return;
    case UNAVAILABLE:
      logc(FAILURE, "%s is unavailable currently", path); return;
    case XLOCK:
      logc(FAILURE, "%s is being written to by a client", path); return;
    case PERM:
      logc(FAILURE, "Missing permissions for read"); return;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type); return;
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
  FILE* file = fopen_tx(localpath, "w+");
  
  switch(msg.type)
  {
    case READ + 1:
      bytes = atoi(msg.data);
      logc(PROGRESS, "Receiving %d bytes from storage server %s:%d", bytes, ip, port);
      goto recv_read;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path);
      goto ret_read;
    case PERM:
      logc(FAILURE, "Missing permissions for read");
      goto ret_read;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type);
      goto ret_read;
  }

recv_read:
  int left = bytes;
  while (1)
  {
    recv_tx(ss_sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        logc(COMPLETION, "Received %d bytes from storage server %s:%d", bytes, ip, port);
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
        logc(FAILURE, "Received an invalid response %d from the server", msg.type);
        goto ret_read;
    }
  }

ret_read:
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
      logc(PROGRESS, "Sending request to storage server %s:%d", ip, port);
      goto connect_write;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path); return;
    case UNAVAILABLE:
      logc(FAILURE, "%s is unavailable currently", path); return;
    case BEING_READ:
      logc(FAILURE, "%s is being read currently", path); return;
    case RDONLY:
      logc(FAILURE, "%s has been marked read-only", path); return;
    case XLOCK:
      logc(FAILURE, "%s is being written to by a client", path); return;
    case PERM:
      logc(FAILURE, "Missing permissions for write"); return;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type); return;
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

  FILE* file = fopen_tx(localpath, "r");
  struct stat st;
  stat(localpath, &st);
  int bytes = st.st_size;
  
  switch(msg.type)
  {
    case WRITE + 1:
      logc(PROGRESS, "Sending %d bytes to storage server %s:%d", bytes, ip, port);
      goto send_write;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path);
      goto ret_write;
    case RDONLY:
      logc(FAILURE, "%s has been marked read-only", path);
      goto ret_write;
    case XLOCK:
      logc(FAILURE, "%s is being written to by a client", path);
      goto ret_write;
    case PERM:
      logc(FAILURE, "Missing permissions for write");
      goto ret_write;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type);
      goto ret_write;
  }

send_write:
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
  logc(COMPLETION, "Sent %d bytes to storage server %s:%d", bytes, ip, port);
  
ret_write:
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

  char path[PATH_MAX];
  printf("==> Path: ");
  scanf(" %[^\n]s", path);
  strcpy(msg.data, path);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case CREATE_DIR + 1:
    case CREATE_FILE + 1:
      logc(COMPLETION, "%s was created", path); break;
    case EXISTS:
      logc(FAILURE, "%s already exists", path); break;
    case NOTFOUND:
      logc(FAILURE, "Parent directory of %s was not found", path); break;
    case PERM:
      logc(FAILURE, "Missing permissions for create"); break;
    case UNAVAILABLE:
      logc(FAILURE, "The server was unavailable"); break;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type);
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
      logc(COMPLETION, "%s was deleted", path); break;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path); break;
    case UNAVAILABLE:
      logc(FAILURE, "%s is unavailable currently", path); break;
    case BEING_READ:
      logc(FAILURE, "%s is being read currently", path); break;
    case RDONLY:
      logc(FAILURE, "%s has been marked read-only", path); break;
    case XLOCK:
      logc(FAILURE, "%s is being written to by a client", path); break;
    case PERM:
      logc(FAILURE, "Missing permissions for delete"); break;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type);
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
      logc(PROGRESS, "Receiving %d bytes from naming server", bytes);
      goto recv_info;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", path); return;
    case PERM:
      logc(FAILURE, "Missing permissions for info"); return;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type); return;
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
        logc(COMPLETION, "Received %d bytes from naming server", bytes);
        goto print_info;

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
        logc(FAILURE, "Received an invalid response %d from the server", msg.type);
        goto ret_info;
    }
  }

print_info:
  char perms[11], mtime_str[80];
  get_permissions(perms, info->mode);
  strftime(mtime_str, sizeof(mtime_str), "%b %d %H:%M", localtime(&(info->mtime)));
  printf("%s %zu %s\n", perms, info->size, mtime_str);

ret_info:
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
      logc(PROGRESS, "Receiving %d bytes from naming server", bytes);
      goto recv_list;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type); return;
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
        logc(COMPLETION, "Received %d bytes from naming server", bytes);
        goto print_list;

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
        logc(FAILURE, "Received an invalid response %d from the server", msg.type);
        goto ret_list;
    }
  }

print_list:
  metadata_t* file = (metadata_t*) info;
  while (end > (void*) file) {
    if (S_ISDIR(file->mode))
      printf(BLUE "%s" RESET "\n", file->path);
    else
      printf("%s\n", file->path);
    file++;
  }
  
ret_list:
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
  strcpy(msg.data, newpath);
  send_tx(sock, &msg, sizeof(msg), 0);
  
  recv_tx(sock, &msg, sizeof(msg), 0);
  switch (msg.type)
  {
    case COPY + 1:
      logc(COMPLETION, "%s was copied to %s", curpath, newpath); break;
    case NOTFOUND:
      logc(FAILURE, "%s was not found", curpath); break;
    case UNAVAILABLE:
      logc(FAILURE, "%s is unavailable currently", curpath); break;
    case XLOCK:
      logc(FAILURE, "%s is being written to by a client", curpath); break;
    case PERM:
      logc(FAILURE, "Missing permissions for copy"); break;
    default:
      logc(FAILURE, "Received an invalid response %d from the server", msg.type); break;
  }
}

void request_invalid(void)
{
  message_t msg;
  msg.type = INVALID;

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);
}
