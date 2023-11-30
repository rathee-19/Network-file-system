#ifndef __API_H
#define __API_H

#include <time.h>
#include <netdb.h>
#include <limits.h>
#include <sys/stat.h>

#define NSIP "127.0.0.1"
#define NSPORT 7001

#define READ 0                   // request codes
#define WRITE 2                  // +1 for response
#define CREATE_FILE 4
#define CREATE_DIR 6
#define DELETE 8
#define LIST 10
#define INFO 12
#define COPY 14
#define COPY_INTERNAL 16
#define COPY_ACROSS 18
#define BACKUP 20
#define UPDATE 22
#define JOIN 24
#define PING 26
#define STOP 28

#define INVALID -1
#define NOTFOUND -2
#define EXISTS -3
#define BEING_READ -4
#define RDONLY -5                // writes disabled on ss failure
#define XLOCK -6
#define PERM -7
#define UNAVAILABLE -8

#define BUFSIZE 4096             // assumption: greater than PATH_MAX, 4096
#define RETRY_DIFF 5             // seconds

#define COPY_COND 100            // set to indicate user defined operation
#define BACKUP_COND 101          // set to indicate naming server defined backup operation

typedef struct __message {
  int32_t type;
  char data[BUFSIZE];
} message_t;

typedef struct __request {
  int sock;
  int newsock;
  struct sockaddr_in addr;
  socklen_t addrlen;
  message_t msg;
  void* allocptr;
} request_t;

typedef struct __metadata {
  char path[PATH_MAX];
  mode_t mode;
  off_t size;
  __time_t ctime;                // TODO: struct_stat.h may refer to struct timespec instead
  __time_t mtime;
} metadata_t;

typedef struct __storage {
  char ip[INET_ADDRSTRLEN];
  int32_t nsport;
  int32_t clport;
  int32_t stport;
} storage_t;

typedef struct __logfile {
  char path[PATH_MAX];
  pthread_mutex_t lock;
} logfile_t;

#endif
