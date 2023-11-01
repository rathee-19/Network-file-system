#include "api.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

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
