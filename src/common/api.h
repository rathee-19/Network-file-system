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
#define STORAGE_JOIN 16
#define STOP 18

#define INVALID -1
#define NOTFOUND -2
#define EXISTS -3
#define RDONLY -4                // writes disabled on ss failure
#define XLOCK -5
#define PERM -6                  // DOUBT: do we need this?

#define BUFSIZE 4096             // assumption: greater than PATH_MAX, 4096
#define RETRY 5                  // ... seconds

typedef struct __message {
  int32_t type;
  char data[BUFSIZE];
} message_t;

typedef struct __metadata {
  char path[PATH_MAX];
  char parent[PATH_MAX];
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

#endif
