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
#include "../common/utilities.h"

#define IP "127.0.0.1"

// server.c
void nsnotify(int nsport, int clport, int stport);
void* nslisten(void* arg);
metadata_t** dirinfo(char* dir, int* bytes);
void popdirinfo(char* dir, metadata_t** data, int* idx, int level);
int countfiles(char* dir);

// client.c
void* cllisten(void* arg);

// storage.c
void* stlisten(void* arg);

#endif
