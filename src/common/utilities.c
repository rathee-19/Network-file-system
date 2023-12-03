#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
// #include <sys/syscall.h>
#include "api.h"
#include "colors.h"
#include "utilities.h"

extern logfile_t* logfile;

void timestamp(FILE* stream)
{
  time_t timer;
  char buffer[24];
  struct tm* tm_info;

  timer = time(NULL);
  tm_info = localtime(&timer);

  strftime(buffer, 24, "%Y-%m-%d %H:%M:%S", tm_info);
  fprintf(stream, "\033[90m" "%s " "\033[0m", buffer);
}

void logevent(enum caller c, enum level lvl, const char* message, ...)
{
  pthread_mutex_lock_tx(&(logfile->lock));
  va_list args;
  va_start(args, message);

  switch (lvl)
  {
    case STATUS:
      timestamp(stdout);
      fprintf(stdout, ORANGE);
      // fprintf(stdout, "[%ld] ", syscall(SYS_gettid));
      vfprintf(stdout, message, args);
      fprintf(stdout, RESET "\n");
      break;

    case EVENT:
      timestamp(stdout);
      fprintf(stdout, CYAN);
      // fprintf(stdout, "[%ld] ", syscall(SYS_gettid));
      vfprintf(stdout, message, args);
      fprintf(stdout, RESET "\n");
      break;

    case PROGRESS:
#ifdef DEBUG
      timestamp(stdout);
      fprintf(stdout, MAGENTA);
      // fprintf(stdout, "[%ld] ", syscall(SYS_gettid));
      vfprintf(stdout, message, args);
      fprintf(stdout, RESET "\n");
#endif
      break;

    case COMPLETION:
      timestamp(stdout);
      fprintf(stdout, GREEN);
      // fprintf(stdout, "[%ld] ", syscall(SYS_gettid));
      vfprintf(stdout, message, args);
      fprintf(stdout, RESET "\n");
      break;
    
    case FAILURE:
      timestamp(stderr);
      fprintf(stderr, RED);
      // fprintf(stderr, "[%ld] ", syscall(SYS_gettid));
      vfprintf(stderr, message, args);
      fprintf(stderr, RESET "\n");
      break;
  }

#ifdef LOG
  va_start(args, message);
  if (logfile->path[0] != 0) {
    FILE* outfile = fopen_tx(logfile->path, "a+");
    vfprintf(outfile, message, args);
    fclose(outfile);
  }
#endif

  va_end(args);
  pthread_mutex_unlock_tx(&(logfile->lock));
}

request_t* reqalloc(void)
{
  request_t* req = (request_t*) calloc(1, sizeof(request_t));
  if (req == 0)
    perror_tx("calloc");
  req->sock = -1;
  req->newsock = -1;
  req->addrlen = sizeof(req->addr);
  return req;
}

void reqfree(request_t* req)
{
  if (req->sock != -1)
    close(req->sock);
  if (req->newsock != -1)
    close(req->newsock);
  if (req->allocptr != NULL)
    free(req->allocptr);
  free(req);
}

void get_permissions(char* perms, mode_t mode)
{
  strcpy(perms, "----------");
  if (S_ISDIR(mode)) perms[0] = 'd';
  if (mode & S_IRUSR) perms[1] = 'r';
  if (mode & S_IWUSR) perms[2] = 'w';
  if (mode & S_IXUSR) perms[3] = 'x';
  if (mode & S_IRGRP) perms[4] = 'r';
  if (mode & S_IWGRP) perms[5] = 'w';
  if (mode & S_IXGRP) perms[6] = 'x';
  if (mode & S_IROTH) perms[7] = 'r';
  if (mode & S_IWOTH) perms[8] = 'w';
  if (mode & S_IXOTH) perms[9] = 'x';
}

void get_parent_dir(char* dest, char* src)
{
  *dest = 0;
  strcpy(dest, src);

  for (int i = strlen(dest) - 1; i >= 0; i--) {
    if (dest[i] == '/') {
      dest[i] = 0;
      return;
    }
  }

  dest[0] = 0;
}

int create_dir(char* path)
{
  struct stat st;
  char parent[PATH_MAX];
  get_parent_dir(parent, path);

  if (parent[0] == 0) {
    if (mkdir(path, 0777) == 0)
      return 0;
    else
      return -1;
  }

  if (stat(parent, &st) < 0) {
    if (create_dir(parent) == 0) {
      if (mkdir(path, 0777) == 0)
        return 0;
      else
        return -1;
    }
    return -1;
  }
  else if (S_ISDIR(st.st_mode)) {
    if (mkdir(path, 0777) == 0)
      return 0;
    else
      return -1;
  }
  else
    return -1;
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
  strcat(dest, src);
}
