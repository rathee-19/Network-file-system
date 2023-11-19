#include "defs.h"

void* handle_createdir(void* arg)
{
  // while (1);

  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* dirpath = msg.data;

  if(dirpath[strlen(dirpath)-1]=='/') {
    dirpath[strlen(dirpath)-1]='\0';
  }

  for(int i=strlen(dirpath)-1;i>-1;i--) {
    if(dirpath[i]=='/') {
      dirpath[i]='\0';
    }
  }

  storage_t* storage_server=NULL;

  if((storage_server = cache_retrieve(&server_cache, dirpath)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t createmsg, ackmsg;
  createmsg.type=CREATE_DIR;
  bzero(ackmsg.data, BUFSIZE);

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;

    send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);

    return NULL;
  }
  else {
    createmsg.type = msg.type+1;

    dirpath[strlen(dirpath)]='/';
    snprintf(createmsg.data, "%s", dirpath);

    send_tx(storage_server->stport, &createmsg, sizeof(createmsg), 0);

    message_t response;

    recv_tx(storage_server, &response, sizeof(response), 0);

    ackmsg.type=response.type;
    snprintf(ackmsg.data, "%s", response.data);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);
  
  return NULL;
}

void* handle_createfile(void* arg)
{
  // while (1);

  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* fiilepath = msg.data;

  if(fiilepath[strlen(fiilepath)-1]=='/') {
    fiilepath[strlen(fiilepath)-1]='\0';
  }

  for(int i=strlen(fiilepath)-1;i>-1;i--) {
    if(fiilepath[i]=='/') {
      fiilepath[i]='\0';
    }
  }

  storage_t* storage_server=NULL;

  if((storage_server = cache_retrieve(&server_cache, fiilepath)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t createmsg, ackmsg;
  createmsg.type=CREATE_FILE;
  bzero(ackmsg.data, BUFSIZE);

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;

    send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);

    return NULL;
  }
  else {
    createmsg.type = msg.type+1;

    fiilepath[strlen(fiilepath)]='/';
    snprintf(createmsg.data, "%s", fiilepath);

    send_tx(storage_server->stport, &createmsg, sizeof(createmsg), 0);

    message_t response;

    recv_tx(storage_server, &response, sizeof(response), 0);

    ackmsg.type=response.type;
    snprintf(ackmsg.data, "%s", response.data);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);
  
  return NULL;
}

void* handle_delete(void* arg)
{
  // while (1);

  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* fiilepath = msg.data;

  if(fiilepath[strlen(fiilepath)-1]=='/') {
    fiilepath[strlen(fiilepath)-1]='\0';
  }

  for(int i=strlen(fiilepath)-1;i>-1;i--) {
    if(fiilepath[i]=='/') {
      fiilepath[i]='\0';
    }
  }

  storage_t* storage_server=NULL;

  if((storage_server = cache_retrieve(&server_cache, fiilepath)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t createmsg, ackmsg;
  createmsg.type=DELETE;
  bzero(ackmsg.data, BUFSIZE);

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;

    send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);

    return NULL;
  }
  else {
    createmsg.type = msg.type+1;

    fiilepath[strlen(fiilepath)]='/';
    snprintf(createmsg.data, "%s", fiilepath);

    send_tx(storage_server->stport, &createmsg, sizeof(createmsg), 0);

    message_t response;

    recv_tx(storage_server, &response, sizeof(response), 0);

    ackmsg.type=response.type;
    snprintf(ackmsg.data, "%s", response.data);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);

  return NULL;
}

void* handle_info(void* arg)
{
  // while (1);

  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* path = msg.data;

  storage_t* storage_server = NULL;

  if((storage_server = cache_retrieve(&server_cache, path)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t ackmsg;

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;
    bzero(ackmsg.data, BUFSIZE);
  }
  else {
    ackmsg.type = msg.type+1;
    snprintf(ackmsg.data, "%d", storage_server->stport);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);

  return NULL;
}

void* handle_invalid(void* arg)
{
  // while (1);

  request_t* req=(request_t*)arg;

  message_t invalidmsg;
  invalidmsg.type=INVALID;

  send_tx(req->sock, &invalidmsg, sizeof(invalidmsg), 0);

  return NULL;
}

void* handle_list(void* arg)
{
  while (1);

  return NULL;
}

void* handle_read(void* arg) {
  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* path = msg.data;

  storage_t* storage_server = NULL;

  if((storage_server = cache_retrieve(&server_cache, path)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t ackmsg;

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;
    bzero(ackmsg.data, BUFSIZE);
  }
  else {
    ackmsg.type = msg.type+1;
    snprintf(ackmsg.data, "%d", storage_server->stport);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);
}

void* handle_write(void* arg) {
  request_t* req = (request_t*)arg;

  message_t msg=req->msg;

  char* path = msg.data;

  storage_t* storage_server = NULL;

  if((storage_server = cache_retrieve(&server_cache, path)) == NULL) {
    //Search for the ss that has the data
  }

  if(search(&storage, storage_server)==0) {
    storage_server=NULL;
  }

  message_t ackmsg;

  if(storage_server == NULL) {
    ackmsg.type = NOTFOUND;
    bzero(ackmsg.data, BUFSIZE);
  }
  else {
    ackmsg.type = msg.type+1;
    snprintf(ackmsg.data, "%d", storage_server->stport);
  }

  send_tx(req->sock, &ackmsg, sizeof(ackmsg), 0);
}

void* handle_copy(void* arg) {

}
