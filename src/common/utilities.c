#include "api.h"
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "utilities.h"

void get_permissions(char *perms, mode_t mode)
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

void logevent(enum caller c, logfile_t* logfile, enum level lvl, const char* message, ...)
{
  pthread_mutex_lock(&(logfile->lock));
  va_list args;
  va_start(args, message);

  switch (lvl)
  {
    case STATUS:
      timestamp(stdout);
      vfprintf(stdout, message, args);
      break;

    case EVENT:
      timestamp(stdout);
      vfprintf(stdout, message, args);
      break;

    case PROGRESS:
#ifdef DEBUG
      timestamp(stdout);
      vfprintf(stdout, message, args);
#endif
      break;

    case COMPLETION:
      timestamp(stdout);
      vfprintf(stdout, message, args);
      break;
    
    case FAILURE:
      timestamp(stderr);
      vfprintf(stderr, message, args);
      break;
  }

#ifdef LOG
  va_start(args, message);
  if (logfile != NULL) {
    FILE* outfile = fopen_tx(logfile->path, "a+");
    vfprintf(outfile, message, args);
    fclose(outfile);
  }
#endif

  va_end(args);
  pthread_mutex_unlock(&(logfile->lock));
}
