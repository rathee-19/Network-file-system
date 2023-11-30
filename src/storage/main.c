#include "defs.h"

logfile_t* logfile;

int main(int argc, char *argv[])
{
#ifdef LOG
  if (argc < 5) {
    fprintf(stderr, "usage: %s <nsport> <clport> <stport> <logfile>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  strcpy(logfile->path, argv[4]);
  pthread_mutex_init_tx(&(logfile->lock), NULL);

#else
  if (argc < 4) {
    fprintf(stderr, "usage: %s <nsport> <clport> <stport>\n", argv[0]);
    exit(1);
  }
  logfile = (logfile_t*) calloc(1, sizeof(logfile_t));
  logfile->path[0] = 0;
  pthread_mutex_init_tx(&(logfile->lock), NULL);
#endif

  signal_tx(SIGPIPE, SIG_IGN);

  int nsport = atoi(argv[1]);
  int clport = atoi(argv[2]);
  int stport = atoi(argv[3]);

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
