#ifndef __UTILITIES_H
#define __UTILITIES_H

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void get_permissions(char *perms, mode_t mode);
void timestamp(FILE* stream);

// errno.h
void perror_t(const char* s);
void perror_tx(const char* s);

// stdio.h
void fprintf_t(FILE* stream, const char* format, ...);
FILE* fopen_tx(const char *restrict pathname, const char *restrict mode);
size_t fwrite_tx(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fread_tx(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
int fclose_tx(FILE* stream);

// socket.h
int socket_tx(int domain, int type, int protocol);
int connect_t(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void bind_tx(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void listen_tx(int sockfd, int backlog);
int accept_tx(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
ssize_t recv_tx(int sockfd, void* buf, size_t len, int flags);
ssize_t send_tx(int sockfd, void* buf, size_t len, int flags);

// arpa/inet.h
in_addr_t inet_addr_tx(const char* cp);

// unistd.h
int close_tx(int sockfd);

// pthread.h
void pthread_create_tx(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);

#endif
