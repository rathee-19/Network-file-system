#include <unistd.h>
#include <sys/socket.h>
#include "../api.h"
#include "../utilities.h"

int socket_tx(int domain, int type, int protocol)
{
  int sock = socket(domain, type, protocol);
  if (sock < 0)
    perror_tx("sock");
  return sock;
}

int setsockopt_t(int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
  int ret = setsockopt(socket, level, option_name, option_value, option_len);
  if (ret < 0)
    perror_t("setsockopt");
  return ret;
}

int connect_t(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  while (connect(sockfd, addr, addrlen) < 0) {
    perror_t("connect");
    fprintf_t(stderr, "Retrying after %d seconds...\n", RETRY_DIFF);
    sleep(RETRY_DIFF);
  }
  return 0;
}

int connect_tb(int sockfd, const struct sockaddr* addr, socklen_t addrlen, int timeout)
{
  while ((timeout >= 0) && (connect(sockfd, addr, addrlen) < 0)) {
    perror_t("connect");
    fprintf_t(stderr, "Retrying after %d seconds...\n", RETRY_DIFF);
    sleep(RETRY_DIFF);
    timeout -= RETRY_DIFF;
  }
  if (timeout >= 0)
    return 0;
  else
    return 1;
}

void bind_tx(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  int ret = bind(sockfd, addr, addrlen);
  if (ret < 0)
    perror_tx("bind");
}

void listen_tx(int sockfd, int backlog)
{
  int ret = listen(sockfd, backlog);
  if (ret < 0)  
    perror_tx("listen");
}

int accept_tx(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
  int client_sock = accept(sockfd, addr, addrlen);
  if (client_sock < 0)
    perror_tx("accept");
  return client_sock;
}

ssize_t recv_tx(int sockfd, void* buf, size_t len, int flags)
{
  int __len = len;
  while (len > 0) {
    ssize_t ret = recv(sockfd, buf, len, flags);
    if (ret < 0)
      perror_tx("recv");
    buf += ret;
    len -= ret;
  }
  return __len;
}

ssize_t send_tx(int sockfd, void* buf, size_t len, int flags)
{
  int __len = len;
  while (len > 0) {
    ssize_t ret = send(sockfd, buf, len, flags);
    if (ret < 0)
      perror_tx("send");
    buf += ret;
    len -= ret;
  }
  return __len;
}
