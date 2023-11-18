#include "defs.h"

void log(const char* message) {
    FILE* logfile=fopen_tx("logfile.txt", "a");
    
    fprintf_t(logfile, message);
    // fprintf(logfile, "\n");

    fclose(logfile);
}