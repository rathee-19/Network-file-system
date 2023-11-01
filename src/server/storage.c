#include "defs.h"

void* stping(void* arg)
{
  while (1)
  {
    
  }

  return NULL;
}

void* handle_join(void* arg)
{
  request_t* req = arg;
  message_t buffer = req->msg;
  int bytes = atoi(buffer.data + 128);
  int client_sock = req->sock;

  char ip[INET_ADDRSTRLEN];
  int port = ntohs(req->addr.sin_port);
  inet_ntop(AF_INET, &(req->addr.sin_addr), ip, INET_ADDRSTRLEN);

  metadata_t** data = (metadata_t**) malloc(bytes);
  void* ptr = data;
  void* end = ptr + bytes;

  while (1)
  {
    int ret = recv_tx(client_sock, &buffer, sizeof(buffer), 0);
    
    switch (buffer.type)
    {
      case STOP:
        fprintf_t(stdout, "Received %d bytes from %s/%d\n", bytes, ip, port);
        buffer.type = STORAGE_JOIN + 1;
        send_tx(client_sock, &buffer, sizeof(buffer), 0);
        goto ret_join;

      case STORAGE_JOIN:
        if (end >= ptr + ret - sizeof(int32_t))
        {
          memcpy(ptr, buffer.data, BUFSIZE);
          ptr += ret;
          continue;
        }
        else if (end >= ptr)
        {
          memcpy(ptr, buffer.data, (void*) data + bytes - ptr + 1);
          ptr = (void*) data + bytes + 1;
          continue;
        }

      default:
#ifdef DEBUG
        fprintf_t(stdout, "Received invalid request from %s/%d\n", ip, port);
#endif
        buffer.type = INVALID;
        send_tx(client_sock, &buffer, sizeof(buffer), 0);
        goto ret_join;
    }
  }

ret_join:
  close_tx(client_sock);
  free(req);
  return NULL;
}

void* handle_read(void* arg)
{
  while (1);

  return NULL;
}

void* handle_write(void* arg)
{
  while (1);

  return NULL;
}
