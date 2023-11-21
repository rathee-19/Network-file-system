#include "defs.h"

extern logfile_t* logfile;

metadata_t** dirinfo(char* paths, int n, int* bytes)
{
  int files = 0;
  for (int i = 0; i < n; i++)
    files += countfiles(paths + (i * PATH_MAX));
  logst(logfile, PROGRESS, "Found %d files in the root directory.\n", files);

  metadata_t** data = (metadata_t**) calloc(files, sizeof(metadata_t));
  *bytes = files * sizeof(metadata_t);

  int idx = 0;
  for (int i = 0; i < n; i++)
    popdirinfo(paths + (i * PATH_MAX), data, &idx);
  logst(logfile, PROGRESS, "Retrieved metadata of all files.\n");

  return data;
}

void popdirinfo(char* cdir, metadata_t** data, int* idx)
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

    metadata_t *info = (metadata_t*) ((void*) data + ((*idx) * sizeof(metadata_t)));
    strcpy(info->path, pathbuf);
    info->mode = statbuf.st_mode;
    info->size = statbuf.st_size;
    info->ctime = statbuf.st_ctime;
    info->mtime = statbuf.st_mtime;
    (*idx)++;

    if (S_ISDIR(statbuf.st_mode))
      popdirinfo(pathbuf, data, idx);

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

    ctr += 1;
    if (S_ISDIR(statbuf.st_mode))
      ctr += countfiles(pathbuf);

    entry = readdir(dir);
  }

  closedir(dir);
  return ctr;
}

void remove_prefix(char* dest, char* src, char* prefix)
{
  *dest = 0;
  while (*src && *prefix && (*src == *prefix)) {
    src++;
    prefix++;
  }
  if (*src == '/')
    src++;
  strcat(dest, src);
}

void add_prefix(char* dest, char* src, char* prefix)
{
  *dest = 0;
  strcat(dest, prefix);
  strcat(dest, "/");
  strcat(dest, src);
}
