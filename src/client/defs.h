#ifndef __DEFS_H
#define __DEFS_H

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "../common/api.h"
#include "../common/utilities.h"

void request_read(void);
void request_write(void);
void request_create(void);
void request_delete(void);
void request_info(void);
void request_list(void);
void invalid_response(int32_t resp);

#endif
