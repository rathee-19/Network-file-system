#include <stdio.h>
#include <stdarg.h>
#include "../utilities.h"

int fprintf_t(FILE* stream, const char* format, ...)
{
  int ret;
  va_list args;
  timestamp(stream);
  va_start(args, format);
  ret = vfprintf(stream, format, args);
  va_end(args);
  return ret;
}

FILE* fopen_tx(const char *restrict pathname, const char *restrict mode)
{
  FILE* file = fopen(pathname, mode);
  if (file == NULL)
    perror_tx("fopen");
  return file;
}

FILE* fopen_tpx(request_t* req, const char *restrict pathname, const char *restrict mode)
{
  FILE* file = fopen(pathname, mode);
  if (file == NULL)
    perror_tpx(req, "fopen");
  return file;
}

size_t fread_tx(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fread(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tx("fread");
  return ret;
}

size_t fread_tpx(request_t* req, void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fread(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tpx(req, "fread");
  return ret;
}

size_t fwrite_tx(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fwrite(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tx("fwrite");
  return ret;
}

size_t fwrite_tpx(request_t* req, const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream)
{
  int ret = fwrite(ptr, size, nitems, stream);
  if (ret != nitems)
    perror_tpx(req, "fwrite");
  return ret;
}

int fclose_tx(FILE* stream)
{
  int ret = fclose(stream);
  if (ret != 0)
    perror_tx("close");
  return ret;
}

int fclose_tpx(request_t* req, FILE* stream)
{
  int ret = fclose(stream);
  if (ret != 0)
    perror_tpx(req, "close");
  return ret;
}
