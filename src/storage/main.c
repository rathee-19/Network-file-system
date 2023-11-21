#include "defs.h"

logfile_t* logfile;

int main(int argc, char *argv[])
{
#ifdef LOG
  if (argc < 3) {
    fprintf(stderr, "usage: %s <port> <logfile>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  strcpy(logfile->path, argv[2]);
  pthread_mutex_init(&(logfile->lock), NULL);

#else
  if (argc < 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  logfile->path[0] = 0;
  pthread_mutex_init(&(logfile->lock), NULL);
#endif

  int nsport = atoi(argv[1]);
  int clport = nsport + 1;
  int stport = nsport + 2;

  int n;
  printf("Enter the number of accessible paths: ");
  scanf("%d", &n);

  char paths[n][PATH_MAX];
  for (int i = 0; i < n; i++) {
    printf("Path %d: ", i + 1);
    scanf(" %[^\n]s", paths[i]);
  }

  nsnotify(nsport, clport, stport, (char*) &paths, n);

  return 0;
}
