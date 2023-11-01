#include <arpa/inet.h>
#include "../utilities.h"

in_addr_t inet_addr_tx(const char* cp)
{
  in_addr_t addr = inet_addr(cp);
  if (addr == (in_addr_t) -1)
    perror_tx("inet_addr");
  return addr;
}
