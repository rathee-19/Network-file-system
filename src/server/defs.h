#ifndef __DEFS_H
#define __DEFS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "../common/api.h"
#include "../common/utilities.h"
#include "list.h"
#include "trie.h"
#include "queue.h"
#include "cache.h"

#define PING_TIMEOUT 5            // ... seconds
#define PING_SLEEP 60             // ... seconds
#define PING_TOLERANCE 3          // number of consecutive ping misses tolerated
#define REPLICATE_TOLERANCE 3     // number of consecutive replicate failures tolerated for a file
#define CRAWL_SLEEP 5             // ... seconds
#define CRAWL_COOLDOWN 50000      // ... microseconds, to avoid flooding storage servers with connections

#define logns(level, ...) logevent(SERVER, level, __VA_ARGS__)

// main.c
int main(int argc, char* argv[]);
void* thread_assignment(void* arg);

// client.c
void* handle_create_dir(void* arg);
void* handle_create_file(void* arg);
void* handle_delete(void* arg);
void* handle_info(void* arg);
void* handle_invalid(void* arg);
void* handle_list(void* arg);

// storage.c
void* stping(void* arg);
void* filecrawl(void* arg);
void* handle_join(void* arg);
void* handle_read(void* arg);
void* handle_read_completion(void* arg);
void* handle_write(void* arg);
void* handle_write_completion(void* arg);
void* handle_copy(void* arg);
void* request_delete(void* arg);
void request_delete_worker(fnode_t* node, snode_t* snode);
void* request_replicate(void* arg);
void request_update(request_t* req, snode_t* dest, snode_t* src);

#endif
