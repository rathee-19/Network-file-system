#include "defs.h"

void* handle_createdir(void* arg)
{
  while (1);
  
  return NULL;
}

void* handle_createfile(void* arg)
{
  while (1);
  
  return NULL;
}

void* handle_delete(void* arg)
{
  while (1);

  return NULL;
}

void* handle_info(void* arg)
{
  // while (1);



  return NULL;
}

void* handle_invalid(void* arg)
{
  while (1);

  return NULL;
}

void* handle_list(void* arg)
{
  while (1);

  return NULL;
}

void* handle_read(void* arg) {
  request_t* req = (request_t*)arg;

  message_t msg;

  recv_tx(req->sock, &msg, sizeof(msg), 0);

  char* path = msg.data;

  storage_t* storage_server = NULL;

  if((storage_server = cache_retrieve(&server_cache, path)) == NULL) {
    //Search for the ss that has the data
  }

  message_t ackmsg;

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;
    bzero(ackmsg.data, BUFSIZE);
  }
  // Add code for checking the permissions of the file
  else {
    ackmsg.type = msg.type+1;
    snprintf(ackmsg.data, "%d", storage_server->stport);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);
}

void* handle_copy(void* arg) {

}
