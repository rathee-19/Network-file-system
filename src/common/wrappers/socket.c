#include <unistd.h>
#include <sys/socket.h>
#include "../api.h"
#include "../colors.h"
#include "../utilities.h"

extern logfile_t* logfile;

int socket_tx(int domain, int type, int protocol)
{
  int sock = socket(domain, type, protocol);
  if (sock < 0)
    perror_tx("sock");
  return sock;
}

int socket_tpx(request_t* req, int domain, int type, int protocol)
{
  int sock = socket(domain, type, protocol);
  if (sock < 0)
    perror_tpx(req, "sock");
  return sock;
}

int setsockopt_t(int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
  int ret = setsockopt(socket, level, option_name, option_value, option_len);
  if (ret < 0)
    perror_t("setsockopt");
  return ret;
}

int setsockopt_tx(int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
  int ret = setsockopt(socket, level, option_name, option_value, option_len);
  if (ret < 0)
    perror_tx("setsockopt");
  return ret;
}

int setsockopt_tpx(request_t* req, int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
  int ret = setsockopt(socket, level, option_name, option_value, option_len);
  if (ret < 0)
    perror_tpx(req, "setsockopt");
  return ret;
}

int connect_t(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  while (connect(sockfd, addr, addrlen) < 0) {
    perror_t("connect");
    pthread_mutex_lock(&(logfile->lock));
    fprintf_t(stderr, RED "connect: retrying after %d seconds..." RESET "\n", RETRY_DIFF);
    pthread_mutex_unlock(&(logfile->lock));
    sleep(RETRY_DIFF);
  }
  return 0;
}

int connect_tb(int sockfd, const struct sockaddr* addr, socklen_t addrlen, int timeout)
{
  while ((timeout >= 0) && (connect(sockfd, addr, addrlen) < 0)) {
    perror_t("connect");
    pthread_mutex_lock(&(logfile->lock));
    fprintf_t(stderr, RED "connect: retrying after %d seconds..." RESET "\n", RETRY_DIFF);
    pthread_mutex_unlock(&(logfile->lock));
    sleep(RETRY_DIFF);
    timeout -= RETRY_DIFF;
  }
  if (timeout >= 0)
    return 0;
  else
    return 1;
}

int bind_t(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  while (bind(sockfd, addr, addrlen) < 0) {
    perror_t("bind");
    pthread_mutex_lock(&(logfile->lock));
    fprintf_t(stderr, RED "bind: retrying after %d seconds..." RESET "\n", RETRY_DIFF);
    pthread_mutex_unlock(&(logfile->lock));
    sleep(RETRY_DIFF);
  };
  return 0;
}

int listen_tx(int sockfd, int backlog)
{
  int ret = listen(sockfd, backlog);
  if (ret < 0)  
    perror_tx("listen");
  return ret;
}

int listen_tpx(request_t* req, int sockfd, int backlog)
{
  int ret = listen(sockfd, backlog);
  if (ret < 0)  
    perror_tpx(req, "listen");
  return ret;
}

int accept_tx(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
  int client_sock = accept(sockfd, addr, addrlen);
  if (client_sock < 0)
    perror_tx("accept");
  return client_sock;
}

int accept_tpx(request_t* req, int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
  int client_sock = accept(sockfd, addr, addrlen);
  if (client_sock < 0)
    perror_tpx(req, "accept");
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

ssize_t recv_tpx(request_t* req, int sockfd, void* buf, size_t len, int flags)
{
  int __len = len;
  while (len > 0) {
    ssize_t ret = recv(sockfd, buf, len, flags);
    if (ret < 0)
      perror_tpx(req, "recv");
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

ssize_t send_tpx(request_t* req, int sockfd, void* buf, size_t len, int flags)
{
  int __len = len;
  while (len > 0) {
    ssize_t ret = send(sockfd, buf, len, flags);
    if (ret < 0)
      perror_tpx(req, "send");
    buf += ret;
    len -= ret;
  }
  return __len;
}
