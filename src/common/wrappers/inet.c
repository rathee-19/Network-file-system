#include <arpa/inet.h>
#include "../utilities.h"

in_addr_t inet_addr_tx(const char* cp)
{
  in_addr_t addr = inet_addr(cp);
  if (addr == (in_addr_t) -1)
    perror_tx("inet_addr");
  return addr;
}

in_addr_t inet_addr_tpx(request_t* req, const char* cp)
{
  in_addr_t addr = inet_addr(cp);
  if (addr == (in_addr_t) -1)
    perror_tpx(req, "inet_addr");
  return addr;
}

const char* inet_ntop_tx(int af, const void *restrict src, char* dst, socklen_t size)
{
  const char* ret = inet_ntop(af, src, dst, size);
  if (ret == NULL)
    perror_tx("inet_ntop");
  return ret;
}

const char* inet_ntop_tpx(request_t* req, int af, const void *restrict src, char* dst, socklen_t size)
{
  const char* ret = inet_ntop(af, src, dst, size);
  if (ret == NULL)
    perror_tpx(req, "inet_ntop");
  return ret;
}
