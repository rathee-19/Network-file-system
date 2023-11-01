#include <stdio.h>
#include <stdarg.h>
#include "../utilities.h"

void fprintf_t(FILE* stream, const char* format, ...)
{
  va_list args;
  timestamp(stream);
  va_start(args, format);
  vfprintf(stream, format, args);
  va_end(args);
}

FILE* fopen_tx(const char *restrict pathname, const char *restrict mode)
{
  FILE* file = fopen(pathname, mode);
  if (file == NULL)
    perror_tx("fopen");
  return file;
}

size_t fwrite_tx(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fwrite(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tx("fwrite");                                                   // TODO: look at all executions leading to this error
  return ret;
}

size_t fread_tx(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fread(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tx("fread");
  return ret;
}

int fclose_tx(FILE* stream)
{
  int ret = fclose(stream);
  if (ret != 0)
    perror_tx("close");
  return ret;
}
