#include "defs.h"

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  int nsport = atoi(argv[1]);         // assumption
  int clport = nsport + 1;
  int stport = nsport + 2;

  nsnotify(nsport, clport, stport);   // calls nslisten() and never returns, upon success

  return 0;
}
