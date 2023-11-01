#ifndef __DEFS_H
#define __DEFS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "../common/api.h"
#include "../common/list.h"
#include "../common/utilities.h"

typedef struct __request {
  int sock;
  struct sockaddr_in addr;
  socklen_t addr_size;
  message_t msg;
} request_t;

// client.c
void* handle_createdir(void* arg);
void* handle_createfile(void* arg);
void* handle_delete(void* arg);
void* handle_info(void* arg);
void* handle_invalid(void* arg);
void* handle_list(void* arg);

// storage.c
void* stping(void* arg);
void* handle_join(void* arg);
void* handle_read(void* arg);
void* handle_write(void* arg);

#endif
