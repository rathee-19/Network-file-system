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
  logst(STATUS, "Connected to the naming server");

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
  logst(PROGRESS, "Sent %d bytes to the naming server", bytes);

  recv_tx(sock, &msg, sizeof(msg), 0);
  if (msg.type != JOIN + 1) {
    logst(FAILURE, "Failed to register with the naming server");
    exit(1);
  }
  logst(COMPLETION, "Registered with the naming server");

  close_tx(sock);

  pthread_t server, client, storage;
  pthread_create_tx(&server, NULL, nslisten, &nsport);
  pthread_create_tx(&client, NULL, cllisten, &clport);
  pthread_create_tx(&storage, NULL, stlisten, &stport);

  logst(STATUS, "Started the listener threads");
  
  pthread_join(server, NULL);
  pthread_join(client, NULL);
  pthread_join(storage, NULL);
}

metadata_t** dirinfo(char* paths, int n, int* bytes)
{
  int files = 0;
  for (int i = 0; i < n; i++)
    files += countfiles(paths + (i * PATH_MAX));
  logst(PROGRESS, "Found %d files in the accessible paths", files);

  metadata_t** data = (metadata_t**) calloc(files, sizeof(metadata_t));
  *bytes = files * sizeof(metadata_t);

  int idx = 0;
  for (int i = 0; i < n; i++)
    popdirinfo(paths + (i * PATH_MAX), data, &idx);
  logst(PROGRESS, "Retrieved metadata of all files");

  return data;
}

void popdirinfo(char* cdir, metadata_t** data, int* idx)
{
  DIR *dir = opendir(cdir);
  if (dir == NULL) {
    perror_t("opendir");
    return;
  }
  
  metadata_t* info;
  struct dirent *entry = readdir(dir);
  char pathbuf[PATH_MAX];
  struct stat statbuf;

  if (stat(cdir, &statbuf) == -1) {
    perror_t("stat");
    return;
  }

  info = (metadata_t*) ((void*) data + ((*idx) * sizeof(metadata_t)));
  strcpy(info->path, cdir);
  info->mode = statbuf.st_mode;
  info->size = statbuf.st_size;
  info->ctime = statbuf.st_ctime;
  info->mtime = statbuf.st_mtime;
  (*idx)++;

  while (entry != NULL)
  {
    if (entry->d_name[0] == '.') {
      entry = readdir(dir);
      continue;
    }

    sprintf(pathbuf, "%s/%s", cdir, entry->d_name);
    if (stat(pathbuf, &statbuf) == -1) {
      perror_t("stat");
      entry = readdir(dir);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
      popdirinfo(pathbuf, data, idx);
    else {
      info = (metadata_t*) ((void*) data + ((*idx) * sizeof(metadata_t)));
      strcpy(info->path, pathbuf);
      info->mode = statbuf.st_mode;
      info->size = statbuf.st_size;
      info->ctime = statbuf.st_ctime;
      info->mtime = statbuf.st_mtime;
      (*idx)++;
    }

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
  ctr += 1;

  while (entry != NULL)
  {
    if (entry->d_name[0] == '.') {
      entry = readdir(dir);
      continue;
    }

    sprintf(pathbuf, "%s/%s", cdir, entry->d_name);
    if (stat(pathbuf, &statbuf) == -1) {
      perror_t("stat");
      entry = readdir(dir);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
      ctr += countfiles(pathbuf);
    else
      ctr += 1;

    entry = readdir(dir);
  }

  closedir(dir);
  return ctr;
}
