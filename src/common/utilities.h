#ifndef __UTILITIES_H
#define __UTILITIES_H

#include "api.h"
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

enum caller {CLIENT, SERVER, STORAGE};
enum level {STATUS, EVENT, PROGRESS, COMPLETION, FAILURE};

// utilities.c
void timestamp(FILE* stream);
void logevent(enum caller c, enum level lvl, const char* message, ...);
request_t* reqalloc(void);
void reqfree(request_t* req);
void get_permissions(char* perms, mode_t mode);
void get_parent_dir(char* dest, char* src);
void remove_prefix(char* dest, char* src, char* prefix);
void add_prefix(char* dest, char* src, char* prefix);

// errno.c
void perror_t(const char* s);
void perror_tx(const char* s);
void perror_tpx(request_t* req, const char* s);

// inet.c
in_addr_t inet_addr_tx(const char* cp);
in_addr_t inet_addr_tpx(request_t* req, const char* cp);
const char* inet_ntop_tx(int af, const void *restrict src, char* dst, socklen_t size);
const char* inet_ntop_tpx(request_t* req, int af, const void *restrict src, char* dst, socklen_t size);

// pthread.c
int pthread_create_tx(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);
int pthread_mutex_init_tx(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_lock_tx(pthread_mutex_t* mutex);
int pthread_mutex_unlock_tx(pthread_mutex_t* mutex);

// signal.c
typedef void (*sighandler_t)(int);
sighandler_t signal_tx(int signum, sighandler_t handler);

// socket.c
int socket_tx(int domain, int type, int protocol);
int socket_tpx(request_t* req, int domain, int type, int protocol);
int setsockopt_t(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int setsockopt_tx(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int setsockopt_tpx(request_t* req, int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int connect_t(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int connect_tb(int sockfd, const struct sockaddr* addr, socklen_t addrlen, int timeout);
int bind_t(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen_tx(int sockfd, int backlog);
int listen_tpx(request_t* req, int sockfd, int backlog);
int accept_tx(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
int accept_tpx(request_t* req, int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
ssize_t recv_tx(int sockfd, void* buf, size_t len, int flags);
ssize_t recv_tpx(request_t* req, int sockfd, void* buf, size_t len, int flags);
ssize_t send_tx(int sockfd, void* buf, size_t len, int flags);
ssize_t send_tpx(request_t* req, int sockfd, void* buf, size_t len, int flags);

// stdio.c
int fprintf_t(FILE* stream, const char* format, ...);
FILE* fopen_tx(const char *restrict pathname, const char *restrict mode);
FILE* fopen_tpx(request_t* req, const char *restrict pathname, const char *restrict mode);
size_t fread_tx(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fread_tpx(request_t* req, void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fwrite_tx(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fwrite_tpx(request_t* req, const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
int fclose_tx(FILE* stream);
int fclose_tpx(request_t* req, FILE* stream);

// unistd.c
int close_tx(int sockfd);
int close_tpx(request_t* req, int sockfd);

#endif
