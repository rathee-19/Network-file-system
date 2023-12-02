#ifndef __DEFS_H
#define __DEFS_H

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "../common/api.h"
#include "../common/utilities.h"

#define IP "127.0.0.1"
#define logst(level, ...) logevent(STORAGE, level, __VA_ARGS__)

// client.c
void* cllisten(void* arg);
void* thread_assignment_cl(void* arg);
void* handle_read(void* arg);
void* handle_write(void* arg);
void* handle_invalid(void* arg);

// notify.c
void nsnotify(int nsport, int clport, int stport, char* paths, int n);
metadata_t** dirinfo(char* paths, int n, int* bytes);
void popdirinfo(char* cdir, metadata_t** data, int* idx);
int countfiles(char* cdir);

// server.c
void* nslisten(void* arg);
void* thread_assignment_ns(void* arg);
void* handle_copy_internal(void* arg);
void* handle_create_dir(void* arg);
void* handle_create_file(void* arg);
void* handle_delete(void* arg);
void* handle_ping(void* arg);

// storage.c
void* stlisten(void* arg);
void* thread_assignment_st(void* arg);
void* handle_backup_recv(void* arg);
void* handle_backup_send(void* arg);
void* handle_copy_recv(void* arg);
void* handle_copy_send(void* arg);
void* handle_update_recv(void* arg);
void* handle_update_send(void* arg);

#endif
