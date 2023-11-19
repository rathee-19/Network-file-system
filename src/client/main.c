#include "defs.h"

int sock;

int main(void)
{
  sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = NSPORT;
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  printf("Waiting for the naming server.\n");
  connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));
  printf("Connected to the server.\n");

  char op;

  while (1)
  {
    printf("\n==> [R]ead [W]rite [C]reate [D]elete [I]nfo [L]ist [P]hotocopy [Q]uit\n");
    printf("==> Operation to request: ");
    scanf(" %c", &op);
    op = toupper(op);
    
    switch (op)
    {
      case 'R':
        request_read(); break;
      case 'W':
        request_write(); break;
      case 'C':
        request_create(); break;
      case 'D':
        request_delete(); break;
      case 'I':
        request_info(); break;
      case 'L':
        request_list(); break;
      case 'P':
        request_copy(); break;
      case 'Q':
        goto ret_main;
      default:
        printf("Invalid message\n");
        continue;
    }
  }

ret_main:
  close_tx(sock);
  printf("\nDisconnected from the server.\n");
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

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  char ip[INET_ADDRSTRLEN];
  int port;

  switch (msg.type)
  {
    case READ + 1:
      strncpy(ip, msg.data, INET_ADDRSTRLEN);
      port = atoi(msg.data + 32);
      goto connect_read;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;             // ????????????????????
    default:
      invalid_response(msg.type); return;
  }

connect_read:
  int ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = port;
  addr.sin_addr.s_addr = inet_addr_tx(ip);

  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = READ;
  strcpy(msg.data, path);
  send_tx(ss_sock, &msg, sizeof(msg), 0);
#ifdef DEBUG
  fprintf_t(stdout, "Request sent to the storage server %s/%d\n", ip, port);
#endif
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  FILE* file;
  
  switch(msg.type)
  {
    case READ + 1:
      bytes = atoi(msg.data + 32);
      goto recv_read;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;      
                                                              // TODO: inform the naming server of the storage's mischief ??
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;
    default:
      invalid_response(msg.type); return;
  }

recv_read:
  file = fopen_tx(path, "w+");                            // TODO: make sure that the storage server does not panic
                                                          //       because the poor client couldn't create a damn file
#ifdef DEBUG
  fprintf_t(stdout, "Receiving %d bytes from storage server %s/%d\n", bytes, ip, port);
#endif

  while (1)
  {
    recv_tx(sock, &msg, sizeof(msg), 0);
    switch (msg.type)
    {
      case STOP:
        goto ret_read;

      case READ + 1:
        if (bytes > BUFSIZE)
        {
          fwrite_tx(msg.data, sizeof(char), BUFSIZE, file);
          bytes -= BUFSIZE;
          continue;
        }
        else if (bytes > 0)
        {
          fwrite_tx(msg.data, sizeof(char), bytes, file);
          bytes = 0;
          continue;
        }

      default:
        invalid_response(msg.type); return;
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
      goto connect_write;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;
    case RDONLY:
    case XLOCK:
      fprintf(stderr, "%s has been marked read-only.\n", path); break;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;
    default:
      invalid_response(msg.type); return;
  }

connect_write:
  int ss_sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = port;
  addr.sin_addr.s_addr = inet_addr_tx(ip);

  connect_t(ss_sock, (struct sockaddr*) &addr, sizeof(addr));

  msg.type = WRITE;
  strcpy(msg.data, path);
  send_tx(ss_sock, &msg, sizeof(msg), 0);
#ifdef DEBUG
  fprintf_t(stdout, "Request sent to the storage server %s/%d\n", ip, port);
#endif
  recv_tx(ss_sock, &msg, sizeof(msg), 0);

  int bytes;
  FILE* file;
  struct stat st;
  
  switch(msg.type)
  {
    case WRITE + 1:
      goto recv_write;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;      
                                                              // TODO: inform the naming server of the storage's mischief ??
    case RDONLY:
    case XLOCK:
      fprintf(stderr, "%s has been marked read-only.\n", path); return;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;
    default:
      invalid_response(msg.type); return;
  }

recv_write:
  file = fopen_tx(localpath, "r");                            // TODO: make sure that the storage server does not panic
                                                         //       because the poor client couldn't create a damn file
  stat(localpath, &st);
  bytes = st.st_size;

#ifdef DEBUG
  fprintf_t(stdout, "Sending %d bytes from storage server %s/%d\n", bytes, ip, port);
#endif

  msg.type = WRITE;
  sprintf(msg.data, "%d", bytes);
  send_tx(sock, &msg, sizeof(msg), 0);

  int ret, sent = 0;
  while (sent < bytes)
  {
    if (bytes - sent >= BUFSIZE)
    {
      fread_tx(msg.data, sizeof(char), BUFSIZE, file);
      ret = send_tx(sock, &msg, sizeof(msg), 0);
      sent += ret;
    }
    else
    {
      bzero(msg.data, BUFSIZE);
      fread_tx(msg.data, sizeof(char), bytes - sent, file);
      ret = send_tx(sock, &msg, sizeof(msg), 0);
      sent += ret;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case WRITE + 1:
      goto ret_write;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;      
                                                              // TODO: inform the naming server of the storage's mischief ??
    case RDONLY:
    case XLOCK:
      fprintf(stderr, "%s has been marked read-only.\n", path); return;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;
    default:
      invalid_response(msg.type); return;
  }

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
      return;
  }

  printf("==> Path: ");
  scanf(" %[^\n]s", msg.data);

  send_tx(sock, &msg, sizeof(msg), 0);
  recv_tx(sock, &msg, sizeof(msg), 0);

  switch (msg.type)
  {
    case CREATE_DIR + 1:
    case CREATE_FILE + 1:
      printf("%s was created.\n", msg.data); break;
    case EXISTS:
      fprintf(stderr, "%s already exists.\n", msg.data); break;
    case PERM:
      fprintf(stderr, "Missing permissions for create.\n"); break;
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
      printf("%s was deleted.\n", path); break;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); break;
    case RDONLY:
    case XLOCK:
      fprintf(stderr, "%s has been marked read-only.\n", path); break;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); break;
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
  metadata_t *info;

  switch (msg.type)
  {
    case INFO + 1:
      bytes = atoi(msg.data);
      goto recv_info;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", path); return;
    case PERM:
      fprintf(stderr, "Missing permissions for delete.\n"); return;
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
        goto ret_info;

      case INFO + 1:
        if (end >= ptr + sizeof(msg) - sizeof(int32_t))
        {
          memcpy(ptr, msg.data, BUFSIZE);
          ptr += sizeof(msg);
          continue;
        }
        else if (end >= ptr)
        {
          memcpy(ptr, msg.data, (void*) end - ptr + 1);
          ptr = (void*) end + 1;
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
  // TODO: after setting up the data structure
}

void request_copy(void) {
  message_t srcmsg;
  srcmsg.type = COPY;
  bzero(srcmsg.data, BUFSIZE);

  char srcpath[PATH_MAX];
  printf("==> Source Path: ");
  scanf(" %[^\n]s", srcpath);
  strcpy(srcmsg.data, srcpath);

  send_tx(sock, &srcmsg, sizeof(srcmsg), 0);
  #ifdef DEBUG
  printf("Sent src path message to naming server\n");
  #endif
  recv_tx(sock, &srcmsg, sizeof(srcmsg), 0);

  switch(srcmsg.type) {
    case COPY+1:
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", srcpath); break;
    default:
      invalid_response(srcmsg.type);
  }

  message_t destmsg;
  destmsg.type = READ;
  bzero(destmsg.data, BUFSIZE);

  char destpath[PATH_MAX];
  printf("==> Destnation Path: ");
  scanf(" %[^\n]s", destpath);
  strcpy(destmsg.data, destpath);

  send_tx(sock, &destmsg, sizeof(destmsg), 0);
  #ifdef DEBUG
  printf("Sent dest path message to naming server\n");
  #endif
  recv_tx(sock, &destmsg, sizeof(destmsg), 0);

  switch(destmsg.type) {
    case COPY+1:
      break;
    case NOTFOUND:
      fprintf(stderr, "%s was not found.\n", destpath); break;
    default:
      invalid_response(destmsg.type);
  }

  message_t flagmsg;
  flagmsg.type = READ;
  bzero(flagmsg.data, BUFSIZE);

  char flag[BUFSIZE];
  snprintf(flag, "%s", "COPY_COND");

  send_tx(sock, &flagmsg, sizeof(flagmsg), 0);
  #ifdef DEBUG
  printf("Sent flag message to naming server\n");
  #endif
  recv_tx(sock, &flagmsg, sizeof(flagmsg), 0);

  switch(flagmsg.type) {
    case COPY+1:
      break;
    default:
      invalid_response(flagmsg.type);
  }
}

void invalid_response(int resp)
{
  // TODO: track all possible executions arriving here
  fprintf(stderr, "Received an invalid response from the server.\n");
}
