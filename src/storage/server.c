#include "defs.h"

char root_dir[PATH_MAX];

void nsnotify(int nsport, int clport, int stport)
{
  int sock = socket_tx(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = NSPORT;
  addr.sin_addr.s_addr = inet_addr_tx(NSIP);

  connect_t(sock, (struct sockaddr*) &addr, sizeof(addr));
  fprintf_t(stdout, "Connected to the naming server.\n");

  int bytes = 0;
  sprintf(root_dir, "%d", nsport);                    // assumption
  void* paths = (void*) dirinfo(root_dir, &bytes);

  message_t msg;
  msg.type = STORAGE_JOIN;
  bzero(msg.data, BUFSIZE);
  sprintf(msg.data, "%s", IP);
  sprintf(msg.data + 32, "%d", nsport);
  sprintf(msg.data + 64, "%d", clport);
  sprintf(msg.data + 96, "%d", stport);
  sprintf(msg.data + 128, "%d", bytes);

  send_tx(sock, &msg, sizeof(msg), 0);

  int ret, sent = 0;
  while (sent < bytes)
  {
    if (bytes - sent >= BUFSIZE)
    {
      memcpy(msg.data, paths + sent, BUFSIZE);
      ret = send_tx(sock, &msg, sizeof(msg), 0);
      sent += ret;
    }
    else
    {
      bzero(msg.data, BUFSIZE);
      memcpy(msg.data, paths + sent, bytes - sent);
      ret = send_tx(sock, &msg, sizeof(msg), 0);
      sent += ret;
    }
  }

  msg.type = STOP;
  send_tx(sock, &msg, sizeof(msg), 0);

#ifdef DEBUG
  fprintf_t(stdout, "Sent %d bytes to the naming server.\n", bytes);
#endif

  recv_tx(sock, &msg, sizeof(msg), 0);

  if (msg.type != STORAGE_JOIN + 1) {
    fprintf_t(stderr, "Failed to register with the naming server.\n");
    exit(1);
  }

  close_tx(sock);

  pthread_t server, client, storage;
  pthread_create_tx(&server, NULL, nslisten, &nsport);
  pthread_create_tx(&client, NULL, cllisten, &clport);
  pthread_create_tx(&storage, NULL, stlisten, &stport);

  fprintf_t(stdout, "Started the listener threads.\n");
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
  addr.sin_port = port;
  addr.sin_addr.s_addr = inet_addr_tx(IP);

  bind_tx(sock, (struct sockaddr*) &addr, sizeof(addr));
  listen_tx(sock, 2);
  
  int client_sock;
  struct sockaddr_in client_addr;
  socklen_t addr_size = sizeof(client_addr);
  message_t buffer;

  while (1)
  {
    client_sock = accept_tx(sock, (struct sockaddr*) &client_addr, &addr_size);
    recv_tx(client_sock, &buffer, sizeof(buffer), 0);

    switch (buffer.type)
    {
      case CREATE_DIR:
      case CREATE_FILE:
      case DELETE:
      case COPY:
      default:
    }
  }

  return NULL;
}

metadata_t** dirinfo(char* cdir, int* bytes)
{
  DIR *dir;
  while ((dir = opendir(cdir)) == NULL) {
    if (errno == ENOENT && cdir != NULL) {                              // create dir if does not exist
      if (mkdir(cdir, 0755) < 0)
        perror_tx("mkdir");
    }
    else
      perror_tx("opendir");
  }
  closedir(dir);

  int idx = 0, num = countfiles(cdir);                 // TODO: try dynamic array with realloc or something

#ifdef DEBUG
  fprintf_t(stdout, "Found %d files in the root directory.\n", num);
#endif

  metadata_t** data = (metadata_t**) calloc(num, sizeof(metadata_t));
  *bytes = num * sizeof(metadata_t);
  popdirinfo(cdir, data, &idx, 0);
  
#ifdef DEBUG
  fprintf_t(stdout, "Retrieved metadata of all files.\n");
#endif

  return data;
}

void popdirinfo(char* cdir, metadata_t** data, int* idx, int level)
{
  DIR *dir = opendir(cdir);
  if (dir == NULL) {
    perror_t("opendir");
    return;
  }
  
  struct dirent *entry = readdir(dir);
  char pathbuf[PATH_MAX];
  struct stat statbuf;

  while (entry != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      entry = readdir(dir);
      continue;
    }

    sprintf(pathbuf, "%s/%s", cdir, entry->d_name);
    if (stat(pathbuf, &statbuf) == -1) {
      perror_t("stat");
      entry = readdir(dir);
      continue;
    }

    metadata_t *info = (metadata_t*) (data + *idx);
    strcpy(info->path, pathbuf);                                              // TODO: remove root_dir/ prefix
    if (level != 0)
      strcpy(info->parent, cdir);
    info->mode = statbuf.st_mode;
    info->size = statbuf.st_size;
    info->ctime = statbuf.st_ctime;
    info->mtime = statbuf.st_mtime;
    (*idx)++;

    if (S_ISDIR(statbuf.st_mode))
      popdirinfo(pathbuf, data, idx, level + 1);

    entry = readdir(dir);
  }

  closedir(dir);
}

int countfiles(char* cdir)
{
  int ctr = 0;
  DIR *dir = opendir(cdir);
  if (dir == NULL) {
    perror_t("opendir");
    return 0;
  }
  
  struct dirent *entry = readdir(dir);
  char pathbuf[PATH_MAX];
  struct stat statbuf;

  while (entry != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      entry = readdir(dir);
      continue;
    }

    sprintf(pathbuf, "%s/%s", cdir, entry->d_name);
    if (stat(pathbuf, &statbuf) == -1) {
      perror_t("stat");
      entry = readdir(dir);
      continue;
    }

    ctr += 1;

    if (S_ISDIR(statbuf.st_mode))
      ctr += countfiles(pathbuf);

    entry = readdir(dir);
  }

  closedir(dir);
  return ctr;
}
